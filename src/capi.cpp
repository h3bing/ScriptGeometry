/**
 * @file capi.cpp
 * @brief C API implementation for TCC scripts
 */

#include "capi.h"

#include <cmath>
#include <vector>
#include <cstring>

// ============================================================
//  Constants
// ============================================================

const float CAPI_PI = geo::PI;
const float CAPI_E  = geo::E;

// ============================================================
//  Helper: cast GeoDataHandle to geo::GeoData*
// ============================================================

static inline geo::GeoData* toGD(GeoDataHandle h) {
    return reinterpret_cast<geo::GeoData*>(h);
}

static inline uint32_t addVertex(geo::GeoData* gd,
                                  const geo::Point3& pos,
                                  const geo::Vector3& normal = {0.f, 0.f, 1.f},
                                  const geo::Vector2& uv = {0.f, 0.f}) {
    geo::Vertex v;
    v.pos    = pos;
    v.normal = normal;
    v.uv     = uv;
    gd->vertices.push_back(v);
    return static_cast<uint32_t>(gd->vertices.size() - 1);
}

// ============================================================
//  math_ functions
// ============================================================

float math_sin(float x)             { return std::sin(x); }
float math_cos(float x)             { return std::cos(x); }
float math_tan(float x)             { return std::tan(x); }
float math_asin(float x)            { return std::asin(x); }
float math_acos(float x)            { return std::acos(x); }
float math_atan(float x)            { return std::atan(x); }
float math_atan2(float y, float x)  { return std::atan2(y, x); }
float math_sqrt(float x)            { return std::sqrt(x); }
float math_pow(float b, float e)    { return std::pow(b, e); }
float math_exp(float x)             { return std::exp(x); }
float math_log(float x)             { return std::log(x); }
float math_log2(float x)            { return std::log2(x); }
float math_log10(float x)           { return std::log10(x); }
float math_abs(float x)             { return std::fabs(x); }
float math_floor(float x)           { return std::floor(x); }
float math_ceil(float x)            { return std::ceil(x); }
float math_round(float x)           { return std::round(x); }
float math_fmod(float x, float y)   { return std::fmod(x, y); }

float math_clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
float math_lerp(float a, float b, float t) { return a + (b - a) * t; }
float math_min2(float a, float b)          { return geo::min2(a, b); }
float math_max2(float a, float b)          { return geo::max2(a, b); }
float math_min3(float a, float b, float c) { return geo::min3(a, b, c); }
float math_max3(float a, float b, float c) { return geo::max3(a, b, c); }
float math_deg2rad(float d)                { return d * (geo::PI / 180.f); }
float math_rad2deg(float r)                { return r * (180.f / geo::PI); }

// ============================================================
//  curve_ functions
// ============================================================

void curve_line(GeoDataHandle h,
                float x0, float y0, float z0,
                float x1, float y1, float z1)
{
    auto* gd = toGD(h);
    uint32_t i0 = addVertex(gd, {x0, y0, z0});
    uint32_t i1 = addVertex(gd, {x1, y1, z1});
    gd->edges.push_back({i0, i1});
}

