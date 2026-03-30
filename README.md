# ScriptGeometry

**AI-driven procedural geometry system with in-memory C scripting and OpenGL rendering.**

---

## Features

| Feature | Detail |
|---|---|
| **AI Code Generation** | Chat with an LLM to generate C geometry scripts in TOON format |
| **In-Memory Compilation** | TCC compiles scripts at runtime — no disk I/O, sub-millisecond rebuild |
| **Attribute System** | Live-edit entity parameters; geometry rebuilds instantly |
| **Hot-Plug Scripts** | Add/remove `.toon` files while the app is running |
| **OpenGL Renderer** | Solid, wireframe, solid+wire, points; perspective/ortho; standard views |
| **Four-Party Chat** | System / TCC / AI / User conversation model |
| **C++20 / MinGW** | Windows-first, cross-platform CMake build |

---

## Quick Start

```bash
# 1. Clone
git clone <repo-url> ScriptGeometry
cd ScriptGeometry

# 2. Install dependencies  (vcpkg recommended on Windows)
#    glfw3, glm, libcurl, libtcc, imgui (bundled in third_party/)

# 3. Build
cmake -B build -G "MinGW Makefiles" \
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 4. Run
./build/bin/ScriptGeometry
```

---

## Project Structure

```
ScriptGeometry/
├── include/            # Public headers
│   ├── geolib.h        # Geometry data structures + Entity + Document
│   ├── capi.h          # C API exposed to TCC scripts
│   ├── toon.h          # TOON file format parser/serializer
│   ├── scriptlib.h     # Hot-plug script library
│   ├── tccengine.h     # TCC in-memory compiler
│   ├── chat.h          # AI chat assistant (four-party dialogue)
│   ├── glview.h        # OpenGL viewport
│   └── mainwindow.h    # ImGui layout manager
├── src/                # Implementation files
├── scripts/            # Built-in .toon scripts
│   ├── sphere.toon
│   ├── cone.toon
│   ├── box.toon
│   ├── torus.toon
│   ├── helix.toon
│   ├── star.toon
│   └── gear.toon
├── shaders/            # GLSL shaders
├── third_party/        # imgui, glad, tcc (place here)
├── docs/
│   └── architecture.md
└── CMakeLists.txt
```

---

## Writing a TOON Script

```toon
[meta]
id       = "my_shape"
name     = "My Shape"
category = "solid"
version  = "1.0"

[attrs]
radius  float  1.0  "Radius"  "Shape radius"
detail  int    32   "Detail"  "Tessellation"

[code]
void build(GeoDataHandle h) {
    /* Use any function from the C API */
    solid_sphere(h, 0.0f, 0.0f, 0.0f, 1.0f, 16, 32);
}
```

Save it in the `scripts/` folder — it loads automatically on startup and supports hot-reload.

---

## C API Reference

| Prefix | Examples |
|---|---|
| `math_` | `math_sin`, `math_cos`, `math_sqrt`, `math_clamp`, `math_lerp` |
| `curve_` | `curve_line`, `curve_arc`, `curve_circle`, `curve_bezier` |
| `loop_` | `loop_rect`, `loop_circle`, `loop_polygon`, `loop_ellipse` |
| `path_` | `path_line`, `path_arc`, `path_spline` |
| `surface_` | `surface_plane`, `surface_disk`, `surface_fill` |
| `solid_` | `solid_box`, `solid_sphere`, `solid_cylinder`, `solid_cone`, `solid_torus`, `solid_capsule` |

---

## AI Chat

1. Open the **AI** panel on the right.
2. Type a geometry request, e.g.: *"Create a twisted helix with 5 turns and radius 2"*
3. The AI returns a TOON block which is compiled and rendered immediately.
4. Edit the resulting entity's attributes in the **Document** panel for live updates.

---

## Dependencies

| Library | Purpose | License |
|---|---|---|
| [GLFW 3](https://www.glfw.org/) | Window + input | zlib |
| [GLM](https://github.com/g-truc/glm) | Math | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | GUI | MIT |
| [GLAD](https://glad.dav1d.de/) | OpenGL loader | MIT |
| [libcurl](https://curl.se/libcurl/) | HTTP for AI API | MIT-like |
| [libtcc](https://bellard.org/tcc/) | In-memory C compiler | LGPL 2.1 |

---

## License

MIT — see `LICENSE`.
