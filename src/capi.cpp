/**
 * @file capi.cpp
 * @brief C API 实现
 *
 * 核心设计：
 * - entity_ 函数：从Entity指针直接读取属性值
 * - geo_ 函数：向GeoData缓冲区添加几何数据
 * - 其他函数：生成几何形状
 */

#include "capi.h"

#include <cmath>
#include <vector>
#include <cstring>
#include <array>

// Clipper2 和 earcut
#include <clipper2/clipper.h>
#include <earcut.hpp>

namespace {

// ============================================================
//  辅助函数
// ============================================================

inline uint32_t addVertexInternal(geo::GeoData* gd,
                                   const geo::Point3& pos,
                                   const geo::Vector3& normal = {0.f, 0.f, 1.f},
                                   const geo::Vector2& uv = {0.f, 0.f}) {
    geo::Vertex v;
    v.pos = pos;
    v.normal = normal;
    v.uv = uv;
    gd->vertices.push_back(v);
    return static_cast<uint32_t>(gd->vertices.size() - 1);
}

} // anonymous namespace

// ============================================================
//  常量
// ============================================================

const float CAPI_PI = geo::PI;
const float CAPI_E  = geo::E;

// ============================================================
//  Entity 属性访问
// ============================================================

float entity_getFloat(const geo::Entity* entity, const char* attrName) {
    if (!entity || !attrName) return 0.f;
    return entity->getFloat(attrName, 0.f);
}

int entity_getInt(const geo::Entity* entity, const char* attrName) {
    if (!entity || !attrName) return 0;
    return entity->getInt(attrName, 0);
}

int entity_getBool(const geo::Entity* entity, const char* attrName) {
    if (!entity || !attrName) return 0;
    return entity->getBool(attrName, false) ? 1 : 0;
}

int entity_hasAttr(const geo::Entity* entity, const char* attrName) {
    if (!entity || !attrName) return 0;
    return entity->hasAttr(attrName) ? 1 : 0;
}

const char* entity_getName(const geo::Entity* entity) {
    if (!entity) return "";
    return entity->name().c_str();
}

uint64_t entity_getId(const geo::Entity* entity) {
    if (!entity) return 0;
    return entity->id();
}

// ============================================================
//  GeoData 几何输出
// ============================================================

uint32_t geo_addVertex(geo::GeoData* geoData,
                        float x, float y, float z,
                        float nx, float ny, float nz,
                        float u, float v) {
    if (!geoData) return 0;
    return addVertexInternal(geoData, {x, y, z}, {nx, ny, nz}, {u, v});
}

void geo_addEdge(geo::GeoData* geoData, uint32_t a, uint32_t b) {
    if (!geoData) return;
    geoData->edges.push_back({a, b});
}

void geo_addTriangle(geo::GeoData* geoData, uint32_t a, uint32_t b, uint32_t c) {
    if (!geoData) return;
    geoData->triangles.push_back({a, b, c});
}

// ============================================================
//  math_ 数学函数
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
//  curve_ 曲线函数
// ============================================================

void curve_line(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1) {
    uint32_t i0 = addVertexInternal(gd, {x0, y0, z0});
    uint32_t i1 = addVertexInternal(gd, {x1, y1, z1});
    gd->edges.push_back({i0, i1});
}