void curve_polyline(GeoDataHandle h, const float* pts, int count) {
    if (!pts || count < 2) return;
    auto* gd = toGD(h);
    uint32_t prev = addVertex(gd, {pts[0], pts[1], pts[2]});
    for (int i = 1; i < count; ++i) {
        uint32_t cur = addVertex(gd, {pts[i*3+0], pts[i*3+1], pts[i*3+2]});
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_arc(GeoDataHandle h,
               float cx, float cy, float cz,
               float r, float startAngle, float endAngle, int samples)
{
    if (samples < 2) samples = 2;
    auto* gd = toGD(h);
    float step = (endAngle - startAngle) / static_cast<float>(samples);
    uint32_t prev = addVertex(gd, {cx + r * std::cos(startAngle),
                                    cy + r * std::sin(startAngle), cz});
    for (int i = 1; i <= samples; ++i) {
        float a = startAngle + step * static_cast<float>(i);
        uint32_t cur = addVertex(gd, {cx + r * std::cos(a),
                                       cy + r * std::sin(a), cz});
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_circle(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int samples)
{
    curve_arc(h, cx, cy, cz, r, 0.f, 2.f * geo::PI, samples);
    // Close the loop
    auto* gd = toGD(h);
    if (!gd->edges.empty()) {
        uint32_t first = gd->edges.front().a;
        uint32_t last  = gd->edges.back().b;
        gd->edges.push_back({last, first});
    }
}

void curve_bezier(GeoDataHandle h, const float* pts, int count, int samples) {
    if (!pts || count < 2 || samples < 2) return;
    auto* gd = toGD(h);
    // De Casteljau evaluation
    std::vector<geo::Point3> cp(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        cp[i] = {pts[i*3+0], pts[i*3+1], pts[i*3+2]};

    auto eval = [&](float t) -> geo::Point3 {
        std::vector<geo::Point3> tmp = cp;
        for (int k = 1; k < count; ++k)
            for (int j = 0; j < count - k; ++j)
                tmp[j] = glm::mix(tmp[j], tmp[j+1], t);
        return tmp[0];
    };

    uint32_t prev = addVertex(gd, eval(0.f));
    for (int i = 1; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples);
        uint32_t cur = addVertex(gd, eval(t));
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_catmull(GeoDataHandle h, const float* pts, int count, int samples) {
    if (!pts || count < 2 || samples < 2) return;
    auto* gd = toGD(h);
    std::vector<geo::Point3> cp(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        cp[i] = {pts[i*3+0], pts[i*3+1], pts[i*3+2]};

    auto catmull = [&](int seg, float t) -> geo::Point3 {
        int i0 = std::max(0, seg - 1);
        int i1 = seg;
        int i2 = std::min(count - 1, seg + 1);
        int i3 = std::min(count - 1, seg + 2);
        float t2 = t * t, t3 = t * t * t;
        return 0.5f * ((2.f * cp[i1]) +
                       (-cp[i0] + cp[i2]) * t +
                       (2.f * cp[i0] - 5.f * cp[i1] + 4.f * cp[i2] - cp[i3]) * t2 +
                       (-cp[i0] + 3.f * cp[i1] - 3.f * cp[i2] + cp[i3]) * t3);
    };

    int segs = count - 1;
    uint32_t prev = addVertex(gd, cp[0]);
    for (int s = 0; s < segs; ++s) {
        for (int i = 1; i <= samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(samples);
            uint32_t cur = addVertex(gd, catmull(s, t));
            gd->edges.push_back({prev, cur});
            prev = cur;
        }
    }
}

// ============================================================
//  loop_ functions
// ============================================================

void loop_rect(GeoDataHandle h, float cx, float cy, float width, float height) {
    auto* gd = toGD(h);
    float hw = width  * 0.5f;
    float hh = height * 0.5f;
    uint32_t i0 = addVertex(gd, {cx - hw, cy - hh, 0.f});
    uint32_t i1 = addVertex(gd, {cx + hw, cy - hh, 0.f});
    uint32_t i2 = addVertex(gd, {cx + hw, cy + hh, 0.f});
    uint32_t i3 = addVertex(gd, {cx - hw, cy + hh, 0.f});
    gd->edges.push_back({i0, i1});
    gd->edges.push_back({i1, i2});
    gd->edges.push_back({i2, i3});
    gd->edges.push_back({i3, i0});
}

void loop_polygon(GeoDataHandle h, float cx, float cy, float r, int sides) {
    if (sides < 3) sides = 3;
    auto* gd = toGD(h);
    float step = 2.f * geo::PI / static_cast<float>(sides);
    std::vector<uint32_t> indices;
    for (int i = 0; i < sides; ++i) {
        float a = step * static_cast<float>(i);
        indices.push_back(addVertex(gd, {cx + r * std::cos(a), cy + r * std::sin(a), 0.f}));
    }
    for (int i = 0; i < sides; ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % sides]});
}

void loop_circle(GeoDataHandle h, float cx, float cy, float r, int samples) {
    loop_polygon(h, cx, cy, r, samples);
}

void loop_ellipse(GeoDataHandle h, float cx, float cy, float rx, float ry, int samples) {
    if (samples < 3) samples = 3;
    auto* gd = toGD(h);
    float step = 2.f * geo::PI / static_cast<float>(samples);
    std::vector<uint32_t> indices;
    for (int i = 0; i < samples; ++i) {
        float a = step * static_cast<float>(i);
        indices.push_back(addVertex(gd, {cx + rx * std::cos(a), cy + ry * std::sin(a), 0.f}));
    }
    for (int i = 0; i < samples; ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % samples]});
}

void loop_roundrect(GeoDataHandle h,
                    float cx, float cy, float width, float height,
                    float radius, int cornerSamples)
{
    if (cornerSamples < 2) cornerSamples = 4;
    auto* gd = toGD(h);
    float hw = width  * 0.5f - radius;
    float hh = height * 0.5f - radius;

    std::vector<uint32_t> indices;
    float corners[4][2] = {{cx+hw, cy+hh}, {cx-hw, cy+hh},
                            {cx-hw, cy-hh}, {cx+hw, cy-hh}};
    float startAngles[4] = {0.f, geo::PI*0.5f, geo::PI, geo::PI*1.5f};

    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= cornerSamples; ++i) {
            float a = startAngles[c] + (geo::PI * 0.5f) * static_cast<float>(i) / cornerSamples;
            indices.push_back(addVertex(gd, {
                corners[c][0] + radius * std::cos(a),
                corners[c][1] + radius * std::sin(a), 0.f}));
        }
    }
    for (size_t i = 0; i < indices.size(); ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % indices.size()]});
}

