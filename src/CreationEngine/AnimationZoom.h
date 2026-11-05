#pragma once

// Weapon animations narrow the world FOV for iron and reflex sights; explicit zoom data controls scopes and the scanner.
// In VR, remove the animation contribution at the effective world-FOV getter shared by camera setup, crosshair spread and aim assist.
namespace AnimationZoom
{
    void Install();
} // namespace AnimationZoom
