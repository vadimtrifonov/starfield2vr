#include "AnimationZoom.h"

#include <CreationEngine/memory/offsets.h>
#include <RE/P/PlayerCamera.h>
#include <algorithm>
#include <mods/VR.hpp>
#include <safetyhook/easy.hpp>
#include <spdlog/spdlog.h>

namespace AnimationZoom
{
    namespace
    {
        safetyhook::InlineHook g_hook;

        // The game's world FOV without the animation's share.
        float world_fov(RE::PlayerCamera* camera)
        {
            static auto vr = VR::get();
            if (!vr->is_hmd_active()) {
                return g_hook.call<float>(camera);
            }
            const auto zoom = camera->fovOffsetsEnabled ? camera->zoomFov : 0.0f;
            return std::max(camera->fov + zoom, 1.0f);
        }
    } // namespace

    void Install()
    {
        if (const auto address = GameStore::MemoryOffsets::PlayerCamera::WorldFov()) {
            g_hook = safetyhook::create_inline(reinterpret_cast<void*>(address), reinterpret_cast<void*>(&world_fov));
        }
        if (!g_hook) {
            spdlog::error("[AnimationZoom] Cannot hook the world FOV; aiming down iron and reflex sights zooms the view");
        }
    }
} // namespace AnimationZoom