// ============================================================
//  path_ functions
// ============================================================

void path_line(GeoDataHandle h,
               float x0, float y0, float z0,
               float x1, float y1, float z1)
{
    curve_line(h, x0, y0, z0, x1, y1, z1);
}

void path_arc(GeoDataHandle h,
              float cx, float cy, float cz,
              float r, float startAngle, float endAngle, int samples)
{
    curve_arc(h, cx, cy, cz, r, startAngle, endAngle, samples);
}

void path_spline(GeoDataHandle h, const float* pts, int count, int samples) {
    curve_catmull(h, pts, count, samples);
}

// ============================================================
//  surface_ functions
// ============================================================

void surface_plane(GeoDataHandle h,
                   float cx, float cy, float cz,
                   float width, float height,
                   int subdX, int subdY)
{
    auto* gd = toGD(h);
    if (subdX < 1) subdX = 1;
    if (subdY < 1) subdY = 1;

    float dx = width  / static_cast<float>(subdX);
    float dy = height / static_cast<float>(subdY);
    float ox = cx - width  * 0.5f;
    float oy = cy - height * 0.5f;
    geo::Vector3 normal{0.f, 0.f, 1.f};

    // Vertex grid
    std::vector<std::vector<uint32_t>> grid(static_cast<size_t>(subdY + 1),
                                             std::vector<uint32_t>(static_cast<size_t>(subdX + 1)));
    for (int j = 0; j <= subdY; ++j) {
        for (int i = 0; i <= subdX; ++i) {
            float u = static_cast<float>(i) / subdX;
            float v = static_cast<float>(j) / subdY;
            geo::Vertex vtx;
            vtx.pos    = {ox + dx * i, oy + dy * j, cz};
            vtx.normal = normal;
            vtx.uv     = {u, v};
            gd->vertices.push_back(vtx);
            grid[j][i] = static_cast<uint32_t>(gd->vertices.size() - 1);
        }
    }
    for (int j = 0; j < subdY; ++j)
        for (int i = 0; i < subdX; ++i) {
            uint32_t a = grid[j][i],   b = grid[j][i+1];
            uint32_t c = grid[j+1][i], d = grid[j+1][i+1];
            gd->triangles.push_back({a, b, d});
            gd->triangles.push_back({a, d, c});
        }
}