void curve_polyline(geo::GeoData* gd, const float* pts, int count) {
    if (!pts || count < 2) return;
    uint32_t prev = addVertexInternal(gd, {pts[0], pts[1], pts[2]});
    for (int i = 1; i < count; ++i) {
        uint32_t cur = addVertexInternal(gd, {pts[i*3], pts[i*3+1], pts[i*3+2]});
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_arc(geo::GeoData* gd, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples) {
    if (samples < 2) samples = 2;
    float step = (endAngle - startAngle) / samples;
    uint32_t prev = addVertexInternal(gd, {cx + r * std::cos(startAngle), cy + r * std::sin(startAngle), cz});
    for (int i = 1; i <= samples; ++i) {
        float a = startAngle + step * i;
        uint32_t cur = addVertexInternal(gd, {cx + r * std::cos(a), cy + r * std::sin(a), cz});
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_circle(geo::GeoData* gd, float cx, float cy, float cz, float r, int samples) {
    curve_arc(gd, cx, cy, cz, r, 0.f, 2.f * geo::PI, samples);
    if (!gd->edges.empty()) {
        gd->edges.push_back({gd->edges.back().b, gd->edges.front().a});
    }
}

void curve_bezier(geo::GeoData* gd, const float* pts, int count, int samples) {
    if (!pts || count < 2 || samples < 2) return;
    std::vector<geo::Point3> cp(count);
    for (int i = 0; i < count; ++i) cp[i] = {pts[i*3], pts[i*3+1], pts[i*3+2]};

    auto eval = [&](float t) -> geo::Point3 {
        std::vector<geo::Point3> tmp = cp;
        for (int k = 1; k < count; ++k)
            for (int j = 0; j < count - k; ++j)
                tmp[j] = glm::mix(tmp[j], tmp[j+1], t);
        return tmp[0];
    };

    uint32_t prev = addVertexInternal(gd, eval(0.f));
    for (int i = 1; i <= samples; ++i) {
        float t = (float)i / samples;
        uint32_t cur = addVertexInternal(gd, eval(t));
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_catmull(geo::GeoData* gd, const float* pts, int count, int samples) {
    if (!pts || count < 2 || samples < 2) return;
    std::vector<geo::Point3> cp(count);
    for (int i = 0; i < count; ++i) cp[i] = {pts[i*3], pts[i*3+1], pts[i*3+2]};

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
    uint32_t prev = addVertexInternal(gd, cp[0]);
    for (int s = 0; s < segs; ++s) {
        for (int i = 1; i <= samples; ++i) {
            float t = (float)i / samples;
            uint32_t cur = addVertexInternal(gd, catmull(s, t));
            gd->edges.push_back({prev, cur});
            prev = cur;
        }
    }
}

// ============================================================
//  loop_ 循环函数
// ============================================================

void loop_rect(geo::GeoData* gd, float cx, float cy, float width, float height) {
    float hw = width * 0.5f, hh = height * 0.5f;
    uint32_t i0 = addVertexInternal(gd, {cx - hw, cy - hh, 0.f});
    uint32_t i1 = addVertexInternal(gd, {cx + hw, cy - hh, 0.f});
    uint32_t i2 = addVertexInternal(gd, {cx + hw, cy + hh, 0.f});
    uint32_t i3 = addVertexInternal(gd, {cx - hw, cy + hh, 0.f});
    gd->edges.push_back({i0, i1});
    gd->edges.push_back({i1, i2});
    gd->edges.push_back({i2, i3});
    gd->edges.push_back({i3, i0});
}

void loop_polygon(geo::GeoData* gd, float cx, float cy, float r, int sides) {
    if (sides < 3) sides = 3;
    float step = 2.f * geo::PI / sides;
    std::vector<uint32_t> indices;
    for (int i = 0; i < sides; ++i) {
        float a = step * i;
        indices.push_back(addVertexInternal(gd, {cx + r * std::cos(a), cy + r * std::sin(a), 0.f}));
    }
    for (int i = 0; i < sides; ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % sides]});
}

void loop_circle(geo::GeoData* gd, float cx, float cy, float r, int samples) {
    loop_polygon(gd, cx, cy, r, samples);
}

void loop_ellipse(geo::GeoData* gd, float cx, float cy, float rx, float ry, int samples) {
    if (samples < 3) samples = 3;
    float step = 2.f * geo::PI / samples;
    std::vector<uint32_t> indices;
    for (int i = 0; i < samples; ++i) {
        float a = step * i;
        indices.push_back(addVertexInternal(gd, {cx + rx * std::cos(a), cy + ry * std::sin(a), 0.f}));
    }
    for (int i = 0; i < samples; ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % samples]});
}

void loop_roundrect(geo::GeoData* gd, float cx, float cy, float width, float height, float radius, int samples) {
    if (samples < 2) samples = 4;
    float hw = width * 0.5f - radius;
    float hh = height * 0.5f - radius;

    std::vector<uint32_t> indices;
    float corners[4][2] = {{cx+hw, cy+hh}, {cx-hw, cy+hh}, {cx-hw, cy-hh}, {cx+hw, cy-hh}};
    float startAngles[4] = {0.f, geo::PI*0.5f, geo::PI, geo::PI*1.5f};

    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= samples; ++i) {
            float a = startAngles[c] + (geo::PI * 0.5f) * i / samples;
            indices.push_back(addVertexInternal(gd, {
                corners[c][0] + radius * std::cos(a),
                corners[c][1] + radius * std::sin(a), 0.f}));
        }
    }
    for (size_t i = 0; i < indices.size(); ++i)
        gd->edges.push_back({indices[i], indices[(i+1) % indices.size()]});
}

// ============================================================
//  布尔运算 (Clipper2)
// ============================================================

namespace {

using namespace Clipper2Lib;

PathD floatArrayToPathD(const float* pts, int count) {
    PathD path;
    path.reserve(count);
    for (int i = 0; i < count; ++i)
        path.push_back(PointD(pts[i*2], pts[i*2+1]));
    return path;
}

PathsD floatArrayToPathsD(const float* pts, int count) {
    PathsD paths;
    paths.push_back(floatArrayToPathD(pts, count));
    return paths;
}

void pathsDToGeoDataEdges(geo::GeoData* gd, const PathsD& result) {
    for (const auto& path : result) {
        if (path.empty()) continue;
        uint32_t firstIdx = addVertexInternal(gd, {(float)path[0].x, (float)path[0].y, 0.f});
        uint32_t prevIdx = firstIdx;
        for (size_t i = 1; i < path.size(); ++i) {
            uint32_t curIdx = addVertexInternal(gd, {(float)path[i].x, (float)path[i].y, 0.f});
            gd->edges.push_back({prevIdx, curIdx});
            prevIdx = curIdx;
        }
        if (path.size() > 2) gd->edges.push_back({prevIdx, firstIdx});
    }
}

} // anonymous namespace

void loop_union(geo::GeoData* gd, const float* l1, int c1, const float* l2, int c2) {
    if (!l1 || !l2 || c1 < 3 || c2 < 3) return;
    PathsD subj = floatArrayToPathsD(l1, c1);
    PathsD clip = floatArrayToPathsD(l2, c2);
    PathsD result = Union(subj, clip, FillRule::NonZero);
    pathsDToGeoDataEdges(gd, result);
}

void loop_intersect(geo::GeoData* gd, const float* l1, int c1, const float* l2, int c2) {
    if (!l1 || !l2 || c1 < 3 || c2 < 3) return;
    PathsD subj = floatArrayToPathsD(l1, c1);
    PathsD clip = floatArrayToPathsD(l2, c2);
    PathsD result = Intersect(subj, clip, FillRule::NonZero);
    pathsDToGeoDataEdges(gd, result);
}

void loop_difference(geo::GeoData* gd, const float* l1, int c1, const float* l2, int c2) {
    if (!l1 || !l2 || c1 < 3 || c2 < 3) return;
    PathsD subj = floatArrayToPathsD(l1, c1);
    PathsD clip = floatArrayToPathsD(l2, c2);
    PathsD result = Difference(subj, clip, FillRule::NonZero);
    pathsDToGeoDataEdges(gd, result);
}

