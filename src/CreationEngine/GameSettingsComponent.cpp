#include "GameSettingsComponent.h"
#include "CreationEngineSettings.h"
#include <mods/VR.hpp>

#include <cmath>

namespace
{
    // Starfield's camera FOV settings are horizontal angles at 16:9; the game converts them to the viewport aspect, keeping the vertical angle.
    float to_game_fov(float tan_half_horizontal, float aspect)
    {
        return glm::degrees(2.0f * std::atan(tan_half_horizontal * 16.0f / (9.0f * aspect)));
    }
} // namespace

void GameSettingsComponent::on_device_reset()
{
    auto       vr      = VR::get();
    const auto runtime = vr->get_runtime();
    const auto frustum = [&] {
        std::shared_lock _{ runtime->eyes_mtx };
        return runtime->frustums[0];
    }();
    const auto tan_half_horizontal = (frustum[1] - frustum[0]) / 2.0f;
    // The projection stays zero from loading the runtime until its first view update.
    if (!runtime->loaded || tan_half_horizontal <= 0.0f) {
        return;
    }
    static auto settings = CreationEngineSettings::Get();
    // The game letterboxes its view to these limits, and converts the FOV settings below with the view's aspect.
    const auto aspect = (float)vr->get_hmd_width() / (float)vr->get_hmd_height();
    settings->set_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);
    settings->set_setting("fNarrowAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);

    // The world and first-person views are drawn with the framework's symmetric headset projection, the same for both eyes.
    // These settings give game code that works with an FOV number, such as crosshair spread, aim assist and zoom, its horizontal angle.
    // The game derives the vertical angle from the viewport's aspect, which can differ from the projection's.
    // The first-person setting shares the world's base FOV, but Starfield still derives a separate first-person lens that VR does not draw with.
    const auto fov = to_game_fov(tan_half_horizontal, aspect);
    settings->set_setting("fFPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, fov);
    settings->set_setting("fTPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, fov);
    settings->set_setting("fTPShipFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, fov);
    settings->set_setting("fFPGeometryFOV:Camera", CreationEngineSettings::SettingType::kINISetting, fov);
    spdlog::info("[FOV] Projection horizontal FOV {:.2f}, viewport aspect {:.4f}, game FOV setting {:.2f}", glm::degrees(2.0f * std::atan(tan_half_horizontal)), aspect, fov);
}

void GameSettingsComponent::on_draw_ui()
{
#ifdef _DEBUG
    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }
    ImGui::Separator();
    auto creation_engine_settings = CreationEngineSettings::Get();
    {
        //"fNarrowAspectLimit:Display"
        static float wideAspectLimit = creation_engine_settings->get_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, 1.5f);
        if (ImGui::SliderFloat("fWideAspectLimit:Display", &wideAspectLimit, 0.8f, 1.5f)) {
            creation_engine_settings->set_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, wideAspectLimit);
        }
    }
    {
        static int iSizeW = creation_engine_settings->get_setting("iSize W:Display", CreationEngineSettings::SettingType::kINIPrefSetting, 1920);
        if (ImGui::InputInt("iSize W:Display", &iSizeW)) {
            creation_engine_settings->set_setting("iSize W:Display", CreationEngineSettings::SettingType::kINIPrefSetting, iSizeW);
        }
    }
    {
        static bool scan = false;
        if (ImGui::Checkbox("Scan Options", &scan)) {
            if (scan) {
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kGameSetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kRendererQualitySetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kRendererPerfSetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kINISetting);
                CreationEngineSettings::get_setting("test", CreationEngineSettings::SettingType::kINIPrefSetting);
            }
        }
    }
#endif
}