void surface_disk(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int rings, int sectors)
{
    auto* gd = toGD(h);
    if (rings   < 1) rings   = 1;
    if (sectors < 3) sectors = 3;

    uint32_t center = addVertex(gd, {cx, cy, cz}, {0.f, 0.f, 1.f}, {0.5f, 0.5f});

    std::vector<std::vector<uint32_t>> grid(static_cast<size_t>(rings),
                                             std::vector<uint32_t>(static_cast<size_t>(sectors)));
    for (int ri = 0; ri < rings; ++ri) {
        float rad = r * static_cast<float>(ri + 1) / static_cast<float>(rings);
        for (int si = 0; si < sectors; ++si) {
            float a = 2.f * geo::PI * static_cast<float>(si) / static_cast<float>(sectors);
            float u = 0.5f + 0.5f * std::cos(a) * static_cast<float>(ri + 1) / rings;
            float v = 0.5f + 0.5f * std::sin(a) * static_cast<float>(ri + 1) / rings;
            grid[ri][si] = addVertex(gd,
                {cx + rad * std::cos(a), cy + rad * std::sin(a), cz},
                {0.f, 0.f, 1.f}, {u, v});
        }
    }
    // Inner ring triangles (connect to center)
    for (int si = 0; si < sectors; ++si) {
        uint32_t a = grid[0][si];
        uint32_t b = grid[0][(si + 1) % sectors];
        gd->triangles.push_back({center, a, b});
    }
    // Remaining ring quads
    for (int ri = 1; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri-1][si];
            uint32_t b = grid[ri-1][(si+1) % sectors];
            uint32_t c = grid[ri]  [(si+1) % sectors];
            uint32_t d = grid[ri]  [si];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }
}

void surface_fill(GeoDataHandle h, const float* pts2d, int count) {
    // Simple ear-clipping triangulation of a convex/simple polygon
    if (!pts2d || count < 3) return;
    auto* gd = toGD(h);
    std::vector<uint32_t> indices;
    for (int i = 0; i < count; ++i)
        indices.push_back(addVertex(gd, {pts2d[i*2], pts2d[i*2+1], 0.f}));
    for (int i = 1; i + 1 < count; ++i)
        gd->triangles.push_back({indices[0], indices[i], indices[i+1]});
}

// ============================================================
//  solid_ functions
// ============================================================

void solid_box(GeoDataHandle h,
               float cx, float cy, float cz,
               float wx, float wy, float wz)
{
    auto* gd = toGD(h);
    float hx = wx * 0.5f, hy = wy * 0.5f, hz = wz * 0.5f;

    // 8 corners
    geo::Point3 corners[8] = {
        {cx-hx, cy-hy, cz-hz}, {cx+hx, cy-hy, cz-hz},
        {cx+hx, cy+hy, cz-hz}, {cx-hx, cy+hy, cz-hz},
        {cx-hx, cy-hy, cz+hz}, {cx+hx, cy-hy, cz+hz},
        {cx+hx, cy+hy, cz+hz}, {cx-hx, cy+hy, cz+hz}
    };

    // 6 face normals
    geo::Vector3 normals[6] = {
        {0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}
    };

    int faces[6][4] = {
        {0,1,2,3}, // -Z
        {7,6,5,4}, // +Z
        {0,3,7,4}, // -X
        {1,5,6,2}, // +X
        {0,4,5,1}, // -Y
        {3,2,6,7}  // +Y
    };

    for (int f = 0; f < 6; ++f) {
        std::array<uint32_t, 4> idx;
        for (int k = 0; k < 4; ++k)
            idx[k] = addVertex(gd, corners[faces[f][k]], normals[f]);
        gd->triangles.push_back({idx[0], idx[1], idx[2]});
        gd->triangles.push_back({idx[0], idx[2], idx[3]});
    }
}

