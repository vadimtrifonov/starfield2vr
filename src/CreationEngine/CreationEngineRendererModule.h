#pragma once
#include <RE/C/CreationRendererPrivate.h>
#include <_deps/directxtk12-src/Src/PlatformHelpers.h>
#include <_deps/directxtk12-src/Src/d3dx12.h>
#include <safetyhook/inline_hook.hpp>
#include <memory/FunctionHook.h>
#include <windef.h>

#include "math/Math.hpp"
using Microsoft::WRL::ComPtr;

namespace RE
{
    struct WindowRect
    {
        int x;
        int y;
        int cx;
        int cy;
    };

    struct HWindowHolder
    {
        int     windowX;
        int     windowY;
        int     windowCX;
        int     windowCY;
        uint8_t pad[8];
        LPCSTR  windowName;
        HWND    parentWindowHandle;
        uint8_t pad20[128];
        HWND    windowHandle;
        uint8_t trailing[120];
    };

    struct DisplayGameSettings
    {
        WindowRect windowRect;
        WindowRect displayRect;
        uint32_t   flags;
    };

    static_assert(offsetof(HWindowHolder, windowHandle) == 168);
    static_assert(sizeof(HWindowHolder) == 0x128);

    struct CreationEngineSettings
    {
        uint64_t            unk0;
        uint64_t            unk1;
        DisplayGameSettings displayGameSettings;
        uint32_t            height;
        uint32_t            width;
        uint32_t            letterboxedHeight;
        uint32_t            letterboxedWidth;
        uint8_t             pad44[4];
        HWindowHolder*      pHwindow;
        uint8_t             pad50;
        bool                forcedAspect;
        uint8_t             pad52[22];
        uint64_t            features;
        uint8_t             trainling[1024];
    };
    static_assert(offsetof(CreationEngineSettings, pHwindow) == 0x48);
    static_assert(offsetof(CreationEngineSettings, displayGameSettings) == 0x10);
    static_assert(offsetof(CreationEngineSettings, height) == 0x34);
    static_assert(offsetof(CreationEngineSettings, letterboxedWidth) == 0x40);
    static_assert(offsetof(CreationEngineSettings, forcedAspect) == 0x51);
    static_assert(offsetof(CreationEngineSettings, features) == 0x68);

    struct DlssConstants
    {
        uint8_t    pad[0x50];
        Matrix4x4f projectionMatrix;
        uint8_t    pad1[0xC0];
        Matrix4x4f previousProjectionMatrix;
    };

    static_assert(offsetof(DlssConstants, projectionMatrix) == 0x50);
    static_assert(offsetof(DlssConstants, previousProjectionMatrix) == 0x150);

    // aka CameraBlocks
    struct RenderPassConstantBufferView
    {
        Vector2f   unk0;
        int        unk8;
        char       pad_000C[68];
        Matrix4x4f unk50;
        char       pad_0090[64];
        Matrix4x4f unk0D0;
        Vector4f   unk110;
        Vector4f   unk120;
        Vector4f   unk130;
        char       pad_0140[16];
        Matrix4x4f unk150;
        Vector2f   unk190;
        int        unk198;
        char       pad_019C[4];
        Vector4f   unk1A0;
        Vector4f   unk1B0;
        Vector4f   unk1C0;
        Vector4f   unk1D0;
        Matrix4x4f unk1E0;
        char       pad220[68];
        float      horizontalHalfFov;
        float      verticalHalfFov;
        float      unk26C;
        float      cameraNear;
        float      cameraFar;
        char       trail[56];
    };

    static_assert(sizeof(RenderPassConstantBufferView) == 0x2B0);
    static_assert(offsetof(RenderPassConstantBufferView, unk50) == 0x50);
    static_assert(offsetof(RenderPassConstantBufferView, unk0D0) == 0xD0);

    struct MotionVectorRenderPassConstants
    {
        Matrix4x4f currentProjectionMatrix;
        Matrix4x4f previousProjectionMatrix;
        Vector4f   currentCameraPosition;
        Vector4f   previousCameraPosition;
    };

    struct D3D12Context;
    struct D3DContextCommnadAllocatorData;
    struct D3DContextBindData;
#pragma pack(push, 1)

