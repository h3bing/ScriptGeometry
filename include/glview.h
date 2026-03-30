/**
 * @file glview.h
 * @brief OpenGL viewport – renders GeoData from a Document
 *
 * Supports:
 *   - Perspective / orthographic projection toggle
 *   - Wireframe / solid / point render modes
 *   - Coordinate axes overlay
 *   - Standard views (front, back, top, bottom, left, right, isometric)
 *   - Orbit/pan/zoom camera control via mouse
 */

#pragma once

#include "geolib.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <string>
#include <vector>

namespace glview {

// ============================================================
//  Camera
// ============================================================

enum class ProjectionMode { Perspective, Orthographic };

struct Camera {
    glm::vec3 target   {0.f, 0.f, 0.f};
    float     distance {5.f};
    float     yaw      {45.f};    // degrees
    float     pitch    {30.f};    // degrees
    float     fovDeg   {45.f};
    float     near_    {0.01f};
    float     far_     {1000.f};
    ProjectionMode projection {ProjectionMode::Perspective};

    glm::vec3 position() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;

    void orbit(float dYaw, float dPitch);
    void pan(float dx, float dy);
    void zoom(float delta);

    void setStandardView(const std::string& name);  // "front","back","top","bottom","left","right","iso"
};

// ============================================================
//  RenderMode
// ============================================================

enum class RenderMode { Solid, Wireframe, SolidWireframe, Points };

// ============================================================
//  GlMesh  – GPU-side representation of one GeoData
// ============================================================

struct GlMesh {
    GLuint vao   {0};
    GLuint vbo   {0};
    GLuint ebo   {0};   // triangle index buffer
    GLuint lineEbo{0};  // edge index buffer
    GLsizei triCount  {0};
    GLsizei edgeCount {0};

    GlMesh()  = default;
    ~GlMesh() { destroy(); }

    GlMesh(const GlMesh&) = delete;
    GlMesh& operator=(const GlMesh&) = delete;
    GlMesh(GlMesh&&) noexcept;
    GlMesh& operator=(GlMesh&&) noexcept;

    void upload(const geo::GeoData& gd);
    void destroy();
    bool isValid() const { return vao != 0; }
};

// ============================================================
//  GlView
// ============================================================

class GlView {
public:
    GlView() = default;
    ~GlView();

    bool initialize();
    void shutdown();

    /// Upload / refresh one entity's GeoData
    void uploadEntity(uint64_t entityId, const geo::GeoData& gd);

    /// Remove an entity from the GPU buffers
    void removeEntity(uint64_t entityId);

    /// Clear all entities
    void clearAll();

    /// Render all entities into the current framebuffer
    void render(int width, int height);

    // Camera control
    Camera& camera()             { return camera_; }
    const Camera& camera() const { return camera_; }

    // Display toggles
    RenderMode renderMode     {RenderMode::Solid};
    bool       showAxes       {true};
    bool       showGrid       {false};
    glm::vec4  clearColor     {0.15f, 0.15f, 0.18f, 1.f};

    // Selection
    uint64_t   selectedEntity {0};
    void       setSelectedEntity(uint64_t id) { selectedEntity = id; }

    // Mouse events (forwarded from window)
    void onMouseButton(int button, int action, float x, float y);
    void onMouseMove(float x, float y);
    void onMouseScroll(float delta);

private:
    struct EntityBuffer {
        uint64_t entityId;
        GlMesh   mesh;
    };

    Camera                       camera_;
    std::vector<EntityBuffer>    buffers_;
    GLuint                       solidShader_    {0};
    GLuint                       wireShader_     {0};
    GLuint                       axesShader_     {0};
    GLuint                       axesVao_        {0};
    GLuint                       axesVbo_        {0};
    GLuint                       gridVao_        {0};
    GLuint                       gridVbo_        {0};
    GLsizei                      gridLineCount_  {0};

    // Mouse drag state
    bool   dragging_      {false};
    bool   panning_       {false};
    float  lastMouseX_    {0.f};
    float  lastMouseY_    {0.f};

    bool compileShaders();
    void buildAxesMesh();
    void buildGridMesh(int cells, float spacing);

    void renderMesh(const GlMesh& mesh, const glm::mat4& mvp,
                    const glm::mat4& model, bool selected);
    void renderAxes(const glm::mat4& vp);
    void renderGrid(const glm::mat4& vp);

    EntityBuffer* findBuffer(uint64_t id);
};

} // namespace glview
