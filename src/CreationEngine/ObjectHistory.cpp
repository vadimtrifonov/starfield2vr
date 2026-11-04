#include "ObjectHistory.h"

#include <CreationEngine/memory/offsets.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <_deps/directxtk12-src/Src/d3dx12.h>
#include <mods/VR.hpp>
#include <safetyhook/easy.hpp>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

#include "shaders/object_history_cs.h"

using Microsoft::WRL::ComPtr;

namespace ObjectHistory
{
    namespace
    {
        // GatherGeometryTransforms's root signature container as shipped in shaderspc_dx12 (Starfield 1.16.244).
        // Parameters: b0 update count; table {t0, t1, u0 GeometryBlocks} in space4; table {t0 updates} in space5.
        constexpr uint8_t kGatherRootSignature[] = {
            0x44, 0x58, 0x42, 0x43, 0x85, 0x32, 0x41, 0x15, 0x4f, 0x29, 0xb1, 0x45, 0x4a, 0xa1, 0xa2, 0xbc, 0xec, 0xf8, 0x9e, 0x37, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00,
            0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x52, 0x54, 0x53, 0x30, 0xb8, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
            0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        };
        constexpr UINT kCountParameter = 0, kBlocksTable = 1, kUpdatesTable = 2, kStampsParameter = 3, kSlotsParameter = 4, kFrameParameter = 5;

        constexpr UINT kBlockCapacity = 0x10000000 / 96; // the game's GeometryBlock view
        constexpr UINT kSlotCapacity  = 1 << 17;         // updates per frame; a cockpit in flight gathers about 23,000
        constexpr UINT kTransformSize = 32;

        // A gather batch as the engine bound it for its Dispatch.
        struct Batch
        {
            ID3D12GraphicsCommandList*  list;
            ID3D12RootSignature*        root_signature;
            ID3D12PipelineState*        pipeline;
            D3D12_GPU_DESCRIPTOR_HANDLE blocks, updates;
            UINT                        count;
        };

        safetyhook::MidHook g_hook;

        struct Pass
        {
            std::mutex                  mutex;
            bool                        failed{ }, unrecognized{ }, retiming{ };
            ComPtr<ID3D12RootSignature> gather, root_signature;
            ComPtr<ID3D12PipelineState> pipeline;
            ComPtr<ID3D12Resource>      stamps, slots;
            int                         frame{ };
            UINT                        used{ }; // update slots taken by this frame's batches
        } g_pass;

