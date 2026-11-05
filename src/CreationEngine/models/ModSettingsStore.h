#pragma once
#include "glm/glm.hpp"
#include <CreationEngine/memory/ScanHelper.h>
#include <CreationEngine/memory/offsets.h>
#include <glm/detail/type_quat.hpp>
#include <glm/gtx/matrix_major_storage.hpp>

namespace GameFlow
{
    struct DebugData {
        std::vector<glm::vec3> points{};
    };

    struct Settings
    {
        struct HudSettings {
            float hudScale{0.4f};
            int perspective{150};
        } hudSettings{};
        struct InternalSettings {
            int headAimingAbsolute{1};
            float flatScreenDistance{1.5f};
            bool nvidiaAndTAAfix{true};
            bool scaleformViewportHook{true};
            bool alternativeJoyLayout{false};
            bool pawnControl{true};
        } internalSettings{};
        DebugData debugData{};
    };

    extern Settings gStore;

//    inline int gameLoopFrameCount() {
//        // 1.14.74
//        static REL::Relocation<int*> gameFc{(uintptr_t)MemoryScan::mod + 0x6101e3c};
//        return *gameFc;
//    }

    inline int renderLoopFrameCount() {
        static REL::Relocation<int*> globalFrameCountAddr{ GameStore::MemoryOffsets::CreationRenderer::GlobalFrameCount() };
//        static REL::Relocation<int*> renderFc{(uintptr_t)MemoryScan::mod + 0x6a50260};
        return *globalFrameCountAddr;
    }
}