void solid_sphere(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int rings, int sectors)
{
    auto* gd = toGD(h);
    if (rings   < 2) rings   = 2;
    if (sectors < 3) sectors = 3;

    std::vector<std::vector<uint32_t>> grid(static_cast<size_t>(rings + 1),
                                             std::vector<uint32_t>(static_cast<size_t>(sectors + 1)));
    for (int ri = 0; ri <= rings; ++ri) {
        float phi = geo::PI * static_cast<float>(ri) / rings;
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * geo::PI * static_cast<float>(si) / sectors;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            geo::Vertex v;
            v.pos    = {cx + r*x, cy + r*y, cz + r*z};
            v.normal = {x, y, z};
            v.uv     = {static_cast<float>(si)/sectors, static_cast<float>(ri)/rings};
            gd->vertices.push_back(v);
            grid[ri][si] = static_cast<uint32_t>(gd->vertices.size() - 1);
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri  ][si],   b = grid[ri  ][si+1];
            uint32_t c = grid[ri+1][si+1], d = grid[ri+1][si];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }
}

void solid_cylinder(GeoDataHandle h,
                    float cx, float cy, float cz,
                    float r, float height,
                    int sectors, int rings)
{
    auto* gd = toGD(h);
    if (sectors < 3) sectors = 3;
    if (rings   < 1) rings   = 1;

    float halfH = height * 0.5f;

    // Side surface
    std::vector<std::vector<uint32_t>> grid(static_cast<size_t>(rings + 1),
                                             std::vector<uint32_t>(static_cast<size_t>(sectors + 1)));
    for (int ri = 0; ri <= rings; ++ri) {
        float y = -halfH + height * static_cast<float>(ri) / rings;
        for (int si = 0; si <= sectors; ++si) {
            float a = 2.f * geo::PI * static_cast<float>(si) / sectors;
            float nx = std::cos(a), nz = std::sin(a);
            geo::Vertex v;
            v.pos    = {cx + r*nx, cy + y, cz + r*nz};
            v.normal = {nx, 0.f, nz};
            v.uv     = {static_cast<float>(si)/sectors, static_cast<float>(ri)/rings};
            gd->vertices.push_back(v);
            grid[ri][si] = static_cast<uint32_t>(gd->vertices.size() - 1);
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri  ][si],   b = grid[ri  ][si+1];
            uint32_t c = grid[ri+1][si+1], d = grid[ri+1][si];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }

    // Caps
    uint32_t botCenter = addVertex(gd, {cx, cy-halfH, cz}, {0.f,-1.f,0.f}, {0.5f,0.5f});
    uint32_t topCenter = addVertex(gd, {cx, cy+halfH, cz}, {0.f, 1.f,0.f}, {0.5f,0.5f});
    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * static_cast<float>(si)   / sectors;
        float a1 = 2.f * geo::PI * static_cast<float>(si+1) / sectors;
        uint32_t b0 = addVertex(gd, {cx+r*std::cos(a0), cy-halfH, cz+r*std::sin(a0)}, {0.f,-1.f,0.f});
        uint32_t b1 = addVertex(gd, {cx+r*std::cos(a1), cy-halfH, cz+r*std::sin(a1)}, {0.f,-1.f,0.f});
        uint32_t t0 = addVertex(gd, {cx+r*std::cos(a0), cy+halfH, cz+r*std::sin(a0)}, {0.f, 1.f,0.f});
        uint32_t t1 = addVertex(gd, {cx+r*std::cos(a1), cy+halfH, cz+r*std::sin(a1)}, {0.f, 1.f,0.f});
        gd->triangles.push_back({botCenter, b1, b0});
        gd->triangles.push_back({topCenter, t0, t1});
    }
}

