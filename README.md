# Starfield2VR

A fork of [Starfield2VR](https://github.com/mutars/starfield2vr) that adds fixes on top of [gsaw0's community branch](https://github.com/gsaw0/starfield2vr/tree/fix/dlss-ram-allocation).
For features, prerequisites, and in-game settings, see the [original README](https://github.com/gsaw0/starfield2vr/blob/f1ee35d8b202e613cc68854f1321743cba7d54d5/README.md).

## Changes from upstream

- The weapon and flashlight point where you look, and the weapon stays in the same spot in your view as you look around.
- Look up and down with your head; the right stick only turns left and right.
  This replaces the Decoupled Pitch and Aiming Mode settings.
- When aiming down sights, the weapon is held in front of your dominant eye and moves with your head.
- The weapon and hands are drawn with the headset's field of view, like the rest of the world.
- Fixes DLSS ghosting during movement, particularly in ship cockpits.
- Fixes crashes when returning to the game after switching to another window or minimizing it.
- Fixes a possible crash when the VR runtime reports an error while drawing a frame.

## Installation

Releases contain only the OpenXR build.
Install the release archive in MO2 with Root Builder.
Without MO2, copy the contents of its `Root` folder into the Starfield folder.
