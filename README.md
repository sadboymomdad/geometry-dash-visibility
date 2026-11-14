# Gameplay Visibility

A macOS-only Geode mod for Geometry Dash 2.2081. It adds one slider in the mod's Geode settings page.

- **0%** leaves the original image unchanged.
- Moving the slider raises gamma and brightness together.
- There is no contrast setting.

## Build

The GitHub Actions workflow builds a macOS `.geode` package and uploads it as an artifact.

## Important testing note

This first implementation uses a GLSL 1.20 post-process pass over the rendered scene. It needs to be checked on the actual macOS Geometry Dash 2.2081 renderer. In particular, the game must expose a compatible OpenGL context and the root-scene framebuffer must be readable with `glCopyTexSubImage2D`. A successful compile alone does not prove the in-game effect works.

The slider is intentionally set to 0 by default so the game looks unchanged until the user enables it.
