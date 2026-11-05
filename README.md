# Starfield2VR

A fork of [Starfield2VR](https://github.com/mutars/starfield2vr), based on [gsaw0's community branch](https://github.com/gsaw0/starfield2vr/tree/fix/dlss-ram-allocation).
For existing features, prerequisites, and in-game settings, see [gsaw0's README](https://github.com/gsaw0/starfield2vr/blob/f1ee35d8b202e613cc68854f1321743cba7d54d5/README.md).

## New features

- **Look up and down with your head, turn with the stick.**\
  Before, the right stick also moved your view or your weapon up and down, so they drifted whenever you turned without pushing the stick exactly sideways.
  Now only your head moves them up and down.
  This replaces the Decoupled Pitch and Head Tracking Type settings.
- **Your weapon and flashlight point where you look.**\
  Before, the flashlight turned with your head left and right but stayed level when you looked up or down.
  Now it follows your head up and down too.
  The weapon turns with your head as well, so it stays in the same place in your view.
- **Your weapon and hands are drawn in the same perspective as the world and keep their detail.**\
  Before, the game drew them with its own narrower field of view, so they looked zoomed in compared with everything around them.
  On wide headsets, it could also switch them to low-detail models, which looked blocky, as if parts were cut away.
  This replaces the Override Weapon Fov setting.
- **Aiming down sights lines the weapon up with your dominant eye.**\
  Before, the sights were centered between your eyes, so neither eye looked straight down them.
  Choose the eye with the Dominant Eye setting; the right eye is the default.
- **Iron sights and most reflex sights no longer zoom the view.**\
  Scopes, the scanner, and the few reflex sights with built-in magnification still zoom.
  This replaces the Prevent Game controlled Zooming setting.

## Fixes

- **Moving objects no longer shimmer with DLSS and TAA.**\
  The mod draws the eyes on alternate frames, so each eye's previous image is two frames old.
  Object motion is now measured over those same two frames instead of one.
- **Light glows stay in place when you turn your head.**\
  Before, the glow around lights slid and stretched as you turned, because each eye blended in the other eye's lighting from the frame before.
- **Fixes a crash when returning to the game after switching to another window or minimizing it.**\
  Before, a five-second rendering pause in the background could trigger an unnecessary recovery attempt.
- **Fixes a crash when recovering from an OpenXR error while drawing a frame.**\
  Before, the recovery discarded resources needed to finish the frame, so even a successful retry crashed the game.

## Installation

[Releases](https://github.com/vadimtrifonov/starfield2vr/releases) contain only the OpenXR build.

- With Mod Organizer 2, install the archive with Root Builder.
- Without it, copy the contents of the archive's `Root` folder into the folder that contains `Starfield.exe`.
