#pragma once
#include <Mod.hpp>

class CreationEngineEntry : public Mod
{
public:
    inline static std::shared_ptr<CreationEngineEntry>& Get()
    {
        static std::shared_ptr<CreationEngineEntry> instance{ std::make_shared<CreationEngineEntry>() };
        return instance;
    }

    [[nodiscard]] inline std::string_view get_name() const override { return "CreationEngine"; }

    std::optional<std::string> on_initialize() override;
    void                       on_draw_ui() override;
    void                       on_config_load(const utility::Config& cfg, bool ) override;
    void                       on_config_save(utility::Config& cfg) override;

private:
    inline static const std::vector<std::string> s_dominant_eye{
        "Right",
        "Left",
    };

    const ModCombo::Ptr m_dominant_eye{ ModCombo::create(generate_name("DominantEye"), s_dominant_eye) };
    const ModToggle::Ptr m_pawn_control_rotation{ ModToggle::create(generate_name("PawnControlRotation"), true) };
    const ModToggle::Ptr m_taa_anf_nvidia_fix{ ModToggle::create(generate_name("NvidiaDlssAndTaaFix"), true) };
    const ModToggle::Ptr m_scaleform_viewport_hook{ ModToggle::create(generate_name("ScaleformViewportHook"), true) };
    const ModSlider::Ptr m_hud_scale{ModSlider::create(generate_name("HUDScale"), 0.1, 1.0, 0.4) };
    const ModSlider::Ptr m_hud_perspective{ ModSlider::create(generate_name("HUDPerspective"), 0, 600, 150) };
    const ModToggle::Ptr m_alternative_joy_layout{ ModToggle::create(generate_name("JoyAlternativeLayout"), false) };

    ValueList m_options{
        *m_dominant_eye, *m_taa_anf_nvidia_fix, *m_scaleform_viewport_hook, *m_hud_scale, *m_hud_perspective, *m_alternative_joy_layout, *m_pawn_control_rotation
    };
};
