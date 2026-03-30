/**
 * @file glview.cpp
 * @brief OpenGL viewport implementation
 */

#include "glview.h"

#include <cmath>
#include <iostream>
#include <map>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace glview {

// ============================================================
//  Shader sources
// ============================================================

static const char* SOLID_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aTangent;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vNormal;
out vec3 vPos;
out vec2 vUV;

void main() {
    vNormal  = normalize(uNormalMat * aNormal);
    vPos     = vec3(uModel * vec4(aPos, 1.0));
    vUV      = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* SOLID_FS = R"(
#version 330 core
in  vec3 vNormal;
in  vec3 vPos;
in  vec2 vUV;

uniform vec4 uColor;
uniform bool uSelected;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.5));
    float diff    = max(dot(vNormal, lightDir), 0.0);
    float ambient = 0.25;
    vec3  col     = uColor.rgb * (ambient + diff * 0.75);
    if (uSelected) col = mix(col, vec3(1.0, 0.6, 0.1), 0.35);
    FragColor = vec4(col, uColor.a);
}
)";

static const char* WIRE_FS = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";

static const char* AXES_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* AXES_FS = R"(
#version 330 core
in  vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)";

// ============================================================
//  Shader helpers
// ============================================================

static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(sh, 512, nullptr, buf);
        std::cerr << "[GlView] Shader error: " << buf << "\n";
    }
    return sh;
}

static GLuint linkProgram(const char* vs_src, const char* fs_src) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vs_src);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, 512, nullptr, buf);
        std::cerr << "[GlView] Program link error: " << buf << "\n";
    }
    return prog;
}

// ============================================================
//  Camera
// ============================================================

glm::vec3 Camera::position() const {
    float yRad = glm::radians(yaw);
    float pRad = glm::radians(pitch);
    return target + glm::vec3{
        distance * std::cos(pRad) * std::cos(yRad),
        distance * std::sin(pRad),
        distance * std::cos(pRad) * std::sin(yRad)
    };
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target, {0.f, 1.f, 0.f});
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    if (projection == ProjectionMode::Perspective)
        return glm::perspective(glm::radians(fovDeg), aspect, near_, far_);
    float orthoSize = distance * 0.5f;
    return glm::ortho(-orthoSize * aspect,  orthoSize * aspect,
                      -orthoSize,            orthoSize,
                       near_, far_);
}

void Camera::orbit(float dYaw, float dPitch) {
    yaw   += dYaw;
    pitch  = glm::clamp(pitch + dPitch, -89.f, 89.f);
}

void Camera::pan(float dx, float dy) {
    glm::mat4 view = viewMatrix();
    glm::vec3 right  = {view[0][0], view[1][0], view[2][0]};
    glm::vec3 up     = {view[0][1], view[1][1], view[2][1]};
    float     scale  = distance * 0.001f;
    target -= right * dx * scale;
    target += up    * dy * scale;
}

void Camera::zoom(float delta) {
    distance = glm::clamp(distance * (1.f - delta * 0.1f), 0.01f, 5000.f);
}

void Camera::setStandardView(const std::string& name) {
    distance = 5.f;
    target   = {0.f, 0.f, 0.f};
    if      (name == "front")   { yaw =   0.f; pitch =  0.f; }
    else if (name == "back")    { yaw = 180.f; pitch =  0.f; }
    else if (name == "top")     { yaw =   0.f; pitch = 89.f; }
    else if (name == "bottom")  { yaw =   0.f; pitch =-89.f; }
    else if (name == "left")    { yaw = -90.f; pitch =  0.f; }
    else if (name == "right")   { yaw =  90.f; pitch =  0.f; }
    else if (name == "iso")     { yaw =  45.f; pitch = 30.f; }
}

// ============================================================
//  GlMesh
// ============================================================

GlMesh::GlMesh(GlMesh&& o) noexcept
    : vao(o.vao), vbo(o.vbo), ebo(o.ebo), lineEbo(o.lineEbo),
      triCount(o.triCount), edgeCount(o.edgeCount)
{
    o.vao = o.vbo = o.ebo = o.lineEbo = 0;
}

GlMesh& GlMesh::operator=(GlMesh&& o) noexcept {
    if (this != &o) {
        destroy();
        vao = o.vao; vbo = o.vbo; ebo = o.ebo; lineEbo = o.lineEbo;
        triCount = o.triCount; edgeCount = o.edgeCount;
        o.vao = o.vbo = o.ebo = o.lineEbo = 0;
    }
    return *this;
}

void GlMesh::destroy() {
    if (vao)     { glDeleteVertexArrays(1, &vao);     vao     = 0; }
    if (vbo)     { glDeleteBuffers(1, &vbo);           vbo     = 0; }
    if (ebo)     { glDeleteBuffers(1, &ebo);           ebo     = 0; }
    if (lineEbo) { glDeleteBuffers(1, &lineEbo);       lineEbo = 0; }
    triCount  = 0;
    edgeCount = 0;
}