void loop_xor(geo::GeoData* gd, const float* l1, int c1, const float* l2, int c2) {
    if (!l1 || !l2 || c1 < 3 || c2 < 3) return;
    PathsD subj = floatArrayToPathsD(l1, c1);
    PathsD clip = floatArrayToPathsD(l2, c2);
    PathsD result = Clipper2Lib::Xor(subj, clip, FillRule::NonZero);
    pathsDToGeoDataEdges(gd, result);
}

void loop_offset(geo::GeoData* gd, const float* pts, int count, float delta, int joinType) {
    if (!pts || count < 3) return;
    PathsD paths = floatArrayToPathsD(pts, count);
    JoinType jt = JoinType::Round;
    switch (joinType) {
        case 0: jt = JoinType::Square; break;
        case 2: jt = JoinType::Miter; break;
    }
    PathsD result = InflatePaths(paths, delta, jt, EndType::Polygon);
    pathsDToGeoDataEdges(gd, result);
}

// ============================================================
//  三角剖分 (earcut)
// ============================================================

void loop_fill(geo::GeoData* gd, const float* pts, int count) {
    if (!pts || count < 3) return;

    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon;
    std::vector<Point> contour;
    for (int i = 0; i < count; ++i)
        contour.push_back({pts[i*2], pts[i*2+1]});
    polygon.push_back(contour);

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    uint32_t baseIdx = gd->vertices.size();
    for (int i = 0; i < count; ++i)
        addVertexInternal(gd, {pts[i*2], pts[i*2+1], 0.f});

    for (size_t i = 0; i < indices.size(); i += 3)
        gd->triangles.push_back({baseIdx + indices[i], baseIdx + indices[i+1], baseIdx + indices[i+2]});
}

void loop_fill_with_holes(geo::GeoData* gd, const float* outer, int outerCount, const float* holes, const int* holeCounts, int numHoles) {
    if (!outer || outerCount < 3) return;

    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon;

    std::vector<Point> outerContour;
    for (int i = 0; i < outerCount; ++i)
        outerContour.push_back({outer[i*2], outer[i*2+1]});
    polygon.push_back(outerContour);

    int holeOffset = 0;
    for (int h = 0; h < numHoles; ++h) {
        int hc = holeCounts[h];
        if (hc < 3) { holeOffset += hc * 2; continue; }
        std::vector<Point> holeContour;
        for (int i = 0; i < hc; ++i)
            holeContour.push_back({holes[holeOffset + i*2], holes[holeOffset + i*2 + 1]});
        polygon.push_back(holeContour);
        holeOffset += hc * 2;
    }

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    uint32_t baseIdx = gd->vertices.size();
    for (int i = 0; i < outerCount; ++i)
        addVertexInternal(gd, {outer[i*2], outer[i*2+1], 0.f});

    holeOffset = 0;
    for (int h = 0; h < numHoles; ++h) {
        int hc = holeCounts[h];
        for (int i = 0; i < hc; ++i)
            addVertexInternal(gd, {holes[holeOffset + i*2], holes[holeOffset + i*2 + 1], 0.f});
        holeOffset += hc * 2;
    }

    for (size_t i = 0; i < indices.size(); i += 3)
        gd->triangles.push_back({baseIdx + indices[i], baseIdx + indices[i+1], baseIdx + indices[i+2]});
}

void loop_simplify(geo::GeoData* gd, const float* pts, int count, float epsilon) {
    if (!pts || count < 3) return;
    PathsD paths = floatArrayToPathsD(pts, count);
    PathsD result = SimplifyPaths(paths, epsilon);
    pathsDToGeoDataEdges(gd, result);
}

// ============================================================
//  path_ 路径函数
// ============================================================

void path_line(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1) {
    curve_line(gd, x0, y0, z0, x1, y1, z1);
}

void path_arc(geo::GeoData* gd, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples) {
    curve_arc(gd, cx, cy, cz, r, startAngle, endAngle, samples);
}

void path_spline(geo::GeoData* gd, const float* pts, int count, int samples) {
    curve_catmull(gd, pts, count, samples);
}

// ============================================================
//  surface_ 表面函数
// ============================================================

void surface_plane(geo::GeoData* gd, float cx, float cy, float cz, float width, float height, int subdX, int subdY) {
    if (subdX < 1) subdX = 1;
    if (subdY < 1) subdY = 1;
    float dx = width / subdX, dy = height / subdY;
    float ox = cx - width * 0.5f, oy = cy - height * 0.5f;

    std::vector<std::vector<uint32_t>> grid(subdY + 1, std::vector<uint32_t>(subdX + 1));
    for (int j = 0; j <= subdY; ++j) {
        for (int i = 0; i <= subdX; ++i) {
            geo::Vertex v;
            v.pos = {ox + dx * i, oy + dy * j, cz};
            v.normal = {0.f, 0.f, 1.f};
            v.uv = {(float)i / subdX, (float)j / subdY};
            gd->vertices.push_back(v);
            grid[j][i] = gd->vertices.size() - 1;
        }
    }
    for (int j = 0; j < subdY; ++j)
        for (int i = 0; i < subdX; ++i) {
            gd->triangles.push_back({grid[j][i], grid[j][i+1], grid[j+1][i+1]});
            gd->triangles.push_back({grid[j][i], grid[j+1][i+1], grid[j+1][i]});
        }
}

