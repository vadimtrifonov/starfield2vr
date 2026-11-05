#pragma once
#include <Mod.hpp>

class GameSettingsComponent : public Mod
{
public:
    inline static std::shared_ptr<GameSettingsComponent>& Get()
    {
        static std::shared_ptr<GameSettingsComponent> instance{ std::make_shared<GameSettingsComponent>() };
        return instance;
    }

    [[nodiscard]] inline std::string_view get_name() const override { return "StarFieldSettings"; }

    void on_draw_ui() override;
    void on_device_reset() override;
};
