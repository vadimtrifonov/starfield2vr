#include "FirstPersonPointMapping.h"

#include <CreationEngine/memory/offsets.h>
#include <RE/N/NiPoint3.h>
#include <mods/VR.hpp>
#include <safetyhook/easy.hpp>
#include <spdlog/spdlog.h>

namespace RE
{
    class PlayerCamera;
}

namespace FirstPersonPointMapping
{
    namespace
    {
        safetyhook::InlineHook g_hook;

        bool map_point(RE::PlayerCamera* camera, const RE::NiPoint3* point, RE::NiPoint3* mapped)
        {
            static auto vr = VR::get();
            if (vr->is_hmd_active()) {
                return false;
            }
            return g_hook.call<bool>(camera, point, mapped);
        }
    } // namespace

    void Install()
    {
        if (const auto address = GameStore::MemoryOffsets::PlayerCamera::MapFirstPersonPoint()) {
            g_hook = safetyhook::create_inline(reinterpret_cast<void*>(address), reinterpret_cast<void*>(&map_point));
        }
        if (!g_hook) {
            spdlog::error("[FirstPersonPointMapping] Cannot hook the first-person point mapping; beams start away from the drawn muzzle");
        }
    }
} // namespace FirstPersonPointMapping