void surface_disk(geo::GeoData* gd, float cx, float cy, float cz, float r, int rings, int sectors) {
    if (rings < 1) rings = 1;
    if (sectors < 3) sectors = 3;

    uint32_t center = addVertexInternal(gd, {cx, cy, cz}, {0.f, 0.f, 1.f}, {0.5f, 0.5f});

    std::vector<std::vector<uint32_t>> grid(rings, std::vector<uint32_t>(sectors));
    for (int ri = 0; ri < rings; ++ri) {
        float rad = r * (ri + 1) / rings;
        for (int si = 0; si < sectors; ++si) {
            float a = 2.f * geo::PI * si / sectors;
            grid[ri][si] = addVertexInternal(gd,
                {cx + rad * std::cos(a), cy + rad * std::sin(a), cz},
                {0.f, 0.f, 1.f});
        }
    }
    for (int si = 0; si < sectors; ++si)
        gd->triangles.push_back({center, grid[0][si], grid[0][(si+1) % sectors]});

    for (int ri = 1; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri-1][si], b = grid[ri-1][(si+1) % sectors];
            uint32_t c = grid[ri][(si+1) % sectors], d = grid[ri][si];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }
}

void surface_fill(geo::GeoData* gd, const float* pts2d, int count) {
    loop_fill(gd, pts2d, count);
}

// ============================================================
//  solid_ 实体函数
// ============================================================

void solid_box(geo::GeoData* gd, float cx, float cy, float cz, float wx, float wy, float wz) {
    float hx = wx * 0.5f, hy = wy * 0.5f, hz = wz * 0.5f;
    geo::Point3 corners[8] = {
        {cx-hx, cy-hy, cz-hz}, {cx+hx, cy-hy, cz-hz},
        {cx+hx, cy+hy, cz-hz}, {cx-hx, cy+hy, cz-hz},
        {cx-hx, cy-hy, cz+hz}, {cx+hx, cy-hy, cz+hz},
        {cx+hx, cy+hy, cz+hz}, {cx-hx, cy+hy, cz+hz}
    };
    geo::Vector3 normals[6] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
    int faces[6][4] = {{0,1,2,3},{7,6,5,4},{0,3,7,4},{1,5,6,2},{0,4,5,1},{3,2,6,7}};

    for (int f = 0; f < 6; ++f) {
        std::array<uint32_t, 4> idx;
        for (int k = 0; k < 4; ++k)
            idx[k] = addVertexInternal(gd, corners[faces[f][k]], normals[f]);
        gd->triangles.push_back({idx[0], idx[1], idx[2]});
        gd->triangles.push_back({idx[0], idx[2], idx[3]});
    }
}

void solid_sphere(geo::GeoData* gd, float cx, float cy, float cz, float r, int rings, int sectors) {
    if (rings < 2) rings = 2;
    if (sectors < 3) sectors = 3;

    std::vector<std::vector<uint32_t>> grid(rings + 1, std::vector<uint32_t>(sectors + 1));
    for (int ri = 0; ri <= rings; ++ri) {
        float phi = geo::PI * ri / rings;
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * geo::PI * si / sectors;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            geo::Vertex v;
            v.pos = {cx + r*x, cy + r*y, cz + r*z};
            v.normal = {x, y, z};
            v.uv = {(float)si/sectors, (float)ri/rings};
            gd->vertices.push_back(v);
            grid[ri][si] = gd->vertices.size() - 1;
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            gd->triangles.push_back({grid[ri][si], grid[ri][si+1], grid[ri+1][si+1]});
            gd->triangles.push_back({grid[ri][si], grid[ri+1][si+1], grid[ri+1][si]});
        }
}

void solid_cylinder(geo::GeoData* gd, float cx, float cy, float cz, float r, float height, int sectors, int rings) {
    if (sectors < 3) sectors = 3;
    if (rings < 1) rings = 1;
    float halfH = height * 0.5f;

    std::vector<std::vector<uint32_t>> grid(rings + 1, std::vector<uint32_t>(sectors + 1));
    for (int ri = 0; ri <= rings; ++ri) {
        float y = -halfH + height * ri / rings;
        for (int si = 0; si <= sectors; ++si) {
            float a = 2.f * geo::PI * si / sectors;
            float nx = std::cos(a), nz = std::sin(a);
            geo::Vertex v;
            v.pos = {cx + r*nx, cy + y, cz + r*nz};
            v.normal = {nx, 0.f, nz};
            v.uv = {(float)si/sectors, (float)ri/rings};
            gd->vertices.push_back(v);
            grid[ri][si] = gd->vertices.size() - 1;
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            gd->triangles.push_back({grid[ri][si], grid[ri][si+1], grid[ri+1][si+1]});
            gd->triangles.push_back({grid[ri][si], grid[ri+1][si+1], grid[ri+1][si]});
        }

    uint32_t botCenter = addVertexInternal(gd, {cx, cy-halfH, cz}, {0.f,-1.f,0.f});
    uint32_t topCenter = addVertexInternal(gd, {cx, cy+halfH, cz}, {0.f, 1.f,0.f});
    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * si / sectors;
        float a1 = 2.f * geo::PI * (si+1) / sectors;
        uint32_t b0 = addVertexInternal(gd, {cx+r*std::cos(a0), cy-halfH, cz+r*std::sin(a0)}, {0.f,-1.f,0.f});
        uint32_t b1 = addVertexInternal(gd, {cx+r*std::cos(a1), cy-halfH, cz+r*std::sin(a1)}, {0.f,-1.f,0.f});
        uint32_t t0 = addVertexInternal(gd, {cx+r*std::cos(a0), cy+halfH, cz+r*std::sin(a0)}, {0.f, 1.f,0.f});
        uint32_t t1 = addVertexInternal(gd, {cx+r*std::cos(a1), cy+halfH, cz+r*std::sin(a1)}, {0.f, 1.f,0.f});
        gd->triangles.push_back({botCenter, b1, b0});
        gd->triangles.push_back({topCenter, t0, t1});
    }
}

