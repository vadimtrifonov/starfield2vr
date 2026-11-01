#include "GameSettingsComponent.h"
#include "CreationEngineRendererModule.h"
#include "CreationEngineSettings.h"
#include <mods/VR.hpp>

std::optional<std::string> GameSettingsComponent::on_initialize()
{
    spdlog::info("GameSettingsComponent::on_initialize");
    return Mod::on_initialize();
}

std::optional<std::string> GameSettingsComponent::on_initialize_d3d_thread()
{
    spdlog::info("GameSettingsComponent::on_initialize_d3d_thread");
    return Mod::on_initialize_d3d_thread();
}

void GameSettingsComponent::on_frame()
{

}


void GameSettingsComponent::on_device_reset()
{
    spdlog::info("GameSettingsComponent::on_device_reset");
    auto vr = VR::get();
    if(vr->get_runtime()->loaded) {
        static auto creation_engine_settings = CreationEngineSettings::Get();
        float aspect = (float)vr->get_hmd_width() / (float)vr->get_hmd_height();
        //        spdlog::info("VR Runtime is loaded {}x{} fov={}", vr->get_hmd_width(), vr->get_hmd_height(), vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fWideAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);
        creation_engine_settings->set_setting("fNarrowAspectLimit:Display", CreationEngineSettings::SettingType::kINISetting, aspect);
        creation_engine_settings->set_setting("fFPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fTPWorldFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fTPShipFOV:Camera", CreationEngineSettings::SettingType::kINIPrefSetting, vr->get_runtime()->diagonal_fov);
        creation_engine_settings->set_setting("fFPGeometryFOV:Camera", CreationEngineSettings::SettingType::kINISetting, vr->get_runtime()->diagonal_fov);
    };
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
        if(ImGui::InputInt("iSize W:Display", &iSizeW)) {
            creation_engine_settings->set_setting("iSize W:Display", CreationEngineSettings::SettingType::kINIPrefSetting, iSizeW);
        }
    }
    {
        static bool scan = false;
        if(ImGui::Checkbox("Scan Options", &scan)) {
            if(scan) {
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
