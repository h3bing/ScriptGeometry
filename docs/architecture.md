# ScriptGeometry Architecture

## Overview

ScriptGeometry is a procedural geometry system that combines an AI chat assistant, a TCC in-memory C compiler, and an OpenGL renderer into one cohesive workflow.

```
┌──────────────────────────────────────────────────────────────────┐
│                        Host Application                           │
│                                                                    │
│  ┌──────────┐   ┌──────────────┐  ┌────────────┐  ┌──────────┐  │
│  │ Document │   │  ScriptLib   │  │ TccEngine  │  │ GlView   │  │
│  │ (geolib) │◄──│  (.toon)     │──►│ (libtcc)   │──►│ (OpenGL) │  │
│  └──────────┘   └──────────────┘  └────────────┘  └──────────┘  │
│       ▲                ▲                                          │
│       │                │                                          │
│  ┌────┴───────────────┴──────────────────────────────────────┐   │
│  │                    C API (capi.h)                          │   │
│  │  math_  curve_  loop_  path_  surface_  solid_            │   │
│  └──────────────────────────────────────────────────────────┘   │
│                            ▲                                      │
│                      ┌─────┴──────┐                              │
│                      │    Chat    │                               │
│                      │ Assistant  │                               │
│                      │  (curl)    │                               │
│                      └────────────┘                              │
└──────────────────────────────────────────────────────────────────┘
```

## Module Descriptions

### geolib (Geometry Library)
Core data structures and entity system. All geometry data is owned here; TCC scripts never allocate memory. Key types: `Vertex`, `GeoData`, `Entity`, `Document`.

### capi (C API)
Flat C interface exposed to TCC scripts. All functions are grouped by prefix (`math_`, `curve_`, `loop_`, `path_`, `surface_`, `solid_`). No headers are needed inside scripts.

### toon (TOON Format)
Parser and serializer for the `.toon` file format (Text Object-Oriented Notation). Each file bundles metadata (`[meta]`), attribute schema (`[attrs]`), and C source (`[code]`).

### scriptlib (Script Library)
Manages a hot-pluggable collection of `ScriptMeta` objects loaded from `.toon` files. Supports directory scanning, per-file reload, and change callbacks.

### tccengine (TCC Engine)
Compiles C scripts in-memory using libtcc. Compiled scripts are cached by source hash. Scripts are sandboxed: no `#include`, no `malloc`, no system calls.

### chat (AI Chat Assistant)
Four-party conversation system (System / TCC / AI / User). Builds structured prompts from role, requirements, context, rules, functions, and examples. Communicates with any OpenAI-compatible LLM API via libcurl.

### glview (OpenGL View)
Manages per-entity GPU buffers (`GlMesh`), renders into a framebuffer, and exposes camera control. Supports solid, wireframe, solid+wireframe, and point render modes.

### mainwindow (Main Window)
ImGui layout: collapsible document panel (left), 3D viewport (center), AI chat panel (right), floating toolbar (bottom).

## Data Flow

1. **Script Loading**: `ScriptLib` reads `.toon` files → `ScriptMeta`
2. **Compilation**: `TccEngine::compile()` wraps source with C API declarations, calls TCC
3. **Execution**: `TccEngine::execute()` calls `build(GeoDataHandle)` → fills `GeoData`
4. **Upload**: `GlView::uploadEntity()` pushes `GeoData` to GPU
5. **Render**: `GlView::render()` draws each frame
6. **AI Loop**: User types → `ChatAssistant::chat()` → LLM replies with TOON block → parse → compile → execute

## TOON File Format

```
[meta]
id          = "my_script"
name        = "My Shape"
category    = "solid"
version     = "1.0"

[attrs]
radius   float  1.0   "Radius"   "Base radius"
sectors  int    32    "Sectors"  "Angular resolution"

[code]
void build(GeoDataHandle h) {
    solid_sphere(h, 0,0,0, 1.0f, 16, 32);
}
```

## Build Instructions (MinGW / Windows)

```bash
# Prerequisites: vcpkg with glfw3, glm, curl installed
# Download libtcc: https://bellard.org/tcc/
# Place libtcc.a and libtcc.h in third_party/tcc/

cmake -B build -G "MinGW Makefiles" \
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Build Instructions (Linux / macOS)

```bash
# Install: libglfw3-dev, libglm-dev, libcurl4-openssl-dev, libtcc-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/ScriptGeometry
```