void solid_cone(geo::GeoData* gd, float cx, float cy, float cz, float r, float height, int sectors) {
    if (sectors < 3) sectors = 3;
    float halfH = height * 0.5f;
    uint32_t apex = addVertexInternal(gd, {cx, cy + halfH, cz}, {0.f, 1.f, 0.f});

    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * si / sectors;
        float a1 = 2.f * geo::PI * (si+1) / sectors;
        float nx0 = std::cos(a0), nz0 = std::sin(a0);
        float nx1 = std::cos(a1), nz1 = std::sin(a1);
        geo::Vector3 n0 = glm::normalize(geo::Vector3{nx0, r/height, nz0});
        geo::Vector3 n1 = glm::normalize(geo::Vector3{nx1, r/height, nz1});
        uint32_t b0 = addVertexInternal(gd, {cx + r*nx0, cy - halfH, cz + r*nz0}, n0);
        uint32_t b1 = addVertexInternal(gd, {cx + r*nx1, cy - halfH, cz + r*nz1}, n1);
        gd->triangles.push_back({apex, b0, b1});
    }

    uint32_t base = addVertexInternal(gd, {cx, cy - halfH, cz}, {0.f, -1.f, 0.f});
    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * geo::PI * si / sectors;
        float a1 = 2.f * geo::PI * (si+1) / sectors;
        uint32_t b0 = addVertexInternal(gd, {cx+r*std::cos(a0), cy-halfH, cz+r*std::sin(a0)}, {0.f,-1.f,0.f});
        uint32_t b1 = addVertexInternal(gd, {cx+r*std::cos(a1), cy-halfH, cz+r*std::sin(a1)}, {0.f,-1.f,0.f});
        gd->triangles.push_back({base, b1, b0});
    }
}

void solid_torus(geo::GeoData* gd, float cx, float cy, float cz, float R, float r, int rings, int sectors) {
    if (rings < 3) rings = 3;
    if (sectors < 3) sectors = 3;

    std::vector<std::vector<uint32_t>> grid(rings, std::vector<uint32_t>(sectors));
    for (int ri = 0; ri < rings; ++ri) {
        float phi = 2.f * geo::PI * ri / rings;
        for (int si = 0; si < sectors; ++si) {
            float theta = 2.f * geo::PI * si / sectors;
            float x = (R + r * std::cos(theta)) * std::cos(phi);
            float z = (R + r * std::cos(theta)) * std::sin(phi);
            float y = r * std::sin(theta);
            geo::Vector3 n = glm::normalize(geo::Vector3{
                std::cos(theta) * std::cos(phi), std::sin(theta), std::cos(theta) * std::sin(phi)});
            geo::Vertex v;
            v.pos = {cx+x, cy+y, cz+z};
            v.normal = n;
            v.uv = {(float)ri/rings, (float)si/sectors};
            gd->vertices.push_back(v);
            grid[ri][si] = gd->vertices.size() - 1;
        }
    }
    for (int ri = 0; ri < rings; ++ri)
        for (int si = 0; si < sectors; ++si) {
            uint32_t a = grid[ri][(si+1) % sectors];
            uint32_t b = grid[ri][si];
            uint32_t c = grid[(ri+1) % rings][si];
            uint32_t d = grid[(ri+1) % rings][(si+1) % sectors];
            gd->triangles.push_back({a, b, c});
            gd->triangles.push_back({a, c, d});
        }
}

void solid_capsule(geo::GeoData* gd, float cx, float cy, float cz, float r, float height, int rings, int sectors) {
    solid_cylinder(gd, cx, cy, cz, r, height, sectors, rings);
    // 半球顶盖可以简化处理
}

// ============================================================
//  curve_factory_ 曲线工厂函数
// ============================================================