void solid_cone(GeoDataHandle h,
                float cx, float cy, float cz,
                float r, float height, int sectors)
{
    auto* gd = toGD(h);
    if (sectors < 3) sectors = 3;

    float halfH = height * 0.5f;
    uint32_t apex = addVertex(gd, {cx, cy + halfH, cz}, {0.f, 1.f, 0.f});

    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * static_cast<float>(si)   / sectors;
        float a1 = 2.f * geo::PI * static_cast<float>(si+1) / sectors;
        float nx0 = std::cos(a0), nz0 = std::sin(a0);
        float nx1 = std::cos(a1), nz1 = std::sin(a1);
        geo::Vector3 n0 = glm::normalize(geo::Vector3{nx0, r/height, nz0});
        geo::Vector3 n1 = glm::normalize(geo::Vector3{nx1, r/height, nz1});
        uint32_t b0 = addVertex(gd, {cx + r*nx0, cy - halfH, cz + r*nz0}, n0);
        uint32_t b1 = addVertex(gd, {cx + r*nx1, cy - halfH, cz + r*nz1}, n1);
        gd->triangles.push_back({apex, b0, b1});
    }

    // Base cap
    uint32_t base = addVertex(gd, {cx, cy - halfH, cz}, {0.f, -1.f, 0.f});
    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * static_cast<float>(si)   / sectors;
        float a1 = 2.f * geo::PI * static_cast<float>(si+1) / sectors;
        uint32_t b0 = addVertex(gd, {cx+r*std::cos(a0), cy-halfH, cz+r*std::sin(a0)}, {0.f,-1.f,0.f});
        uint32_t b1 = addVertex(gd, {cx+r*std::cos(a1), cy-halfH, cz+r*std::sin(a1)}, {0.f,-1.f,0.f});
        gd->triangles.push_back({base, b1, b0});
    }
}

void solid_torus(GeoDataHandle h,
                 float cx, float cy, float cz,
                 float R, float r, int rings, int sectors)
{
    auto* gd = toGD(h);
    if (rings   < 3) rings   = 3;
    if (sectors < 3) sectors = 3;

    std::vector<std::vector<uint32_t>> grid(static_cast<size_t>(rings),
                                             std::vector<uint32_t>(static_cast<size_t>(sectors)));
    for (int ri = 0; ri < rings; ++ri) {
        float phi = 2.f * geo::PI * static_cast<float>(ri) / rings;
        for (int si = 0; si < sectors; ++si) {
            float theta = 2.f * geo::PI * static_cast<float>(si) / sectors;
            float x = (R + r * std::cos(theta)) * std::cos(phi);
            float z = (R + r * std::cos(theta)) * std::sin(phi);
            float y = r * std::sin(theta);
            geo::Vector3 n = glm::normalize(geo::Vector3{
                std::cos(theta) * std::cos(phi),
                std::sin(theta),
                std::cos(theta) * std::sin(phi)
            });
            geo::Vertex v;
            v.pos    = {cx+x, cy+y, cz+z};
            v.normal = n;
            v.uv     = {static_cast<float>(ri)/rings, static_cast<float>(si)/sectors};
            gd->vertices.push_back(v);
            grid[ri][si] = static_cast<uint32_t>(gd->vertices.size() - 1);
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri][(si+1)%sectors];
            uint32_t b = grid[ri][si];
            uint32_t c = grid[(ri+1)%rings][si];
            uint32_t d = grid[(ri+1)%rings][(si+1)%sectors];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }
}

void solid_capsule(GeoDataHandle h,
                   float cx, float cy, float cz,
                   float r, float height,
                   int rings, int sectors)
{
    // Cylinder body
    solid_cylinder(h, cx, cy, cz, r, height, sectors, rings);

    // Hemisphere caps (approximate with sphere halves)
    auto* gd = toGD(h);
    float halfH = height * 0.5f;
    int hRings = std::max(2, rings / 2);

    for (int ri = 0; ri <= hRings; ++ri) {
        // Top cap
        float phi = geo::PI * 0.5f * static_cast<float>(ri) / hRings;
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * geo::PI * static_cast<float>(si) / sectors;
            float nx = std::sin(phi) * std::cos(theta);
            float ny = std::cos(phi);
            float nz = std::sin(phi) * std::sin(theta);
            geo::Vertex v;
            v.pos    = {cx + r*nx, cy + halfH + r*ny, cz + r*nz};
            v.normal = {nx, ny, nz};
            gd->vertices.push_back(v);
        }
        // Bottom cap
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * geo::PI * static_cast<float>(si) / sectors;
            float nx = std::sin(phi) * std::cos(theta);
            float ny = -std::cos(phi);
            float nz = std::sin(phi) * std::sin(theta);
            geo::Vertex v;
            v.pos    = {cx + r*nx, cy - halfH + r*ny, cz + r*nz};
            v.normal = {nx, ny, nz};
            gd->vertices.push_back(v);
        }
    }
}