        bool create_buffer(ID3D12Device* device, UINT64 size, ComPtr<ID3D12Resource>& resource)
        {
            const CD3DX12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
            const auto                    desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            return SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)));
        }

        // The pass binds the game's tables unchanged through the gather's root signature extended by stamps, slots and frame constants.
        bool create_pass(ID3D12Device* device)
        {
            // D3D12 hands back the existing root signature for an identical blob (observed, not documented); that is how the game's gather
            // is recognized, and a runtime that stops doing so leaves the pass off.
            if (FAILED(device->CreateRootSignature(0, kGatherRootSignature, sizeof(kGatherRootSignature), IID_PPV_ARGS(&g_pass.gather)))) {
                return false;
            }
            ComPtr<ID3D12VersionedRootSignatureDeserializer> deserializer;
            const D3D12_VERSIONED_ROOT_SIGNATURE_DESC*       gather{ };
            if (FAILED(D3D12CreateVersionedRootSignatureDeserializer(kGatherRootSignature, sizeof(kGatherRootSignature), IID_PPV_ARGS(&deserializer)))
                || FAILED(deserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &gather)) || gather->Desc_1_1.NumParameters != kStampsParameter)
            {
                return false;
            }
            std::vector<CD3DX12_ROOT_PARAMETER1> parameters(gather->Desc_1_1.pParameters, gather->Desc_1_1.pParameters + gather->Desc_1_1.NumParameters);
            parameters.resize(kFrameParameter + 1);
            parameters[kStampsParameter].InitAsUnorderedAccessView(0, 6);
            parameters[kSlotsParameter].InitAsUnorderedAccessView(1, 6);
            parameters[kFrameParameter].InitAsConstants(4, 1);
            D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
            desc.Desc_1_1               = gather->Desc_1_1;
            desc.Desc_1_1.NumParameters = UINT(parameters.size());
            desc.Desc_1_1.pParameters   = parameters.data();
            ComPtr<ID3DBlob> blob, error;
            if (FAILED(D3D12SerializeVersionedRootSignature(&desc, &blob, &error))
                || FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&g_pass.root_signature))))
            {
                return false;
            }
            const D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{
                g_pass.root_signature.Get(), { g_object_history_cs, sizeof(g_object_history_cs) }
            };
            // Committed resources start zeroed, so no block has a slot yet.
            return SUCCEEDED(device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&g_pass.pipeline))) && create_buffer(device, UINT64(kBlockCapacity) * 8, g_pass.stamps)
                   && create_buffer(device, UINT64(kSlotCapacity) * 2 * kTransformSize, g_pass.slots);
        }

        // Call with g_pass.mutex held.
        bool ready(ID3D12GraphicsCommandList* list)
        {
            if (g_pass.pipeline || g_pass.failed) {
                return !g_pass.failed;
            }
            ComPtr<ID3D12Device> device;
            g_pass.failed = FAILED(list->GetDevice(IID_PPV_ARGS(&device))) || !create_pass(device.Get());
            if (g_pass.failed) {
                spdlog::error("[ObjectHistory] Cannot create the retiming pass; moving objects keep the game's one-frame history");
            }
            return !g_pass.failed;
        }

        void bind(const Batch& batch, ID3D12RootSignature* root_signature, ID3D12PipelineState* pipeline)
        {
            batch.list->SetComputeRootSignature(root_signature);
            batch.list->SetPipelineState(pipeline);
            batch.list->SetComputeRootDescriptorTable(kBlocksTable, batch.blocks);
            batch.list->SetComputeRootDescriptorTable(kUpdatesTable, batch.updates);
            batch.list->SetComputeRoot32BitConstant(kCountParameter, batch.count, 0);
        }

        // Records after the batch's Dispatch on the same list and leaves the game's compute state as it was. No barrier follows: whatever the
        // engine records after its gather to order the blocks' readers also orders this pass, and the next pass's barrier orders stamps and slots.
        void retime(const Batch& batch, int frame, UINT base)
        {
            D3D12_RESOURCE_BARRIER barrier{ D3D12_RESOURCE_BARRIER_TYPE_UAV };
            batch.list->ResourceBarrier(1, &barrier);
            bind(batch, g_pass.root_signature.Get(), g_pass.pipeline.Get());
            batch.list->SetComputeRootUnorderedAccessView(kStampsParameter, g_pass.stamps->GetGPUVirtualAddress());
            batch.list->SetComputeRootUnorderedAccessView(kSlotsParameter, g_pass.slots->GetGPUVirtualAddress());
            const UINT constants[] = { UINT(frame), base, kBlockCapacity, kSlotCapacity };
            batch.list->SetComputeRoot32BitConstants(kFrameParameter, UINT(std::size(constants)), constants, 0);
            batch.list->Dispatch((batch.count + 63) / 64, 1, 1);
            bind(batch, batch.root_signature, batch.pipeline);
        }

        uintptr_t at(uintptr_t address)
        {
            return *reinterpret_cast<const uintptr_t*>(address);
        }

        // Hook site registers (Starfield 1.16.244): rbx is the command context, with the command list at +0x60; r15 is the bound shader
        // state, with the root signature at [[+0]+0x60], the pipeline at [[+8]+0x88], and the GPU handles of the blocks and updates tables
        // at [[+0x20]+0x28] and [[+0x28]+0x28]; ebp is the batch's update count.
        void after_gather_batch(safetyhook::Context& context)
        {
            if (!VR::get()->is_hmd_active() || !GameFlow::gStore.internalSettings.nvidiaAndTAAfix) {
                return;
            }
            const auto state = context.r15;
            Batch      batch{ reinterpret_cast<ID3D12GraphicsCommandList*>(at(context.rbx + 0x60)), reinterpret_cast<ID3D12RootSignature*>(at(at(state) + 0x60)) };
            batch.count = UINT(context.rbp);
            int  frame;
            UINT base;
            {
                std::scoped_lock lock{ g_pass.mutex };
                if (!ready(batch.list)) {
                    return;
                }
                if (batch.root_signature != g_pass.gather.Get()) {
                    if (!std::exchange(g_pass.unrecognized, true)) {
                        spdlog::warn("[ObjectHistory] The transform gather binds an unexpected root signature; moving objects keep the game's one-frame history");
                    }
                    return;
                }
                if (!std::exchange(g_pass.retiming, true)) {
                    spdlog::info("[ObjectHistory] Retiming moved objects after the game's transform gather");
                }
                // Read under the lock so the frame never steps back: a batch recorded late joins the newer frame and keeps native history.
                frame = GameFlow::renderLoopFrameCount();
                if (g_pass.frame != frame) {
                    g_pass.frame = frame;
                    g_pass.used  = 0;
                }
                base        = g_pass.used;
                g_pass.used = std::min(g_pass.used + batch.count, kSlotCapacity);
            }
            batch.pipeline = reinterpret_cast<ID3D12PipelineState*>(at(at(state + 8) + 0x88));
            batch.blocks   = { at(at(state + 0x20) + 0x28) };
            batch.updates  = { at(at(state + 0x28) + 0x28) };
            retime(batch, frame, base);
        }
    } // namespace

    void Install()
    {
        if (const auto address = GameStore::MemoryOffsets::CreationRenderer::GatherTransformsBatchDispatched()) {
            g_hook = safetyhook::create_mid(reinterpret_cast<void*>(address), after_gather_batch);
        }
        if (!g_hook) {
            spdlog::error("[ObjectHistory] Cannot hook the game's transform gather; moving objects keep the game's one-frame history");
        }
    }
} // namespace ObjectHistory