void curve_factory_line(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1) {
    if (!gd) return;
    auto curve = geo::CurveFactory::line(x0, y0, z0, x1, y1, z1);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_arc(geo::GeoData* gd, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::arc(cx, cy, cz, r, startAngle, endAngle, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_circle(geo::GeoData* gd, float cx, float cy, float cz, float r, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::circle(cx, cy, cz, r, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_ellipse_arc(geo::GeoData* gd, float cx, float cy, float cz, float rx, float ry, float startAngle, float endAngle, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::ellipseArc(geo::Point3(cx, cy, cz), rx, ry, startAngle, endAngle, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_ellipse(geo::GeoData* gd, float cx, float cy, float cz, float rx, float ry, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::ellipse(geo::Point3(cx, cy, cz), rx, ry, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_quadratic_bezier(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::quadraticBezier(
        geo::Point3(x0, y0, z0), geo::Point3(x1, y1, z1), geo::Point3(x2, y2, z2), samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_cubic_bezier(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::cubicBezier(
        geo::Point3(x0, y0, z0), geo::Point3(x1, y1, z1), 
        geo::Point3(x2, y2, z2), geo::Point3(x3, y3, z3), samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_helix(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, float turns, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::helix(geo::Point3(cx, cy, cz), radius, height, turns, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_conic_helix(geo::GeoData* gd, float cx, float cy, float cz, float startRadius, float endRadius, float height, float turns, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::conicHelix(geo::Point3(cx, cy, cz), startRadius, endRadius, height, turns, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

void curve_factory_sine_wave(geo::GeoData* gd, float x0, float y0, float z0, float x1, float y1, float z1, float amplitude, float frequency, int samples) {
    if (!gd) return;
    auto curve = geo::CurveFactory::sineWave(geo::Point3(x0, y0, z0), geo::Point3(x1, y1, z1), amplitude, frequency, samples);
    uint32_t prev = addVertexInternal(gd, curve.points[0]);
    for (size_t i = 1; i < curve.points.size(); ++i) {
        uint32_t cur = addVertexInternal(gd, curve.points[i]);
        gd->edges.push_back({prev, cur});
        prev = cur;
    }
}

// ============================================================
//  loop_factory_ 截面工厂函数
// ============================================================

void loop_factory_rect(geo::GeoData* gd, float cx, float cy, float width, float height) {
    if (!gd) return;
    auto loop = geo::LoopFactory::rectangle(cx, cy, width, height);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_square(geo::GeoData* gd, float cx, float cy, float size) {
    loop_factory_rect(gd, cx, cy, size, size);
}

void loop_factory_circle(geo::GeoData* gd, float cx, float cy, float radius, int samples) {
    if (!gd) return;
    auto loop = geo::LoopFactory::circle(cx, cy, radius, samples);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_ellipse(geo::GeoData* gd, float cx, float cy, float rx, float ry, int samples) {
    if (!gd) return;
    auto loop = geo::LoopFactory::ellipse(cx, cy, rx, ry, samples);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_rounded_rect(geo::GeoData* gd, float cx, float cy, float width, float height, float radius, int cornerSamples) {
    if (!gd) return;
    auto loop = geo::LoopFactory::roundedRectangle(cx, cy, width, height, radius, cornerSamples);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_regular_polygon(geo::GeoData* gd, float cx, float cy, int sides, float radius) {
    if (!gd || sides < 3) return;
    auto loop = geo::LoopFactory::regularPolygon(cx, cy, sides, radius);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_star(geo::GeoData* gd, float cx, float cy, int points, float outerRadius, float innerRadius) {
    if (!gd || points < 3) return;
    auto loop = geo::LoopFactory::star(cx, cy, points, outerRadius, innerRadius);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_gear(geo::GeoData* gd, float cx, float cy, int teeth, float outerRadius, float innerRadius, float toothDepth) {
    if (!gd || teeth < 3) return;
    auto loop = geo::LoopFactory::gear(teeth, outerRadius, innerRadius, toothDepth);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, geo::Point3(p.x + cx, p.y + cy, p.z)));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

void loop_factory_arc_segment(geo::GeoData* gd, float cx, float cy, float innerRadius, float outerRadius, float startAngle, float endAngle, int samples) {
    if (!gd) return;
    auto loop = geo::LoopFactory::arcSegment(cx, cy, innerRadius, outerRadius, startAngle, endAngle, samples);
    auto pts = loop.samplePoints();
    std::vector<uint32_t> indices;
    for (const auto& p : pts) indices.push_back(addVertexInternal(gd, p));
    for (size_t i = 0; i < indices.size() - 1; ++i)
        gd->edges.push_back({indices[i], indices[i + 1]});
}

// ============================================================
//  solid_factory_ 实体工厂函数
// ============================================================

void solid_factory_box(geo::GeoData* gd, float cx, float cy, float cz, float width, float height, float depth) {
    if (!gd) return;
    geo::SolidFactory::box(*gd, cx, cy, cz, width, height, depth);
}

void solid_factory_sphere(geo::GeoData* gd, float cx, float cy, float cz, float radius, int rings, int sectors) {
    if (!gd) return;
    geo::SolidFactory::sphere(*gd, cx, cy, cz, radius, rings, sectors);
}

void solid_factory_cylinder(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, int sectors, int rings) {
    if (!gd) return;
    geo::SolidFactory::cylinder(*gd, cx, cy, cz, radius, height, sectors, rings);
}

void solid_factory_cone(geo::GeoData* gd, float cx, float cy, float cz, float baseRadius, float height, int sectors) {
    if (!gd) return;
    geo::SolidFactory::cone(*gd, cx, cy, cz, baseRadius, height, sectors);
}

void solid_factory_cone_frustum(geo::GeoData* gd, float cx, float cy, float cz, float bottomRadius, float topRadius, float height, int sectors) {
    if (!gd) return;
    geo::SolidFactory::coneFrustum(*gd, bottomRadius, topRadius, height, sectors);
}

void solid_factory_torus(geo::GeoData* gd, float cx, float cy, float cz, float majorRadius, float minorRadius, int rings, int sectors) {
    if (!gd) return;
    geo::SolidFactory::torus(*gd, cx, cy, cz, majorRadius, minorRadius, rings, sectors);
}

void solid_factory_capsule(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, int rings, int sectors) {
    if (!gd) return;
    geo::SolidFactory::capsule(*gd, radius, height, rings, sectors);
}

void solid_factory_extrude_rect(geo::GeoData* gd, float cx, float cy, float cz, float width, float height, float depth, int subdivisions) {
    if (!gd) return;
    auto loop = geo::LoopFactory::rectangle(0, 0, width, height);
    geo::SolidFactory::extrude(*gd, loop, depth, subdivisions);
}

void solid_factory_extrude_circle(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, int samples, int subdivisions) {
    if (!gd) return;
    auto loop = geo::LoopFactory::circle(0, 0, radius, samples);
    geo::SolidFactory::extrude(*gd, loop, height, subdivisions);
}

void solid_factory_extrude_polygon(geo::GeoData* gd, float cx, float cy, float cz, int sides, float radius, float height, int subdivisions) {
    if (!gd || sides < 3) return;
    auto loop = geo::LoopFactory::regularPolygon(0, 0, sides, radius);
    geo::SolidFactory::extrude(*gd, loop, height, subdivisions);
}

void solid_factory_extrude_star(geo::GeoData* gd, float cx, float cy, float cz, int points, float outerRadius, float innerRadius, float height, int subdivisions) {
    if (!gd || points < 3) return;
    auto loop = geo::LoopFactory::star(0, 0, points, outerRadius, innerRadius);
    geo::SolidFactory::extrude(*gd, loop, height, subdivisions);
}

void solid_factory_extrude_tapered_circle(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, float topScale, int samples, int subdivisions) {
    if (!gd) return;
    auto loop = geo::LoopFactory::circle(0, 0, radius, samples);
    geo::SolidFactory::extrudeTapered(*gd, loop, height, topScale, subdivisions);
}

void solid_factory_extrude_twisted_circle(geo::GeoData* gd, float cx, float cy, float cz, float radius, float height, float twistAngle, int samples, int subdivisions) {
    if (!gd) return;
    auto loop = geo::LoopFactory::circle(0, 0, radius, samples);
    geo::SolidFactory::extrudeTwisted(*gd, loop, height, twistAngle, subdivisions);
}

void solid_factory_revolve_rect(geo::GeoData* gd, float cx, float cy, float cz, float width, float height, float angle, int segments) {
    if (!gd) return;
    // 创建一个矩形轮廓用于旋转
    auto loop = geo::LoopFactory::rectangle(0, 0, width, height);
    geo::SolidFactory::revolve(*gd, loop, angle, segments);
}

void solid_factory_spring(geo::GeoData* gd, float cx, float cy, float cz, float radius, float wireRadius, float height, float turns, int segments, int wireSegments) {
    if (!gd) return;
    geo::SolidFactory::spring(*gd, radius, wireRadius, height, turns, segments, wireSegments);
}

void solid_factory_gear(geo::GeoData* gd, float cx, float cy, float cz, int teeth, float outerRadius, float innerRadius, float thickness, float toothDepth) {
    if (!gd || teeth < 3) return;
    geo::SolidFactory::gear(*gd, teeth, outerRadius, innerRadius, thickness, toothDepth);
}

// ============================================================
//  符号注册
// ============================================================

#include <libtcc.h>

namespace tcc_engine {

void capi_register_symbols(TCCState* s) {
    // 常量
    tcc_add_symbol(s, "CAPI_PI", &CAPI_PI);
    tcc_add_symbol(s, "CAPI_E",  &CAPI_E);

    // entity_ 属性访问
    tcc_add_symbol(s, "entity_getFloat", (void*)entity_getFloat);
    tcc_add_symbol(s, "entity_getInt",   (void*)entity_getInt);
    tcc_add_symbol(s, "entity_getBool",  (void*)entity_getBool);
    tcc_add_symbol(s, "entity_hasAttr",  (void*)entity_hasAttr);
    tcc_add_symbol(s, "entity_getName",  (void*)entity_getName);
    tcc_add_symbol(s, "entity_getId",    (void*)entity_getId);

    // geo_ 几何输出
    tcc_add_symbol(s, "geo_addVertex",   (void*)geo_addVertex);
    tcc_add_symbol(s, "geo_addEdge",     (void*)geo_addEdge);
    tcc_add_symbol(s, "geo_addTriangle", (void*)geo_addTriangle);

    // math_
    tcc_add_symbol(s, "math_sin",    (void*)math_sin);
    tcc_add_symbol(s, "math_cos",    (void*)math_cos);
    tcc_add_symbol(s, "math_tan",    (void*)math_tan);
    tcc_add_symbol(s, "math_asin",   (void*)math_asin);
    tcc_add_symbol(s, "math_acos",   (void*)math_acos);
    tcc_add_symbol(s, "math_atan",   (void*)math_atan);
    tcc_add_symbol(s, "math_atan2",  (void*)math_atan2);
    tcc_add_symbol(s, "math_sqrt",   (void*)math_sqrt);
    tcc_add_symbol(s, "math_pow",    (void*)math_pow);
    tcc_add_symbol(s, "math_exp",    (void*)math_exp);
    tcc_add_symbol(s, "math_log",    (void*)math_log);
    tcc_add_symbol(s, "math_log2",   (void*)math_log2);
    tcc_add_symbol(s, "math_log10",  (void*)math_log10);
    tcc_add_symbol(s, "math_abs",    (void*)math_abs);
    tcc_add_symbol(s, "math_floor",  (void*)math_floor);
    tcc_add_symbol(s, "math_ceil",   (void*)math_ceil);
    tcc_add_symbol(s, "math_round",  (void*)math_round);
    tcc_add_symbol(s, "math_fmod",   (void*)math_fmod);
    tcc_add_symbol(s, "math_clamp",  (void*)math_clamp);
    tcc_add_symbol(s, "math_lerp",   (void*)math_lerp);
    tcc_add_symbol(s, "math_min2",   (void*)math_min2);
    tcc_add_symbol(s, "math_max2",   (void*)math_max2);
    tcc_add_symbol(s, "math_min3",   (void*)math_min3);
    tcc_add_symbol(s, "math_max3",   (void*)math_max3);
    tcc_add_symbol(s, "math_deg2rad",(void*)math_deg2rad);
    tcc_add_symbol(s, "math_rad2deg",(void*)math_rad2deg);

    // curve_
    tcc_add_symbol(s, "curve_line",     (void*)curve_line);
    tcc_add_symbol(s, "curve_polyline", (void*)curve_polyline);
    tcc_add_symbol(s, "curve_arc",      (void*)curve_arc);
    tcc_add_symbol(s, "curve_circle",   (void*)curve_circle);
    tcc_add_symbol(s, "curve_bezier",   (void*)curve_bezier);
    tcc_add_symbol(s, "curve_catmull",  (void*)curve_catmull);

    // loop_
    tcc_add_symbol(s, "loop_rect",      (void*)loop_rect);
    tcc_add_symbol(s, "loop_polygon",   (void*)loop_polygon);
    tcc_add_symbol(s, "loop_circle",    (void*)loop_circle);
    tcc_add_symbol(s, "loop_ellipse",   (void*)loop_ellipse);
    tcc_add_symbol(s, "loop_roundrect", (void*)loop_roundrect);
    tcc_add_symbol(s, "loop_union",     (void*)loop_union);
    tcc_add_symbol(s, "loop_intersect", (void*)loop_intersect);
    tcc_add_symbol(s, "loop_difference",(void*)loop_difference);
    tcc_add_symbol(s, "loop_xor",       (void*)loop_xor);
    tcc_add_symbol(s, "loop_offset",    (void*)loop_offset);
    tcc_add_symbol(s, "loop_fill",      (void*)loop_fill);
    tcc_add_symbol(s, "loop_fill_with_holes", (void*)loop_fill_with_holes);
    tcc_add_symbol(s, "loop_simplify",  (void*)loop_simplify);

    // path_
    tcc_add_symbol(s, "path_line",   (void*)path_line);
    tcc_add_symbol(s, "path_arc",    (void*)path_arc);
    tcc_add_symbol(s, "path_spline", (void*)path_spline);

    // surface_
    tcc_add_symbol(s, "surface_plane", (void*)surface_plane);
    tcc_add_symbol(s, "surface_disk",  (void*)surface_disk);
    tcc_add_symbol(s, "surface_fill",  (void*)surface_fill);

    // solid_
    tcc_add_symbol(s, "solid_box",      (void*)solid_box);
    tcc_add_symbol(s, "solid_sphere",   (void*)solid_sphere);
    tcc_add_symbol(s, "solid_cylinder", (void*)solid_cylinder);
    tcc_add_symbol(s, "solid_cone",     (void*)solid_cone);
    tcc_add_symbol(s, "solid_torus",    (void*)solid_torus);
    tcc_add_symbol(s, "solid_capsule",  (void*)solid_capsule);

    // curve_factory_
    tcc_add_symbol(s, "curve_factory_line",           (void*)curve_factory_line);
    tcc_add_symbol(s, "curve_factory_arc",            (void*)curve_factory_arc);
    tcc_add_symbol(s, "curve_factory_circle",         (void*)curve_factory_circle);
    tcc_add_symbol(s, "curve_factory_ellipse_arc",    (void*)curve_factory_ellipse_arc);
    tcc_add_symbol(s, "curve_factory_ellipse",        (void*)curve_factory_ellipse);
    tcc_add_symbol(s, "curve_factory_quadratic_bezier", (void*)curve_factory_quadratic_bezier);
    tcc_add_symbol(s, "curve_factory_cubic_bezier",   (void*)curve_factory_cubic_bezier);
    tcc_add_symbol(s, "curve_factory_helix",          (void*)curve_factory_helix);
    tcc_add_symbol(s, "curve_factory_conic_helix",    (void*)curve_factory_conic_helix);
    tcc_add_symbol(s, "curve_factory_sine_wave",      (void*)curve_factory_sine_wave);

    // loop_factory_
    tcc_add_symbol(s, "loop_factory_rect",            (void*)loop_factory_rect);
    tcc_add_symbol(s, "loop_factory_square",          (void*)loop_factory_square);
    tcc_add_symbol(s, "loop_factory_circle",          (void*)loop_factory_circle);
    tcc_add_symbol(s, "loop_factory_ellipse",         (void*)loop_factory_ellipse);
    tcc_add_symbol(s, "loop_factory_rounded_rect",    (void*)loop_factory_rounded_rect);
    tcc_add_symbol(s, "loop_factory_regular_polygon", (void*)loop_factory_regular_polygon);
    tcc_add_symbol(s, "loop_factory_star",            (void*)loop_factory_star);
    tcc_add_symbol(s, "loop_factory_gear",            (void*)loop_factory_gear);
    tcc_add_symbol(s, "loop_factory_arc_segment",     (void*)loop_factory_arc_segment);

    // solid_factory_
    tcc_add_symbol(s, "solid_factory_box",            (void*)solid_factory_box);
    tcc_add_symbol(s, "solid_factory_sphere",         (void*)solid_factory_sphere);
    tcc_add_symbol(s, "solid_factory_cylinder",       (void*)solid_factory_cylinder);
    tcc_add_symbol(s, "solid_factory_cone",           (void*)solid_factory_cone);
    tcc_add_symbol(s, "solid_factory_cone_frustum",   (void*)solid_factory_cone_frustum);
    tcc_add_symbol(s, "solid_factory_torus",          (void*)solid_factory_torus);
    tcc_add_symbol(s, "solid_factory_capsule",        (void*)solid_factory_capsule);
    tcc_add_symbol(s, "solid_factory_extrude_rect",   (void*)solid_factory_extrude_rect);
    tcc_add_symbol(s, "solid_factory_extrude_circle", (void*)solid_factory_extrude_circle);
    tcc_add_symbol(s, "solid_factory_extrude_polygon",(void*)solid_factory_extrude_polygon);
    tcc_add_symbol(s, "solid_factory_extrude_star",   (void*)solid_factory_extrude_star);
    tcc_add_symbol(s, "solid_factory_extrude_tapered_circle", (void*)solid_factory_extrude_tapered_circle);
    tcc_add_symbol(s, "solid_factory_extrude_twisted_circle", (void*)solid_factory_extrude_twisted_circle);
    tcc_add_symbol(s, "solid_factory_revolve_rect",   (void*)solid_factory_revolve_rect);
    tcc_add_symbol(s, "solid_factory_spring",         (void*)solid_factory_spring);
    tcc_add_symbol(s, "solid_factory_gear",           (void*)solid_factory_gear);
}

} // namespace tcc_engine
