# AGENTS.md

Starfield VR mod (`starfield2vr`). Builds a proxy `dxgi.dll` that hooks the game. Windows-only, C++23, CMake + Ninja.

## Submodules

The build needs only `extern/vrframework`, without its nested submodules:

```
git submodule update --init extern/vrframework
```

## Build

Run in an x64 MSVC developer shell with the tools pinned in `mise.toml`:

```
mise exec -- cmake --preset build-release-msvc
mise exec -- cmake --build build/build-release-msvc --target starfield_vr
```

- Build dir is `build/<configurePresetName>`.
- Default `HOOK_METHOD=dxgi` ⇒ output is `dxgi.dll` (a DLL proxy, not an exe).
- All presets set `USE_STARFIELD_SDK_LITE=ON`, so `extern/CommonLibSF` is NOT compiled — game types come from `sdk-lite/include`. CommonLibSF is only built when `USE_STARFIELD_SDK_LITE=OFF`.
- `SIGNATURE_SCAN=ON`: game addresses are resolved at runtime from byte patterns / ID offsets in `src/CreationEngine/memory/` (`offsets.h`, `offsets_table.h`). These are Starfield-build-version dependent.

There are no tests. "Verification" = it compiles and loads in-game.

## Layout

- `extern/vrframework/` — generic, game-agnostic VR framework (praydog REFramework-based: D3D11/D3D12 hooks, OpenXR/OpenVR, ImGui overlay, `Mod` plugin system, `g_framework`). See `extern/vrframework/CLAUDE.md` for its architecture and callback lifecycle; do not duplicate that here.
- `src/` — the Starfield-specific consumer compiled into this DLL. `src/Main.cpp` is `DllMain` → spawns a thread (note the `Sleep(5000)` startup delay) → constructs `g_framework`.
- `src/CreationEngine/` — Creation Engine integration. `CreationEngineEntry` is the `Mod` subclass that installs camera/renderer/input hooks; settings live in `GameFlow::gStore` / `ModConstants`.
- `experimental/` — NOT compiled by the root `CMakeLists.txt` (no `add_subdirectory`/glob covers it). Scratch/disabled code despite the `SFVR_EXPERIMENTAL` preset var.
- `pch/PCH.h` is the precompiled header; `imgui/imconfig.hpp` is the ImGui user config.

## Conventions

- Format with `.clang-format` (WebKit base, 180-col, 2-space no tabs, CRLF). Run `clang-format -i` on changed files.
- No-comments rule (from `extern/vrframework/CLAUDE.md`): prefer self-documenting code; only comment genuinely non-obvious logic (hardware quirks, API workarounds, hook offsets).
- Math is GLM in left-handed / DirectX convention.
