cmake_minimum_required(VERSION 3.20)
project(ScriptGeometry VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
#  Output directories
# ============================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================
#  Platform / Compiler
# ============================================================

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================
#  Dependencies (via vcpkg or system packages)
# ============================================================

find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(CURL   REQUIRED)

# GLAD (bundled in third_party or via vcpkg)
find_package(glad   QUIET)
if(NOT glad_FOUND)
    # Fallback: compile bundled glad
    set(GLAD_SOURCES third_party/glad/src/glad.c)
    set(GLAD_INCLUDE third_party/glad/include)
else()
    set(GLAD_SOURCES "")
    set(GLAD_INCLUDE "")
endif()

# ImGui (bundled)
set(IMGUI_DIR third_party/imgui)
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# TCC library
find_library(TCC_LIB NAMES tcc libtcc
    HINTS "${CMAKE_SOURCE_DIR}/third_party/tcc/lib"
          "${CMAKE_SOURCE_DIR}/third_party/tcc"
)
find_path(TCC_INCLUDE NAMES libtcc.h
    HINTS "${CMAKE_SOURCE_DIR}/third_party/tcc/include"
          "${CMAKE_SOURCE_DIR}/third_party/tcc"
)

if(NOT TCC_LIB)
    message(WARNING "libtcc not found – TCC features will be disabled.")
    set(TCC_LIB "")
    set(TCC_INCLUDE "")
    add_compile_definitions(SG_NO_TCC)
endif()

# ============================================================
#  Clipper2 (polygon boolean operations)
# ============================================================

set(CLIPPER2_DIR ${CMAKE_SOURCE_DIR}/third_party/clipper2/Clipper2Lib)
set(CLIPPER2_SOURCES
    ${CLIPPER2_DIR}/src/clipper.engine.cpp
    ${CLIPPER2_DIR}/src/clipper.offset.cpp
    ${CLIPPER2_DIR}/src/clipper.rectclip.cpp
    ${CLIPPER2_DIR}/src/clipper.triangulation.cpp
)
set(CLIPPER2_INCLUDE ${CLIPPER2_DIR}/include)

# ============================================================
#  earcut.hpp (polygon triangulation, header-only)
# ============================================================

set(EARCUT_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/earcut)

# ============================================================
#  nlohmann/json (JSON serialization, header-only)
# ============================================================

set(JSON_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/json)

# ============================================================
#  Source files
# ============================================================

set(SG_SOURCES
    src/geolib.cpp
    src/capi.cpp
    src/toon.cpp
    src/scriptlib.cpp
    src/tccengine.cpp
    src/chat.cpp
    src/glview.cpp
    src/mainwindow.cpp
    src/main.cpp
    ${GLAD_SOURCES}
    ${IMGUI_SOURCES}
    ${CLIPPER2_SOURCES}
)

# ============================================================
#  Executable
# ============================================================

add_executable(ScriptGeometry ${SG_SOURCES})

target_include_directories(ScriptGeometry PRIVATE
    include
    ${GLAD_INCLUDE}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${TCC_INCLUDE}
    ${CLIPPER2_INCLUDE}
    ${EARCUT_INCLUDE}
    ${JSON_INCLUDE}
)

target_link_libraries(ScriptGeometry PRIVATE
    glfw
    glm::glm
    CURL::libcurl
    ${TCC_LIB}
    $<$<PLATFORM_ID:Windows>:opengl32>
    $<$<PLATFORM_ID:Linux>:GL>
    $<$<PLATFORM_ID:Darwin>:"-framework OpenGL">
)

# On Linux: also link dl (needed by GLAD / TCC)
if(UNIX AND NOT APPLE)
    target_link_libraries(ScriptGeometry PRIVATE dl pthread)
endif()

# ============================================================
#  Copy scripts directory to build output
# ============================================================

add_custom_command(TARGET ScriptGeometry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:ScriptGeometry>/scripts"
    COMMENT "Copying scripts..."
)

# ============================================================
#  Install
# ============================================================

install(TARGETS ScriptGeometry RUNTIME DESTINATION bin)
install(DIRECTORY scripts DESTINATION bin)
install(DIRECTORY shaders DESTINATION bin)
