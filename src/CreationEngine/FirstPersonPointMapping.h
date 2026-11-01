#pragma once

// Keeps beams and weapon aim effects at the drawn muzzle.
// Starfield moves first-person points to where its own first-person lens would show them; in VR the weapon is drawn with the headset's lens.
namespace FirstPersonPointMapping
{
    void Install();
} // namespace FirstPersonPointMapping