void GlMesh::upload(const geo::GeoData& gd) {
    destroy();
    if (gd.vertices.empty()) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Interleaved layout: pos(3) tangent(3) normal(3) uv(2) = 11 floats
    struct VtxFlat { float data[11]; };
    std::vector<VtxFlat> flat;
    flat.reserve(gd.vertices.size());
    for (const auto& v : gd.vertices) {
        VtxFlat vf;
        vf.data[0] = v.pos.x;     vf.data[1] = v.pos.y;     vf.data[2] = v.pos.z;
        vf.data[3] = v.tangent.x; vf.data[4] = v.tangent.y; vf.data[5] = v.tangent.z;
        vf.data[6] = v.normal.x;  vf.data[7] = v.normal.y;  vf.data[8] = v.normal.z;
        vf.data[9] = v.uv.x;      vf.data[10]= v.uv.y;
        flat.push_back(vf);
    }
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(flat.size() * sizeof(VtxFlat)),
                 flat.data(), GL_DYNAMIC_DRAW);

    GLsizei stride = sizeof(VtxFlat);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(9*sizeof(float)));

    // Triangle indices
    if (!gd.triangles.empty()) {
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(gd.triangles.size() * sizeof(geo::TriangleIndex)),
                     gd.triangles.data(), GL_DYNAMIC_DRAW);
        triCount = static_cast<GLsizei>(gd.triangles.size() * 3);
    }

    glBindVertexArray(0);

    // Edge indices (separate VAO not needed, bind vao before draw)
    if (!gd.edges.empty()) {
        glGenBuffers(1, &lineEbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lineEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(gd.edges.size() * sizeof(geo::EdgeIndex)),
                     gd.edges.data(), GL_DYNAMIC_DRAW);
        edgeCount = static_cast<GLsizei>(gd.edges.size() * 2);
    }
}

// ============================================================
//  GlView
// ============================================================

GlView::~GlView() { shutdown(); }

bool GlView::compileShaders() {
    solidShader_ = linkProgram(SOLID_VS, SOLID_FS);
    wireShader_  = linkProgram(SOLID_VS, WIRE_FS);
    axesShader_  = linkProgram(AXES_VS,  AXES_FS);
    return solidShader_ && wireShader_ && axesShader_;
}