    struct RenderGraphDataD3D12Context
    {
        char pad[0x60];
        ID3D12GraphicsCommandList*      pID3D12CommandList;
        D3DContextBindData*             pD3DContextBindData;
        D3DContextBindData*             pD3DContextBindDataDefault;
        uint8_t                         gap_28[16];
        uint64_t                        field_38;
        uint8_t                         raw_data[4096];
    };

#pragma pack(pop)
    static_assert(offsetof(RenderGraphDataD3D12Context, pID3D12CommandList) == 0x60);
} // namespace RE

class CreationEngineRendererModule
{
public:
    void InstallHooks();

    inline static CreationEngineRendererModule* Get()
    {
        static auto instance(new CreationEngineRendererModule);
        return instance;
    }

    [[nodiscard]] inline RE::CreationEngineSettings* GetCreationEngineSettings() const { return *m_creationEngineSettings; }

    void SwapBuffer(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* originalBuffer, int index, int fc, bool cameraCut);
    void SwapVolumetricHistory(ID3D12GraphicsCommandList* cmdList, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, int fc);

    inline static void CopyResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* pSrcResource, ID3D12Resource* pDstResource, D3D12_RESOURCE_STATES srcState,
                                    D3D12_RESOURCE_STATES dstState)
    {
        D3D12_RESOURCE_BARRIER barriers[2] = { CD3DX12_RESOURCE_BARRIER::Transition(pSrcResource, srcState, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                               CD3DX12_RESOURCE_BARRIER::Transition(pDstResource, dstState, D3D12_RESOURCE_STATE_COPY_DEST) };
        cmdList->ResourceBarrier(2, barriers);
        cmdList->CopyResource(pDstResource, pSrcResource);
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter  = srcState;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter  = dstState;
        cmdList->ResourceBarrier(2, barriers);
    }

    __int64   onRenderGraphRenderStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, __int64 i1, __int64 i2);
//    __int64   onRenderFrameStart(void* pVoid, __int64 i, __int64 i1, __int64 i2);
    void      SetWindowSize(int width, int height);
    uintptr_t onUpdateConstantBufferView(uint8_t i, uint8_t i1, uintptr_t i2, uintptr_t i3, uintptr_t i4, double d, char i5, RE::RenderPassConstantBufferView* pView);

private:
    CreationEngineRendererModule()  = default;
    ~CreationEngineRendererModule() = default;

    std::unique_ptr<FunctionHook> m_onRenderGraphRenderStartHook{};
    std::unique_ptr<FunctionHook> m_onRenderFrameStartHook{};
    std::unique_ptr<FunctionHook> m_onUpdateConstantBufferViewHook{};
    std::unique_ptr<FunctionHook> taa_vfunc7_hook{};
    safetyhook::InlineHook m_worldTick_hook{};
    safetyhook::InlineHook m_setReflexMarkerInternalHook{};


    RE::CreationRendererPrivate::RenderPass* m_startFramePass{ nullptr };
    RE::CreationRendererPrivate::RenderPass* m_endFramePass{ nullptr };
    RE::CreationRendererPrivate::RenderPass* m_framePass{ nullptr };

    RE::CreationEngineSettings**          m_creationEngineSettings{ nullptr };
    std::mutex                            m_last_resolution_mutex{};
    std::chrono::steady_clock::time_point m_last_resolution_sync{};

    // SwapBuffer only ever addresses slots (fc-1)&1 and fc&1, so two history slots per target suffice
    ComPtr<ID3D12Resource> m_pastBuffer[12][2];
    // Render frame last saved into each target's backups; -1 after (re)allocation
    int m_pastBufferFrame[12]{};

    static uintptr_t onTaaPass(RE::CreationRendererPrivate::RenderPass* pPass, RE::CreationRendererPrivate::RenderGraphData* i, RE::CreationRendererPrivate::RenderPassData* i1);
    void             RenderGraphStart(RE::CreationRendererPrivate::RenderGraph* pGraph, RE::CreationRendererPrivate::RenderGraphData* pRenderGraphData, bool before);
    bool             ValidateResource(ID3D12Resource* source, int index);
    bool             CanRestoreHistory(int index, int fc, bool cameraCut) const;
    void             SaveHistory(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* source, int index, int fc, bool restore);
    void             logMemoryUsage(const char* reason);
    static uintptr_t setReflexMarkerInternal(uintptr_t rcx, uint32_t marker, uint32_t oldFrameIndex);
};
