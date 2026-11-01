#include "FirstPersonLod.h"

#include <CreationEngine/memory/offsets.h>
#include <mods/VR.hpp>
#include <safetyhook/easy.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace FirstPersonLod
{
    namespace
    {
        // Starfield picks mesh LODs by projected size, using a per-view scale of half the larger of the projection's P00 and P11.
        // A wide headset projection lowers the first-person scale, to 0.5 for a 90-by-90-degree view, and the weapon and hands can switch to coarse LODs.
        // In that view, Starfield's own first-person lens gives about 1.9, from fFPGeometryFOV's 85-degree default and a -20-degree first-person offset.
        constexpr float kMinimumScale = 2.0f;

        // The records are 0x30 bytes each, for the world, first-person and orthographic view; the scale is at +0x1C (Starfield 1.16.244).
        constexpr uintptr_t kFirstPersonScaleOffset = 0x30 + 0x1C;

        safetyhook::MidHook g_hook;

        void before_submit(safetyhook::Context& context)
        {
            static auto vr = VR::get();
            if (vr->is_hmd_active()) {
                auto& scale = *reinterpret_cast<float*>(context.rdx + kFirstPersonScaleOffset);
                scale       = std::max(scale, kMinimumScale);
            }
        }
    } // namespace

    void Install()
    {
        if (const auto address = GameStore::MemoryOffsets::CreationRenderer::SubmitLodViewsCall()) {
            g_hook = safetyhook::create_mid(reinterpret_cast<void*>(address), before_submit);
        }
        if (!g_hook) {
            spdlog::error("[FirstPersonLod] Cannot hook the view LOD setup; the weapon and hands keep the headset projection's coarser LODs");
        }
    }
} // namespace FirstPersonLod