void GlView::buildAxesMesh() {
    // 3 axes: X=red, Y=green, Z=blue
    float axisVerts[] = {
        // pos          color
        0,0,0,  1,0,0,  1,0,0,  1,0,0,
        0,0,0,  0,1,0,  0,1,0,  0,1,0,
        0,0,0,  0,0,1,  0,0,1,  0,0,1
    };
    glGenVertexArrays(1, &axesVao_);
    glGenBuffers(1, &axesVbo_);
    glBindVertexArray(axesVao_);
    glBindBuffer(GL_ARRAY_BUFFER, axesVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVerts), axisVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

void GlView::buildGridMesh(int cells, float spacing) {
    std::vector<float> verts;
    float half = cells * spacing * 0.5f;
    for (int i = -cells/2; i <= cells/2; ++i) {
        float fi = i * spacing;
        // Horizontal
        verts.insert(verts.end(), {-half, 0.f, fi,  0.5f,0.5f,0.5f});
        verts.insert(verts.end(), { half, 0.f, fi,  0.5f,0.5f,0.5f});
        // Vertical
        verts.insert(verts.end(), {fi, 0.f, -half,  0.5f,0.5f,0.5f});
        verts.insert(verts.end(), {fi, 0.f,  half,  0.5f,0.5f,0.5f});
    }
    gridLineCount_ = static_cast<GLsizei>(verts.size() / 6);
    glGenVertexArrays(1, &gridVao_);
    glGenBuffers(1, &gridVbo_);
    glBindVertexArray(gridVao_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size()*sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

bool GlView::initialize() {
    if (!compileShaders()) return false;
    buildAxesMesh();
    buildGridMesh(20, 0.5f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.2f);
    return true;
}

void GlView::shutdown() {
    buffers_.clear();
    if (solidShader_) { glDeleteProgram(solidShader_); solidShader_ = 0; }
    if (wireShader_)  { glDeleteProgram(wireShader_);  wireShader_  = 0; }
    if (axesShader_)  { glDeleteProgram(axesShader_);  axesShader_  = 0; }
    if (axesVao_)     { glDeleteVertexArrays(1, &axesVao_); axesVao_ = 0; }
    if (axesVbo_)     { glDeleteBuffers(1, &axesVbo_);      axesVbo_ = 0; }
    if (gridVao_)     { glDeleteVertexArrays(1, &gridVao_); gridVao_ = 0; }
    if (gridVbo_)     { glDeleteBuffers(1, &gridVbo_);      gridVbo_ = 0; }
}

GlView::EntityBuffer* GlView::findBuffer(uint64_t id) {
    for (auto& b : buffers_)
        if (b.entityId == id) return &b;
    return nullptr;
}

void GlView::uploadEntity(uint64_t entityId, const geo::GeoData& gd) {
    EntityBuffer* buf = findBuffer(entityId);
    if (!buf) {
        buffers_.push_back({entityId, {}});
        buf = &buffers_.back();
    }
    buf->mesh.upload(gd);
}

void GlView::removeEntity(uint64_t entityId) {
    auto it = std::find_if(buffers_.begin(), buffers_.end(),
        [entityId](const EntityBuffer& b){ return b.entityId == entityId; });
    if (it != buffers_.end()) buffers_.erase(it);
}

void GlView::clearAll() {
    buffers_.clear();
}

void GlView::render(int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (height > 0) ? (float)width / height : 1.f;
    glm::mat4 view = camera_.viewMatrix();
    glm::mat4 proj = camera_.projectionMatrix(aspect);
    glm::mat4 vp   = proj * view;

    // Grid
    if (showGrid) renderGrid(vp);

    // Axes
    if (showAxes) renderAxes(vp);

    // Entities
    for (auto& buf : buffers_) {
        if (!buf.mesh.isValid()) continue;
        glm::mat4 model(1.f);
        glm::mat4 mvp = vp * model;
        bool sel = (buf.entityId == selectedEntity);

        if (renderMode == RenderMode::Wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            renderMesh(buf.mesh, mvp, model, sel);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else if (renderMode == RenderMode::SolidWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            renderMesh(buf.mesh, mvp, model, sel);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDepthFunc(GL_LEQUAL);
            renderMesh(buf.mesh, mvp, model, false);
            glDepthFunc(GL_LESS);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else if (renderMode == RenderMode::Points) {
            glPointSize(4.f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            renderMesh(buf.mesh, mvp, model, sel);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else {
            renderMesh(buf.mesh, mvp, model, sel);
        }
    }
}

void GlView::renderMesh(const GlMesh& mesh, const glm::mat4& mvp,
                         const glm::mat4& model, bool selected)
{
    if (!mesh.isValid()) return;

    GLuint prog = (renderMode == RenderMode::Wireframe ||
                   renderMode == RenderMode::Points)
                 ? wireShader_ : solidShader_;
    glUseProgram(prog);

    glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"),  1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"),1, GL_FALSE, glm::value_ptr(model));

    glm::mat3 nm = glm::mat3(glm::transpose(glm::inverse(model)));
    glUniformMatrix3fv(glGetUniformLocation(prog, "uNormalMat"), 1, GL_FALSE, glm::value_ptr(nm));

    if (renderMode == RenderMode::Wireframe || renderMode == RenderMode::Points)
        glUniform4f(glGetUniformLocation(prog, "uColor"), 0.8f, 0.8f, 0.9f, 1.f);
    else
        glUniform4f(glGetUniformLocation(prog, "uColor"), 0.6f, 0.75f, 0.9f, 1.f);

    glUniform1i(glGetUniformLocation(prog, "uSelected"), selected ? 1 : 0);

    glBindVertexArray(mesh.vao);

    // Triangles
    if (mesh.triCount > 0 && mesh.ebo) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glDrawElements(GL_TRIANGLES, mesh.triCount, GL_UNSIGNED_INT, 0);
    }

    // Edges
    if (mesh.edgeCount > 0 && mesh.lineEbo) {
        glUseProgram(wireShader_);
        glUniformMatrix4fv(glGetUniformLocation(wireShader_, "uMVP"),
                           1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(wireShader_, "uColor"), 0.2f, 0.2f, 0.2f, 1.f);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.lineEbo);
        glDrawElements(GL_LINES, mesh.edgeCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}

void GlView::renderAxes(const glm::mat4& vp) {
    glUseProgram(axesShader_);
    glUniformMatrix4fv(glGetUniformLocation(axesShader_, "uMVP"),
                       1, GL_FALSE, glm::value_ptr(vp));
    glLineWidth(2.f);
    glBindVertexArray(axesVao_);
    glDrawArrays(GL_LINES, 0, 6);
    glBindVertexArray(0);
    glLineWidth(1.2f);
}

void GlView::renderGrid(const glm::mat4& vp) {
    glUseProgram(axesShader_);
    glUniformMatrix4fv(glGetUniformLocation(axesShader_, "uMVP"),
                       1, GL_FALSE, glm::value_ptr(vp));
    glBindVertexArray(gridVao_);
    glDrawArrays(GL_LINES, 0, gridLineCount_);
    glBindVertexArray(0);
}

// ============================================================
//  Mouse events
// ============================================================

void GlView::onMouseButton(int button, int action, float x, float y) {
    // button 0=left, 1=right, 2=middle
    if (action == 1) {  // press
        lastMouseX_ = x;
        lastMouseY_ = y;
        if (button == 1) dragging_ = true;   // right drag = orbit
        if (button == 2) panning_  = true;   // middle drag = pan
    } else {
        dragging_ = false;
        panning_  = false;
    }
}

void GlView::onMouseMove(float x, float y) {
    float dx = x - lastMouseX_;
    float dy = y - lastMouseY_;
    lastMouseX_ = x;
    lastMouseY_ = y;

    if (dragging_) camera_.orbit(dx * 0.4f, -dy * 0.4f);
    if (panning_)  camera_.pan(dx, dy);
}

void GlView::onMouseScroll(float delta) {
    camera_.zoom(delta);
}

} // namespace glview
