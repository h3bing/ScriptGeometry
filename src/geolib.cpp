/**
 * @file geolib.cpp
 * @brief ScriptGeometry 几何库实现
 */

#include "geolib.h"

#include <cmath>
#include <stdexcept>
#include <sstream>
#include <fstream>

// Clipper2 和 earcut
#include <clipper2/clipper.h>
#include <earcut.hpp>

// JSON序列化
#include <nlohmann/json.hpp>

namespace geo {

// ============================================================
//  GeoData 辅助
// ============================================================

std::vector<Point3> Loop::samplePoints() const {
    std::vector<Point3> pts;
    for (const auto& c : curves) {
        for (const auto& p : c.points)
            pts.push_back(p);
    }
    return pts;
}

BoundBox Loop::boundBox() const {
    BoundBox bb;
    for (const auto& p : samplePoints()) bb.expand(p);
    return bb;
}

bool Loop::isClosed() const {
    auto pts = samplePoints();
    if (pts.size() < 3) return false;
    return glm::distance(pts.front(), pts.back()) < EPS;
}

std::vector<Point2> Loop::toPath2D() const {
    std::vector<Point2> pts2d;
    for (const auto& pt : samplePoints()) {
        pts2d.emplace_back(pt.x, pt.y);
    }
    return pts2d;
}

Loop Loop::fromPath2D(const std::vector<Point2>& pts) {
    Loop loop;
    Curve c;
    for (const auto& p : pts) {
        c.points.emplace_back(p.x, p.y, 0.f);
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop Loop::fromFloatArray(const float* pts, int count) {
    Loop loop;
    Curve c;
    for (int i = 0; i < count; ++i) {
        c.points.emplace_back(pts[i*2], pts[i*2+1], 0.f);
    }
    loop.addCurve(std::move(c));
    return loop;
}

std::vector<Point3> Path::samplePoints() const {
    std::vector<Point3> pts;
    for (size_t i = 0; i < curves.size(); ++i) {
        const auto& c = curves[i];
        size_t start = (i == 0) ? 0 : 1;
        for (size_t j = start; j < c.points.size(); ++j)
            pts.push_back(c.points[j]);
    }
    return pts;
}

Point3 Path::pointAt(float t) const {
    auto pts = samplePoints();
    if (pts.empty()) return {0,0,0};
    if (pts.size() == 1) return pts[0];

    float totalLen = length();
    if (totalLen < EPS) return pts[0];

    float targetLen = t * totalLen;
    float curLen = 0.f;

    for (size_t i = 1; i < pts.size(); ++i) {
        float segLen = glm::distance(pts[i-1], pts[i]);
        if (curLen + segLen >= targetLen) {
            float segT = (targetLen - curLen) / segLen;
            return glm::mix(pts[i-1], pts[i], segT);
        }
        curLen += segLen;
    }
    return pts.back();
}

Vector3 Path::tangentAt(float t) const {
    float eps = 0.001f;
    Point3 p0 = pointAt(std::max(0.f, t - eps));
    Point3 p1 = pointAt(std::min(1.f, t + eps));
    Vector3 tangent = p1 - p0;
    float len = glm::length(tangent);
    return len > EPS ? tangent / len : Vector3(1, 0, 0);
}

float Path::length() const {
    float len = 0.f;
    auto pts = samplePoints();
    for (size_t i = 1; i < pts.size(); ++i) {
        len += glm::distance(pts[i-1], pts[i]);
    }
    return len;
}

void Loop::triangulate(std::vector<TriangleIndex>& outTriangles) const {
    using Point = std::array<double, 2>;

    auto pts2d = toPath2D();
    if (pts2d.size() < 3) return;

    std::vector<std::vector<Point>> polygon;
    std::vector<Point> contour;
    for (const auto& p : pts2d) {
        contour.push_back({p.x, p.y});
    }
    polygon.push_back(contour);

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    for (size_t i = 0; i < indices.size(); i += 3) {
        outTriangles.push_back({indices[i], indices[i+1], indices[i+2]});
    }
}

// ============================================================
//  布尔运算 (Clipper2)
// ============================================================

namespace {

using namespace Clipper2Lib;

PathD loopToPathD(const Loop& loop) {
    PathD path;
    auto pts = loop.samplePoints();
    path.reserve(pts.size());
    for (const auto& p : pts) {
        path.push_back(PointD(p.x, p.y));
    }
    return path;
}

PathsD loopToPathsD(const Loop& loop) {
    PathsD paths;
    paths.push_back(loopToPathD(loop));
    return paths;
}

Loop pathDToLoop(const PathD& path) {
    Loop loop;
    Curve c;
    for (const auto& pt : path) {
        c.points.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y), 0.f);
    }
    loop.addCurve(std::move(c));
    return loop;
}

std::vector<Loop> pathsDToLoops(const PathsD& paths) {
    std::vector<Loop> loops;
    for (const auto& path : paths) {
        if (path.size() >= 3) {
            loops.push_back(pathDToLoop(path));
        }
    }
    return loops;
}

} // anonymous namespace

std::vector<Loop> Loop::booleanUnion(const Loop& other) const {
    PathsD result = Union(loopToPathsD(*this), loopToPathsD(other), FillRule::NonZero);
    return pathsDToLoops(result);
}

std::vector<Loop> Loop::booleanIntersect(const Loop& other) const {
    PathsD result = Intersect(loopToPathsD(*this), loopToPathsD(other), FillRule::NonZero);
    return pathsDToLoops(result);
}

std::vector<Loop> Loop::booleanDifference(const Loop& other) const {
    PathsD result = Difference(loopToPathsD(*this), loopToPathsD(other), FillRule::NonZero);
    return pathsDToLoops(result);
}

std::vector<Loop> Loop::booleanXor(const Loop& other) const {
    PathsD result = Clipper2Lib::Xor(loopToPathsD(*this), loopToPathsD(other), FillRule::NonZero);
    return pathsDToLoops(result);
}

std::vector<Loop> Loop::offset(float delta, JoinType joinType) const {
    PathsD paths = loopToPathsD(*this);

    Clipper2Lib::JoinType jt = Clipper2Lib::JoinType::Round;
    switch (joinType) {
        case JoinType::Square: jt = Clipper2Lib::JoinType::Square; break;
        case JoinType::Miter:  jt = Clipper2Lib::JoinType::Miter; break;
        default: break;
    }

    PathsD result = InflatePaths(paths, delta, jt, Clipper2Lib::EndType::Polygon);
    return pathsDToLoops(result);
}

// ============================================================
//  CurveFactory 实现
// ============================================================

namespace CurveFactory {

Curve line(const Point3& start, const Point3& end) {
    Curve c;
    c.addPoint(start);
    c.addPoint(end);
    return c;
}

Curve line(float x0, float y0, float z0, float x1, float y1, float z1) {
    return line({x0, y0, z0}, {x1, y1, z1});
}

Curve arc(const Point3& center, float radius, float startAngle, float endAngle, int samples) {
    Curve c;
    float step = (endAngle - startAngle) / samples;
    for (int i = 0; i <= samples; ++i) {
        float a = startAngle + step * i;
        c.addPoint(
            center.x + radius * std::cos(a),
            center.y + radius * std::sin(a),
            center.z
        );
    }
    return c;
}

Curve arc(float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples) {
    return arc({cx, cy, cz}, r, startAngle, endAngle, samples);
}

Curve circle(const Point3& center, float radius, int samples) {
    return arc(center, radius, 0.f, 2.f * PI, samples);
}

Curve circle(float cx, float cy, float cz, float r, int samples) {
    return circle({cx, cy, cz}, r, samples);
}

Curve ellipseArc(const Point3& center, float rx, float ry, float startAngle, float endAngle, int samples) {
    Curve c;
    float step = (endAngle - startAngle) / samples;
    for (int i = 0; i <= samples; ++i) {
        float a = startAngle + step * i;
        c.addPoint(
            center.x + rx * std::cos(a),
            center.y + ry * std::sin(a),
            center.z
        );
    }
    return c;
}

Curve ellipse(const Point3& center, float rx, float ry, int samples) {
    return ellipseArc(center, rx, ry, 0.f, 2.f * PI, samples);
}

Curve quadraticBezier(const Point3& p0, const Point3& p1, const Point3& p2, int samples) {
    Curve c;
    for (int i = 0; i <= samples; ++i) {
        float t = (float)i / samples;
        float mt = 1.f - t;
        Point3 p = mt * mt * p0 + 2.f * mt * t * p1 + t * t * p2;
        c.addPoint(p);
    }
    return c;
}

Curve cubicBezier(const Point3& p0, const Point3& p1, const Point3& p2, const Point3& p3, int samples) {
    Curve c;
    for (int i = 0; i <= samples; ++i) {
        float t = (float)i / samples;
        float mt = 1.f - t;
        Point3 p = mt * mt * mt * p0 + 3.f * mt * mt * t * p1 + 3.f * mt * t * t * p2 + t * t * t * p3;
        c.addPoint(p);
    }
    return c;
}

Curve bezier(const std::vector<Point3>& controlPoints, int samples) {
    if (controlPoints.size() < 2) return Curve();

    size_t n = controlPoints.size() - 1;
    Curve c;

    // De Casteljau算法
    auto evaluate = [&](float t) -> Point3 {
        std::vector<Point3> tmp = controlPoints;
        for (size_t k = 1; k <= n; ++k) {
            for (size_t i = 0; i <= n - k; ++i) {
                tmp[i] = (1.f - t) * tmp[i] + t * tmp[i + 1];
            }
        }
        return tmp[0];
    };

    for (int i = 0; i <= samples; ++i) {
        c.addPoint(evaluate((float)i / samples));
    }
    return c;
}

Curve catmullRom(const std::vector<Point3>& controlPoints, int samples) {
    if (controlPoints.size() < 2) return Curve();

    Curve c;
    size_t n = controlPoints.size();

    auto eval = [&](int seg, float t) -> Point3 {
        int i0 = std::max(0, seg - 1);
        int i1 = seg;
        int i2 = std::min((int)n - 1, seg + 1);
        int i3 = std::min((int)n - 1, seg + 2);

        const auto& p0 = controlPoints[i0];
        const auto& p1 = controlPoints[i1];
        const auto& p2 = controlPoints[i2];
        const auto& p3 = controlPoints[i3];

        float t2 = t * t, t3 = t2 * t;
        return 0.5f * ((2.f * p1) +
                       (-p0 + p2) * t +
                       (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                       (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
    };

    for (size_t seg = 0; seg < n - 1; ++seg) {
        for (int i = 0; i <= samples; ++i) {
            float t = (float)i / samples;
            c.addPoint(eval((int)seg, t));
        }
    }
    return c;
}

Curve helix(const Point3& center, float radius, float height, float turns, int samples) {
    Curve c;
    float totalAngle = 2.f * PI * turns;
    float step = totalAngle / samples;

    for (int i = 0; i <= samples; ++i) {
        float a = step * i;
        float t = (float)i / samples;
        c.addPoint(
            center.x + radius * std::cos(a),
            center.y + t * height,
            center.z + radius * std::sin(a)
        );
    }
    return c;
}

Curve conicHelix(const Point3& center, float startRadius, float endRadius, float height, float turns, int samples) {
    Curve c;
    float totalAngle = 2.f * PI * turns;
    float step = totalAngle / samples;

    for (int i = 0; i <= samples; ++i) {
        float a = step * i;
        float t = (float)i / samples;
        float r = startRadius + t * (endRadius - startRadius);
        c.addPoint(
            center.x + r * std::cos(a),
            center.y + t * height,
            center.z + r * std::sin(a)
        );
    }
    return c;
}

Curve sineWave(const Point3& start, const Point3& end, float amplitude, float frequency, int samples) {
    Curve c;
    Vector3 dir = end - start;
    float len = glm::length(dir);
    if (len < EPS) return c;

    Vector3 forward = dir / len;
    Vector3 up(0, 1, 0);
    Vector3 right = glm::normalize(glm::cross(up, forward));
    if (glm::length(right) < EPS) right = glm::normalize(glm::cross(Vector3(0, 0, 1), forward));

    for (int i = 0; i <= samples; ++i) {
        float t = (float)i / samples;
        float wave = amplitude * std::sin(2.f * PI * frequency * t);
        Point3 p = start + t * dir + wave * right;
        c.addPoint(p);
    }
    return c;
}

Curve polyline(const std::vector<Point3>& points) {
    Curve c;
    for (const auto& p : points) c.addPoint(p);
    return c;
}

} // namespace CurveFactory

// ============================================================
//  LoopFactory 实现
// ============================================================

namespace LoopFactory {

Loop rectangle(float width, float height) {
    return rectangle(0, 0, width, height);
}

Loop rectangle(float cx, float cy, float width, float height) {
    float hw = width * 0.5f, hh = height * 0.5f;
    Loop loop;
    Curve c;
    c.addPoint(cx - hw, cy - hh, 0);
    c.addPoint(cx + hw, cy - hh, 0);
    c.addPoint(cx + hw, cy + hh, 0);
    c.addPoint(cx - hw, cy + hh, 0);
    c.addPoint(cx - hw, cy - hh, 0); // 闭合
    loop.addCurve(std::move(c));
    return loop;
}

Loop square(float size) {
    return rectangle(size, size);
}

Loop square(float cx, float cy, float size) {
    return rectangle(cx, cy, size, size);
}

Loop circle(float radius, int samples) {
    return circle(0, 0, radius, samples);
}

Loop circle(float cx, float cy, float radius, int samples) {
    Loop loop;
    Curve c;
    float step = 2.f * PI / samples;
    for (int i = 0; i <= samples; ++i) {
        float a = step * i;
        c.addPoint(cx + radius * std::cos(a), cy + radius * std::sin(a), 0);
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop ellipse(float rx, float ry, int samples) {
    return ellipse(0, 0, rx, ry, samples);
}

Loop ellipse(float cx, float cy, float rx, float ry, int samples) {
    Loop loop;
    Curve c;
    float step = 2.f * PI / samples;
    for (int i = 0; i <= samples; ++i) {
        float a = step * i;
        c.addPoint(cx + rx * std::cos(a), cy + ry * std::sin(a), 0);
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop roundedRectangle(float width, float height, float radius, int cornerSamples) {
    return roundedRectangle(0, 0, width, height, radius, cornerSamples);
}

Loop roundedRectangle(float cx, float cy, float width, float height, float radius, int cornerSamples) {
    float hw = width * 0.5f - radius;
    float hh = height * 0.5f - radius;

    Loop loop;
    Curve c;

    // 四个角点
    float corners[4][2] = {
        {cx + hw, cy + hh}, {cx - hw, cy + hh},
        {cx - hw, cy - hh}, {cx + hw, cy - hh}
    };
    float startAngles[4] = {0, PI/2, PI, PI*1.5f};

    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= cornerSamples; ++i) {
            float a = startAngles[corner] + (PI * 0.5f) * i / cornerSamples;
            c.addPoint(
                corners[corner][0] + radius * std::cos(a),
                corners[corner][1] + radius * std::sin(a), 0
            );
        }
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop regularPolygon(int sides, float radius) {
    return regularPolygon(0, 0, sides, radius);
}

Loop regularPolygon(float cx, float cy, int sides, float radius) {
    Loop loop;
    Curve c;
    float step = 2.f * PI / sides;
    for (int i = 0; i <= sides; ++i) {
        float a = step * i;
        c.addPoint(cx + radius * std::cos(a), cy + radius * std::sin(a), 0);
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop star(int points, float outerRadius, float innerRadius) {
    return star(0, 0, points, outerRadius, innerRadius);
}

Loop star(float cx, float cy, int points, float outerRadius, float innerRadius) {
    Loop loop;
    Curve c;
    int totalPoints = points * 2;
    float step = 2.f * PI / totalPoints;

    for (int i = 0; i <= totalPoints; ++i) {
        float a = step * i - PI / 2;  // 从顶部开始
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        c.addPoint(cx + r * std::cos(a), cy + r * std::sin(a), 0);
    }
    loop.addCurve(std::move(c));
    return loop;
}

Loop gear(int teeth, float outerRadius, float innerRadius, float toothDepth) {
    Loop loop;
    Curve c;

    float toothAngle = 2.f * PI / teeth;
    float halfTooth = toothAngle * 0.25f;

    for (int i = 0; i < teeth; ++i) {
        float baseAngle = i * toothAngle;

        // 齿根
        float a0 = baseAngle;
        c.addPoint(innerRadius * std::cos(a0), innerRadius * std::sin(a0), 0);

        // 齿上升
        float a1 = baseAngle + halfTooth;
        c.addPoint(outerRadius * std::cos(a1), outerRadius * std::sin(a1), 0);

        // 齿顶
        float a2 = baseAngle + toothAngle * 0.5f;
        c.addPoint((outerRadius + toothDepth) * std::cos(a2), (outerRadius + toothDepth) * std::sin(a2), 0);

        // 齿下降
        float a3 = baseAngle + toothAngle - halfTooth;
        c.addPoint(outerRadius * std::cos(a3), outerRadius * std::sin(a3), 0);
    }
    // 闭合
    c.addPoint(c.points.front());
    loop.addCurve(std::move(c));
    return loop;
}

Loop arcSegment(float cx, float cy, float innerRadius, float outerRadius, float startAngle, float endAngle, int samples) {
    Loop loop;
    Curve c;

    // 外弧
    float step = (endAngle - startAngle) / samples;
    for (int i = 0; i <= samples; ++i) {
        float a = startAngle + step * i;
        c.addPoint(cx + outerRadius * std::cos(a), cy + outerRadius * std::sin(a), 0);
    }

    // 内弧（反向）
    for (int i = samples; i >= 0; --i) {
        float a = startAngle + step * i;
        c.addPoint(cx + innerRadius * std::cos(a), cy + innerRadius * std::sin(a), 0);
    }

    loop.addCurve(std::move(c));
    return loop;
}

Loop fromPoints(const std::vector<Point2>& points) {
    Loop loop;
    Curve c;
    for (const auto& p : points) {
        c.addPoint(p.x, p.y, 0);
    }
    if (!points.empty()) c.addPoint(points.front().x, points.front().y, 0); // 闭合
    loop.addCurve(std::move(c));
    return loop;
}

Loop fromPoints(const std::vector<Point3>& points) {
    Loop loop;
    Curve c;
    for (const auto& p : points) {
        c.addPoint(p);
    }
    if (!points.empty()) c.addPoint(points.front());
    loop.addCurve(std::move(c));
    return loop;
}

Loop fromFloatArray(const float* pts, int count) {
    Loop loop;
    Curve c;
    for (int i = 0; i < count; ++i) {
        c.addPoint(pts[i * 2], pts[i * 2 + 1], 0);
    }
    if (count > 0) c.addPoint(pts[0], pts[1], 0);
    loop.addCurve(std::move(c));
    return loop;
}

std::vector<Loop> booleanUnion(const Loop& a, const Loop& b) {
    return a.booleanUnion(b);
}

std::vector<Loop> booleanIntersect(const Loop& a, const Loop& b) {
    return a.booleanIntersect(b);
}

std::vector<Loop> booleanDifference(const Loop& a, const Loop& b) {
    return a.booleanDifference(b);
}

std::vector<Loop> booleanXor(const Loop& a, const Loop& b) {
    return a.booleanXor(b);
}

std::vector<Loop> offset(const Loop& loop, float delta, JoinType joinType) {
    return loop.offset(delta, joinType);
}

} // namespace LoopFactory

// ============================================================
//  PathFactory 实现
// ============================================================

namespace PathFactory {

Path line(const Point3& start, const Point3& end) {
    Path p;
    p.addCurve(CurveFactory::line(start, end));
    return p;
}

Path arc(const Point3& center, float radius, float startAngle, float endAngle, int samples) {
    Path p;
    p.addCurve(CurveFactory::arc(center, radius, startAngle, endAngle, samples));
    return p;
}

Path spline(const std::vector<Point3>& controlPoints, int samples) {
    Path p;
    p.addCurve(CurveFactory::catmullRom(controlPoints, samples));
    return p;
}

Path helix(const Point3& center, float radius, float height, float turns, int samples) {
    Path p;
    p.addCurve(CurveFactory::helix(center, radius, height, turns, samples));
    return p;
}

Path fromCurves(const std::vector<Curve>& curves) {
    Path p;
    for (const auto& c : curves) p.addCurve(c);
    return p;
}

} // namespace PathFactory

// ============================================================
//  SolidFactory 实现
// ============================================================

namespace SolidFactory {

namespace {

// 辅助：添加顶点并返回索引
inline uint32_t addVert(GeoData& geo, const Point3& pos, const Vector3& normal = {0, 0, 1}, const Vector2& uv = {0, 0}) {
    return geo.addVertex(pos, normal, uv);
}

// 辅助：计算法线
Vector3 computeNormal(const Point3& a, const Point3& b, const Point3& c) {
    return glm::normalize(glm::cross(b - a, c - a));
}

} // anonymous namespace

// --------------------------------------------------------
//  基础实体
// --------------------------------------------------------

void box(GeoData& geo, float width, float height, float depth) {
    box(geo, 0, 0, 0, width, height, depth);
}

void box(GeoData& geo, float cx, float cy, float cz, float width, float height, float depth) {
    float hx = width * 0.5f, hy = height * 0.5f, hz = depth * 0.5f;

    Point3 corners[8] = {
        {cx-hx, cy-hy, cz-hz}, {cx+hx, cy-hy, cz-hz},
        {cx+hx, cy+hy, cz-hz}, {cx-hx, cy+hy, cz-hz},
        {cx-hx, cy-hy, cz+hz}, {cx+hx, cy-hy, cz+hz},
        {cx+hx, cy+hy, cz+hz}, {cx-hx, cy+hy, cz+hz}
    };

    Vector3 normals[6] = {{0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}};
    int faces[6][4] = {
        {0,1,2,3}, {7,6,5,4}, {0,3,7,4}, {1,5,6,2}, {0,4,5,1}, {3,2,6,7}
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t idx[4];
        for (int k = 0; k < 4; ++k) {
            idx[k] = addVert(geo, corners[faces[f][k]], normals[f]);
        }
        geo.addQuad(idx[0], idx[1], idx[2], idx[3]);
    }
}

void sphere(GeoData& geo, float radius, int rings, int sectors) {
    sphere(geo, 0, 0, 0, radius, rings, sectors);
}

void sphere(GeoData& geo, float cx, float cy, float cz, float radius, int rings, int sectors) {
    std::vector<std::vector<uint32_t>> grid(rings + 1, std::vector<uint32_t>(sectors + 1));

    for (int ri = 0; ri <= rings; ++ri) {
        float phi = PI * ri / rings;
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * PI * si / sectors;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            grid[ri][si] = addVert(geo,
                {cx + radius*x, cy + radius*y, cz + radius*z},
                {x, y, z},
                {(float)si / sectors, (float)ri / rings}
            );
        }
    }

    for (int ri = 0; ri < rings; ++ri) {
        for (int si = 0; si < sectors; ++si) {
            geo.addQuad(grid[ri][si], grid[ri][si+1], grid[ri+1][si+1], grid[ri+1][si]);
        }
    }
}

void cylinder(GeoData& geo, float radius, float height, int sectors, int rings) {
    cylinder(geo, 0, 0, 0, radius, height, sectors, rings);
}

void cylinder(GeoData& geo, float cx, float cy, float cz, float radius, float height, int sectors, int rings) {
    float halfH = height * 0.5f;

    std::vector<std::vector<uint32_t>> grid(rings + 1, std::vector<uint32_t>(sectors + 1));

    for (int ri = 0; ri <= rings; ++ri) {
        float y = -halfH + height * ri / rings;
        for (int si = 0; si <= sectors; ++si) {
            float a = 2.f * PI * si / sectors;
            float nx = std::cos(a), nz = std::sin(a);
            grid[ri][si] = addVert(geo,
                {cx + radius*nx, cy + y, cz + radius*nz},
                {nx, 0, nz},
                {(float)si / sectors, (float)ri / rings}
            );
        }
    }

    for (int ri = 0; ri < rings; ++ri) {
        for (int si = 0; si < sectors; ++si) {
            geo.addQuad(grid[ri][si], grid[ri][si+1], grid[ri+1][si+1], grid[ri+1][si]);
        }
    }

    // 顶底盖
    uint32_t botCenter = addVert(geo, {cx, cy - halfH, cz}, {0, -1, 0});
    uint32_t topCenter = addVert(geo, {cx, cy + halfH, cz}, {0, 1, 0});

    for (int si = 0; si < sectors; ++si) {
        float a0 = 2.f * PI * si / sectors;
        float a1 = 2.f * PI * (si+1) / sectors;
        uint32_t b0 = addVert(geo, {cx + radius*std::cos(a0), cy - halfH, cz + radius*std::sin(a0)}, {0, -1, 0});
        uint32_t b1 = addVert(geo, {cx + radius*std::cos(a1), cy - halfH, cz + radius*std::sin(a1)}, {0, -1, 0});
        uint32_t t0 = addVert(geo, {cx + radius*std::cos(a0), cy + halfH, cz + radius*std::sin(a0)}, {0, 1, 0});
        uint32_t t1 = addVert(geo, {cx + radius*std::cos(a1), cy + halfH, cz + radius*std::sin(a1)}, {0, 1, 0});

        geo.addTriangle(botCenter, b1, b0);
        geo.addTriangle(topCenter, t0, t1);
    }
}

void cone(GeoData& geo, const Loop& baseLoop, float height) {
    auto pts = baseLoop.samplePoints();
    if (pts.size() < 3) return;

    BoundBox bb = baseLoop.boundBox();
    Point3 apex(bb.center().x, bb.center().y + height, bb.center().z);

    uint32_t apexIdx = addVert(geo, apex, {0, 1, 0});

    // 计算每个侧面的法线
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t next = (i + 1) % pts.size();
        Point3 p0 = pts[i];
        Point3 p1 = pts[next];

        Vector3 normal = computeNormal(apex, p0, p1);

        uint32_t i0 = addVert(geo, p0, normal);
        uint32_t i1 = addVert(geo, p1, normal);

        geo.addTriangle(apexIdx, i0, i1);
    }

    // 底面
    uint32_t baseCenter = addVert(geo, bb.center(), {0, -1, 0});
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t next = (i + 1) % pts.size();
        uint32_t i0 = addVert(geo, pts[i], {0, -1, 0});
        uint32_t i1 = addVert(geo, pts[next], {0, -1, 0});
        geo.addTriangle(baseCenter, i1, i0);
    }
}

void cone(GeoData& geo, float baseRadius, float height, int sectors) {
    cone(geo, 0, 0, 0, baseRadius, height, sectors);
}

void cone(GeoData& geo, float cx, float cy, float cz, float baseRadius, float height, int sectors) {
    Loop baseLoop = LoopFactory::circle(cx, cz, baseRadius, sectors);
    // 转换Y/Z
    Loop converted;
    for (auto& c : baseLoop.curves) {
        Curve newC;
        for (auto& p : c.points) {
            newC.addPoint(p.x, cy, p.y);
        }
        converted.addCurve(std::move(newC));
    }
    cone(geo, converted, height);
}

void coneFrustum(GeoData& geo, const Loop& bottomLoop, const Loop& topLoop, float height) {
    auto bottomPts = bottomLoop.samplePoints();
    auto topPts = topLoop.samplePoints();

    if (bottomPts.size() != topPts.size() || bottomPts.size() < 3) return;

    size_t n = bottomPts.size() - 1; // 去掉闭合点

    // 计算高度方向
    BoundBox bottomBB = bottomLoop.boundBox();
    Point3 bottomCenter = bottomBB.center();
    Point3 topCenter = topLoop.boundBox().center();
    Vector3 upDir(0, 1, 0); // 简化处理

    // 侧面
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;

        Vector3 normal = computeNormal(bottomPts[i], topPts[i], topPts[next]);

        uint32_t b0 = addVert(geo, bottomPts[i], normal);
        uint32_t b1 = addVert(geo, bottomPts[next], normal);
        uint32_t t0 = addVert(geo, topPts[i], normal);
        uint32_t t1 = addVert(geo, topPts[next], normal);

        geo.addQuad(b0, b1, t1, t0);
    }

    // 底面
    uint32_t bottomCenterIdx = addVert(geo, bottomCenter, {0, -1, 0});
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;
        uint32_t i0 = addVert(geo, bottomPts[i], {0, -1, 0});
        uint32_t i1 = addVert(geo, bottomPts[next], {0, -1, 0});
        geo.addTriangle(bottomCenterIdx, i1, i0);
    }

    // 顶面
    uint32_t topCenterIdx = addVert(geo, topCenter, {0, 1, 0});
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;
        uint32_t i0 = addVert(geo, topPts[i], {0, 1, 0});
        uint32_t i1 = addVert(geo, topPts[next], {0, 1, 0});
        geo.addTriangle(topCenterIdx, i0, i1);
    }
}

void coneFrustum(GeoData& geo, float bottomRadius, float topRadius, float height, int sectors) {
    Loop bottom = LoopFactory::circle(0, 0, bottomRadius, sectors);
    Loop top = LoopFactory::circle(0, height, topRadius, sectors);
    coneFrustum(geo, bottom, top, height);
}

void torus(GeoData& geo, float majorRadius, float minorRadius, int rings, int sectors) {
    torus(geo, 0, 0, 0, majorRadius, minorRadius, rings, sectors);
}

void torus(GeoData& geo, float cx, float cy, float cz, float majorRadius, float minorRadius, int rings, int sectors) {
    std::vector<std::vector<uint32_t>> grid(rings, std::vector<uint32_t>(sectors));

    for (int ri = 0; ri < rings; ++ri) {
        float phi = 2.f * PI * ri / rings;
        for (int si = 0; si < sectors; ++si) {
            float theta = 2.f * PI * si / sectors;
            float x = (majorRadius + minorRadius * std::cos(theta)) * std::cos(phi);
            float z = (majorRadius + minorRadius * std::cos(theta)) * std::sin(phi);
            float y = minorRadius * std::sin(theta);

            Vector3 normal = glm::normalize(Vector3(
                std::cos(theta) * std::cos(phi),
                std::sin(theta),
                std::cos(theta) * std::sin(phi)
            ));

            grid[ri][si] = addVert(geo, {cx + x, cy + y, cz + z}, normal,
                {(float)ri / rings, (float)si / sectors});
        }
    }

    for (int ri = 0; ri < rings; ++ri) {
        int nextRi = (ri + 1) % rings;
        for (int si = 0; si < sectors; ++si) {
            int nextSi = (si + 1) % sectors;
            geo.addQuad(grid[ri][si], grid[ri][nextSi], grid[nextRi][nextSi], grid[nextRi][si]);
        }
    }
}

void capsule(GeoData& geo, float radius, float height, int rings, int sectors) {
    // 圆柱体部分
    cylinder(geo, radius, height - 2 * radius, sectors, 1);

    // 两个半球
    // 简化处理：使用球体的上下半部分
    float halfH = (height - 2 * radius) * 0.5f;

    // 底部半球
    for (int ri = rings / 2; ri <= rings; ++ri) {
        float phi = PI * ri / rings;
        for (int si = 0; si <= sectors; ++si) {
            float theta = 2.f * PI * si / sectors;
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = -halfH - radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            // ... 添加顶点和三角形
        }
    }
}

// --------------------------------------------------------
//  复杂实体构造
// --------------------------------------------------------

void extrude(GeoData& geo, const Loop& profile, float height, int subdivisions) {
    extrudeWithHoles(geo, profile, {}, height);
}

void extrudeBoth(GeoData& geo, const Loop& profile, float heightPositive, float heightNegative, int subdivisions) {
    // 先向下拉伸，再向上拉伸
    if (heightNegative > 0) {
        extrudeTapered(geo, profile, -heightNegative, 1.0f, subdivisions);
    }
    if (heightPositive > 0) {
        extrudeTapered(geo, profile, heightPositive, 1.0f, subdivisions);
    }
}

void extrudeTapered(GeoData& geo, const Loop& profile, float height, float topScale, int subdivisions) {
    auto pts = profile.samplePoints();
    if (pts.size() < 3) return;

    size_t n = pts.size();
    if (profile.isClosed()) n--; // 去掉闭合点

    // 计算中心
    Point3 center(0, 0, 0);
    for (size_t i = 0; i < n; ++i) {
        center.x += pts[i].x;
        center.y += pts[i].y;
        center.z += pts[i].z;
    }
    center.x /= n; center.y /= n; center.z /= n;

    // 每层的高度和缩放
    for (int sub = 0; sub < subdivisions; ++sub) {
        float t0 = (float)sub / subdivisions;
        float t1 = (float)(sub + 1) / subdivisions;

        float y0 = height * t0;
        float y1 = height * t1;
        float s0 = 1.0f + (topScale - 1.0f) * t0;
        float s1 = 1.0f + (topScale - 1.0f) * t1;

        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;

            Point3 p0 = pts[i];
            Point3 p1 = pts[next];

            // 底层点
            Point3 b0(p0.x * s0, y0, p0.z * s0);
            Point3 b1(p1.x * s0, y0, p1.z * s0);

            // 顶层点
            Point3 t0p(p0.x * s1, y1, p0.z * s1);
            Point3 t1p(p1.x * s1, y1, p1.z * s1);

            Vector3 normal = computeNormal(b0, t0p, b1);

            uint32_t b0i = addVert(geo, b0, normal);
            uint32_t b1i = addVert(geo, b1, normal);
            uint32_t t0i = addVert(geo, t0p, normal);
            uint32_t t1i = addVert(geo, t1p, normal);

            geo.addQuad(b0i, b1i, t1i, t0i);
        }
    }

    // 底面
    uint32_t bottomCenter = addVert(geo, {center.x, 0, center.z}, {0, -1, 0});
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;
        uint32_t i0 = addVert(geo, {pts[i].x, 0, pts[i].z}, {0, -1, 0});
        uint32_t i1 = addVert(geo, {pts[next].x, 0, pts[next].z}, {0, -1, 0});
        geo.addTriangle(bottomCenter, i1, i0);
    }

    // 顶面
    uint32_t topCenter = addVert(geo, {center.x * topScale, height, center.z * topScale}, {0, 1, 0});
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;
        uint32_t i0 = addVert(geo, {pts[i].x * topScale, height, pts[i].z * topScale}, {0, 1, 0});
        uint32_t i1 = addVert(geo, {pts[next].x * topScale, height, pts[next].z * topScale}, {0, 1, 0});
        geo.addTriangle(topCenter, i0, i1);
    }
}

void extrudeTwisted(GeoData& geo, const Loop& profile, float height, float twistAngle, int subdivisions) {
    auto pts = profile.samplePoints();
    if (pts.size() < 3) return;

    size_t n = pts.size();
    if (profile.isClosed()) n--;

    float twistRad = twistAngle * PI / 180.f;

    for (int sub = 0; sub < subdivisions; ++sub) {
        float t0 = (float)sub / subdivisions;
        float t1 = (float)(sub + 1) / subdivisions;

        float y0 = height * t0;
        float y1 = height * t1;
        float a0 = twistRad * t0;
        float a1 = twistRad * t1;

        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;

            auto rotate = [&](const Point3& p, float angle) -> Point3 {
                float c = std::cos(angle), s = std::sin(angle);
                return {p.x * c - p.z * s, p.y, p.x * s + p.z * c};
            };

            Point3 b0 = rotate(pts[i], a0);
            b0.y = y0;
            Point3 b1 = rotate(pts[next], a0);
            b1.y = y0;
            Point3 t0p = rotate(pts[i], a1);
            t0p.y = y1;
            Point3 t1p = rotate(pts[next], a1);
            t1p.y = y1;

            Vector3 normal = computeNormal(b0, t0p, b1);

            geo.addQuad(
                addVert(geo, b0, normal),
                addVert(geo, b1, normal),
                addVert(geo, t1p, normal),
                addVert(geo, t0p, normal)
            );
        }
    }
}

void revolve(GeoData& geo, const Loop& profile, float angle, int segments) {
    revolve(geo, profile, Point3(0, 0, 0), Point3(0, 1, 0), angle, segments);
}

void revolve(GeoData& geo, const Loop& profile, const Point3& axisStart, const Point3& axisEnd, float angle, int segments) {
    auto pts = profile.samplePoints();
    if (pts.size() < 2) return;

    Vector3 axisDir = glm::normalize(axisEnd - axisStart);
    float angleRad = angle * PI / 180.f;

    // 构建旋转矩阵（绕Y轴简化处理）
    for (int seg = 0; seg < segments; ++seg) {
        float a0 = angleRad * seg / segments;
        float a1 = angleRad * (seg + 1) / segments;

        for (size_t i = 0; i < pts.size() - 1; ++i) {
            auto rotatePoint = [&](const Point3& p, float a) -> Point3 {
                float c = std::cos(a), s = std::sin(a);
                return {p.x * c, p.y, p.x * s};
            };

            Point3 p0 = rotatePoint(pts[i], a0);
            Point3 p1 = rotatePoint(pts[i], a1);
            Point3 p2 = rotatePoint(pts[i+1], a0);
            Point3 p3 = rotatePoint(pts[i+1], a1);

            Vector3 normal = computeNormal(p0, p1, p2);

            geo.addQuad(
                addVert(geo, p0, normal),
                addVert(geo, p1, normal),
                addVert(geo, p3, normal),
                addVert(geo, p2, normal)
            );
        }
    }
}

void sweep(GeoData& geo, const Loop& profile, const Path& path, int subdivisions) {
    auto profilePts = profile.samplePoints();
    if (profilePts.size() < 3 || path.empty()) return;

    size_t n = profilePts.size();
    if (profile.isClosed()) n--;

    for (int sub = 0; sub < subdivisions; ++sub) {
        float t0 = (float)sub / subdivisions;
        float t1 = (float)(sub + 1) / subdivisions;

        Point3 pos0 = path.pointAt(t0);
        Point3 pos1 = path.pointAt(t1);

        Vector3 tan0 = path.tangentAt(t0);
        Vector3 tan1 = path.tangentAt(t1);

        // 构建局部坐标系
        auto buildFrame = [&](const Point3& pos, const Vector3& tangent) -> std::pair<Vector3, Vector3> {
            Vector3 up(0, 1, 0);
            Vector3 right = glm::normalize(glm::cross(up, tangent));
            if (glm::length(right) < EPS) {
                right = glm::normalize(glm::cross(Vector3(0, 0, 1), tangent));
            }
            Vector3 normal = glm::normalize(glm::cross(tangent, right));
            return {right, normal};
        };

        auto [right0, normal0] = buildFrame(pos0, tan0);
        auto [right1, normal1] = buildFrame(pos1, tan1);

        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;

            auto transformPoint = [&](const Point3& p, const Point3& origin, const Vector3& right, const Vector3& normal) -> Point3 {
                return origin + p.x * right + p.z * normal;
            };

            Point3 b0 = transformPoint(profilePts[i], pos0, right0, normal0);
            Point3 b1 = transformPoint(profilePts[next], pos0, right0, normal0);
            Point3 t0p = transformPoint(profilePts[i], pos1, right1, normal1);
            Point3 t1p = transformPoint(profilePts[next], pos1, right1, normal1);

            Vector3 faceNormal = computeNormal(b0, t0p, b1);

            geo.addQuad(
                addVert(geo, b0, faceNormal),
                addVert(geo, b1, faceNormal),
                addVert(geo, t1p, faceNormal),
                addVert(geo, t0p, faceNormal)
            );
        }
    }
}

void sweepTwisted(GeoData& geo, const Loop& profile, const Path& path, float twistAngle, int subdivisions) {
    // 简化实现：在扫掠过程中旋转截面
    auto profilePts = profile.samplePoints();
    if (profilePts.size() < 3 || path.empty()) return;

    float twistRad = twistAngle * PI / 180.f;
    // ... 实现类似于 sweep，但在变换时加入旋转
}

void loft(GeoData& geo, const std::vector<Loop>& profiles, int subdivisions) {
    if (profiles.size() < 2) return;

    for (size_t p = 0; p < profiles.size() - 1; ++p) {
        auto pts0 = profiles[p].samplePoints();
        auto pts1 = profiles[p + 1].samplePoints();

        if (pts0.size() != pts1.size()) continue;

        size_t n = pts0.size();
        if (profiles[p].isClosed()) n--;

        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;

            Vector3 normal = computeNormal(pts0[i], pts1[i], pts0[next]);

            geo.addQuad(
                addVert(geo, pts0[i], normal),
                addVert(geo, pts0[next], normal),
                addVert(geo, pts1[next], normal),
                addVert(geo, pts1[i], normal)
            );
        }
    }
}

void thickShell(GeoData& geo, float thickness) {
    // 简化实现：基于面法线偏移
    // 实际实现需要更复杂的偏移算法
}

void extrudeWithHoles(GeoData& geo, const Loop& outer, const std::vector<Loop>& holes, float height) {
    // 三角剖分顶面
    std::vector<TriangleIndex> topTris;
    outer.triangulate(topTris);

    auto outerPts = outer.samplePoints();
    size_t n = outerPts.size();
    if (outer.isClosed()) n--;

    // 侧面
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;

        Vector3 normal = computeNormal(outerPts[i], outerPts[next], Point3(outerPts[i].x, height, outerPts[i].z));

        uint32_t b0 = addVert(geo, outerPts[i], normal);
        uint32_t b1 = addVert(geo, outerPts[next], normal);
        uint32_t t0 = addVert(geo, {outerPts[i].x, height, outerPts[i].z}, normal);
        uint32_t t1 = addVert(geo, {outerPts[next].x, height, outerPts[next].z}, normal);

        geo.addQuad(b0, b1, t1, t0);
    }

    // 孔洞侧面
    for (const auto& hole : holes) {
        auto holePts = hole.samplePoints();
        size_t hn = holePts.size();
        if (hole.isClosed()) hn--;

        for (size_t i = 0; i < hn; ++i) {
            size_t next = (i + 1) % hn;

            Vector3 normal = computeNormal(Point3(holePts[i].x, height, holePts[i].z), holePts[i], holePts[next]);

            uint32_t b0 = addVert(geo, holePts[i], normal);
            uint32_t b1 = addVert(geo, holePts[next], normal);
            uint32_t t0 = addVert(geo, {holePts[i].x, height, holePts[i].z}, normal);
            uint32_t t1 = addVert(geo, {holePts[next].x, height, holePts[next].z}, normal);

            geo.addQuad(b0, t0, t1, b1); // 注意顺序相反
        }
    }

    // 底面
    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon;

    std::vector<Point> outerContour;
    for (const auto& p : outerPts) outerContour.push_back({p.x, p.z});
    polygon.push_back(outerContour);

    for (const auto& hole : holes) {
        std::vector<Point> holeContour;
        for (const auto& p : hole.samplePoints()) holeContour.push_back({p.x, p.z});
        polygon.push_back(holeContour);
    }

    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    // 底面顶点
    uint32_t baseIdx = geo.vertices.size();
    for (const auto& p : outerPts) {
        addVert(geo, p, {0, -1, 0});
    }

    // 底面三角形（注意绕序）
    for (size_t i = 0; i < indices.size(); i += 3) {
        geo.addTriangle(baseIdx + indices[i+2], baseIdx + indices[i+1], baseIdx + indices[i]);
    }

    // 顶面
    polygon.clear();
    outerContour.clear();
    for (const auto& p : outerPts) outerContour.push_back({p.x, p.z});
    polygon.push_back(outerContour);

    for (const auto& hole : holes) {
        std::vector<Point> holeContour;
        for (const auto& p : hole.samplePoints()) holeContour.push_back({p.x, p.z});
        polygon.push_back(holeContour);
    }

    indices = mapbox::earcut<uint32_t>(polygon);

    baseIdx = geo.vertices.size();
    for (const auto& p : outerPts) {
        addVert(geo, {p.x, height, p.z}, {0, 1, 0});
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        geo.addTriangle(baseIdx + indices[i], baseIdx + indices[i+1], baseIdx + indices[i+2]);
    }
}

void revolveWithHoles(GeoData& geo, const Loop& outer, const std::vector<Loop>& holes, float angle, int segments) {
    // 简化：先处理outer的旋转，再处理holes
    revolve(geo, outer, angle, segments);
}

void pipe(GeoData& geo, const Path& path, float outerRadius, float innerRadius, int sectors) {
    // 外管
    Loop outerProfile = LoopFactory::circle(0, 0, outerRadius, sectors);
    sweep(geo, outerProfile, path, 32);

    // 内管（反向法线）
    Loop innerProfile = LoopFactory::circle(0, 0, innerRadius, sectors);
    // ... 需要反向顶点顺序
}

void spring(GeoData& geo, float radius, float wireRadius, float height, float turns, int segments, int wireSegments) {
    // 螺旋路径
    Path helixPath = PathFactory::helix({0, 0, 0}, radius, height, turns, segments);

    // 圆截面沿路径扫掠
    Loop wireProfile = LoopFactory::circle(0, 0, wireRadius, wireSegments);
    sweep(geo, wireProfile, helixPath, segments);
}

void thread(GeoData& geo, float radius, float pitch, float depth, float height, int segments) {
    // 螺纹实现
    int turns = (int)(height / pitch);

    for (int turn = 0; turn < turns; ++turn) {
        float y0 = turn * pitch;
        float y1 = (turn + 1) * pitch;

        // 每圈的三角形螺纹
        for (int seg = 0; seg < segments; ++seg) {
            float a0 = 2.f * PI * seg / segments;
            float a1 = 2.f * PI * (seg + 1) / segments;

            float r0 = radius;
            float r1 = radius + depth * (1 - std::abs((seg % (segments/2)) * 2.0f / segments - 1) * 2);

            // ... 添加螺纹齿面
        }
    }
}

void gear(GeoData& geo, int teeth, float outerRadius, float innerRadius, float thickness, float toothDepth) {
    // 齿轮轮廓
    Loop gearLoop = LoopFactory::gear(teeth, outerRadius, innerRadius, toothDepth);

    // 拉伸成3D
    extrude(geo, gearLoop, thickness);
}

} // namespace SolidFactory

// ============================================================
//  Entity 属性方法
// ============================================================

void Entity::setAttr(const std::string& key, const AttrValue& val) {
    attrs_[key] = val;
    markDirty();
    if (attrChangedCb_) attrChangedCb_(*this, key);
}

const AttrValue* Entity::getAttr(const std::string& key) const {
    auto it = attrs_.find(key);
    return it != attrs_.end() ? &it->second : nullptr;
}

float Entity::getFloat(const std::string& key, float defVal) const {
    auto* v = getAttr(key);
    if (!v) return defVal;
    if (std::holds_alternative<float>(*v)) return std::get<float>(*v);
    if (std::holds_alternative<int>(*v)) return (float)std::get<int>(*v);
    return defVal;
}

int Entity::getInt(const std::string& key, int defVal) const {
    auto* v = getAttr(key);
    if (!v) return defVal;
    if (std::holds_alternative<int>(*v)) return std::get<int>(*v);
    if (std::holds_alternative<float>(*v)) return (int)std::get<float>(*v);
    return defVal;
}

bool Entity::getBool(const std::string& key, bool defVal) const {
    auto* v = getAttr(key);
    if (!v) return defVal;
    if (std::holds_alternative<bool>(*v)) return std::get<bool>(*v);
    return defVal;
}

bool Entity::hasAttr(const std::string& key) const {
    return attrs_.count(key) > 0;
}

// ============================================================
//  Document
// ============================================================

Entity& Document::createEntity(const std::string& name) {
    auto entity = std::make_unique<Entity>(name, nextId_++);
    Entity& ref = *entity;
    entities_.push_back(std::move(entity));
    return ref;
}

bool Document::removeEntity(uint64_t id) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    if (it == entities_.end()) return false;
    entities_.erase(it);
    return true;
}

Entity* Document::findEntity(uint64_t id) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    return it != entities_.end() ? it->get() : nullptr;
}

const Entity* Document::findEntity(uint64_t id) const {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    return it != entities_.end() ? it->get() : nullptr;
}

Entity* Document::findEntityByName(const std::string& name) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [&name](const std::unique_ptr<Entity>& e) { return e->name() == name; });
    return it != entities_.end() ? it->get() : nullptr;
}

// ============================================================
//  Entity 位姿接口实现
// ============================================================

// 使用GLM的额外功能
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

Point3 Entity::position() const {
    return Point3(transform_[3]);
}

void Entity::setPosition(const Point3& pos) {
    transform_[3] = Vector4(pos, 1.f);
    markDirty();
}

void Entity::translate(const Vector3& delta) {
    transform_ = glm::translate(transform_, delta);
    markDirty();
}

Vector3 Entity::rotation() const {
    // 从矩阵提取欧拉角
    Vector3 eulerAngles(0.f);
    
    // 提取旋转矩阵部分（去除位移和缩放）
    Matrix4 rotMat = transform_;
    Vector3 scl = scale();
    if (scl.x != 0.f && scl.y != 0.f && scl.z != 0.f) {
        // 去除缩放
        rotMat[0] /= scl.x;
        rotMat[1] /= scl.y;
        rotMat[2] /= scl.z;
    }
    rotMat[3] = Vector4(0.f, 0.f, 0.f, 1.f);
    
    // 使用GLM提取欧拉角
    glm::extractEulerAngleXYZ(rotMat, eulerAngles.x, eulerAngles.y, eulerAngles.z);
    
    return eulerAngles;
}

void Entity::setRotation(const Vector3& eulerAngles) {
    // 保留当前位置和缩放
    Point3 pos = position();
    Vector3 scl = scale();
    
    // 创建新的旋转矩阵
    Matrix4 rotMat = glm::eulerAngleXYZ(eulerAngles.x, eulerAngles.y, eulerAngles.z);
    
    // 重新组合变换：位置 * 旋转 * 缩放
    composeTransform(pos, eulerAngles, scl);
}

void Entity::setRotationAxisAngle(const Vector3& axis, float angle) {
    Point3 pos = position();
    Vector3 scl = scale();
    
    Matrix4 rotMat = glm::rotate(Matrix4(1.f), angle, glm::normalize(axis));
    
    // 组合变换
    transform_ = glm::translate(Matrix4(1.f), pos) * rotMat * glm::scale(Matrix4(1.f), scl);
    markDirty();
}

void Entity::setRotationQuat(float w, float x, float y, float z) {
    Point3 pos = position();
    Vector3 scl = scale();
    
    glm::quat q(w, x, y, z);
    Matrix4 rotMat = glm::toMat4(q);
    
    transform_ = glm::translate(Matrix4(1.f), pos) * rotMat * glm::scale(Matrix4(1.f), scl);
    markDirty();
}

void Entity::rotate(const Vector3& eulerAngles) {
    // 绕世界轴旋转
    Matrix4 rotX = glm::rotate(Matrix4(1.f), eulerAngles.x, Vector3(1, 0, 0));
    Matrix4 rotY = glm::rotate(Matrix4(1.f), eulerAngles.y, Vector3(0, 1, 0));
    Matrix4 rotZ = glm::rotate(Matrix4(1.f), eulerAngles.z, Vector3(0, 0, 1));
    
    transform_ = rotZ * rotY * rotX * transform_;
    markDirty();
}

void Entity::rotateX(float angle) {
    transform_ = glm::rotate(transform_, angle, Vector3(1, 0, 0));
    markDirty();
}

void Entity::rotateY(float angle) {
    transform_ = glm::rotate(transform_, angle, Vector3(0, 1, 0));
    markDirty();
}

void Entity::rotateZ(float angle) {
    transform_ = glm::rotate(transform_, angle, Vector3(0, 0, 1));
    markDirty();
}

Vector3 Entity::scale() const {
    // 从矩阵提取缩放（列向量的长度）
    Vector3 scl;
    scl.x = glm::length(Vector3(transform_[0]));
    scl.y = glm::length(Vector3(transform_[1]));
    scl.z = glm::length(Vector3(transform_[2]));
    return scl;
}

void Entity::setScale(const Vector3& s) {
    Point3 pos = position();
    Vector3 rot = rotation();
    composeTransform(pos, rot, s);
}

void Entity::scaleUniform(float factor) {
    transform_ = glm::scale(transform_, Vector3(factor));
    markDirty();
}

void Entity::resetTransform() {
    transform_ = Matrix4(1.f);
    markDirty();
}

void Entity::composeTransform(const Point3& pos, const Vector3& rot, const Vector3& scl) {
    // TRS顺序：先缩放，再旋转，最后平移
    Matrix4 T = glm::translate(Matrix4(1.f), pos);
    Matrix4 R = glm::eulerAngleXYZ(rot.x, rot.y, rot.z);
    Matrix4 S = glm::scale(Matrix4(1.f), scl);
    
    transform_ = T * R * S;
    markDirty();
}

void Entity::decomposeTransform(Point3& pos, Vector3& rot, Vector3& scl) const {
    pos = position();
    rot = rotation();
    scl = scale();
}

BoundBox Entity::worldBoundBox() const {
    BoundBox localBb = boundBox();
    if (!localBb.isValid()) return localBb;
    
    BoundBox worldBb;
    // 变换包围盒的8个角点
    for (int i = 0; i < 8; ++i) {
        Point3 corner(
            (i & 1) ? localBb.max.x : localBb.min.x,
            (i & 2) ? localBb.max.y : localBb.min.y,
            (i & 4) ? localBb.max.z : localBb.min.z
        );
        // 应用变换
        Vector4 transformed = transform_ * Vector4(corner, 1.f);
        worldBb.expand(Point3(transformed));
    }
    return worldBb;
}

// ============================================================
//  JSON序列化
// ============================================================

using json = nlohmann::json;

// JSON 序列化辅助函数
namespace {

json vector3ToJson(const Vector3& v) {
    return json::array({v.x, v.y, v.z});
}

Vector3 jsonToVector3(const json& j) {
    if (j.is_array() && j.size() >= 3)
        return Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    return Vector3(0.f);
}

json point3ToJson(const Point3& p) {
    return json::array({p.x, p.y, p.z});
}

Point3 jsonToPoint3(const json& j) {
    if (j.is_array() && j.size() >= 3)
        return Point3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    return Point3(0.f);
}

json vertexToJson(const Vertex& v) {
    return {
        {"pos", point3ToJson(v.pos)},
        {"tangent", vector3ToJson(v.tangent)},
        {"normal", vector3ToJson(v.normal)},
        {"uv", {v.uv.x, v.uv.y}}
    };
}

Vertex jsonToVertex(const json& j) {
    Vertex v;
    if (j.contains("pos")) v.pos = jsonToPoint3(j["pos"]);
    if (j.contains("tangent")) v.tangent = jsonToVector3(j["tangent"]);
    if (j.contains("normal")) v.normal = jsonToVector3(j["normal"]);
    if (j.contains("uv") && j["uv"].is_array() && j["uv"].size() >= 2) {
        v.uv.x = j["uv"][0].get<float>();
        v.uv.y = j["uv"][1].get<float>();
    }
    return v;
}

json colorToJson(const Color& c) {
    return json::array({c.r, c.g, c.b, c.a});
}

Color jsonToColor(const json& j) {
    if (j.is_array() && j.size() >= 4)
        return Color(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return Color();
}

json attrValueToJson(const AttrValue& v) {
    return std::visit([](const auto& val) -> json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>)
            return {"float", val};
        else if constexpr (std::is_same_v<T, int>)
            return {"int", val};
        else if constexpr (std::is_same_v<T, bool>)
            return {"bool", val};
        else if constexpr (std::is_same_v<T, std::string>)
            return {"string", val};
        else if constexpr (std::is_same_v<T, AttrEnum>)
            return {"enum", static_cast<int32_t>(val)};
        else if constexpr (std::is_same_v<T, Color>)
            return {"color", colorToJson(val)};
        return {};
    }, v);
}

AttrValue jsonToAttrValue(const json& j) {
    if (!j.is_array() || j.size() < 2) return 0.f;
    std::string type = j[0].get<std::string>();
    if (type == "float") return j[1].get<float>();
    if (type == "int") return j[1].get<int>();
    if (type == "bool") return j[1].get<bool>();
    if (type == "string") return j[1].get<std::string>();
    if (type == "enum") return AttrEnum{j[1].get<int32_t>()};
    if (type == "color") return jsonToColor(j[1]);
    return 0.f;
}

json matrix4ToJson(const Matrix4& m) {
    json arr = json::array();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            arr.push_back(m[i][j]);
    return arr;
}

Matrix4 jsonToMatrix4(const json& j) {
    Matrix4 m(1.f);
    if (j.is_array() && j.size() >= 16) {
        int idx = 0;
        for (int i = 0; i < 4; ++i)
            for (int j2 = 0; j2 < 4; ++j2)
                m[i][j2] = j[idx++].get<float>();
    }
    return m;
}

json geoDataToJson(const GeoData& geo) {
    json j;
    j["vertices"] = json::array();
    for (const auto& v : geo.vertices)
        j["vertices"].push_back(vertexToJson(v));
    
    j["edges"] = json::array();
    for (const auto& e : geo.edges)
        j["edges"].push_back({e.a, e.b});
    
    j["triangles"] = json::array();
    for (const auto& t : geo.triangles)
        j["triangles"].push_back({t.a, t.b, t.c});
    
    return j;
}

void jsonToGeoData(const json& j, GeoData& geo) {
    geo.clear();
    if (j.contains("vertices")) {
        for (const auto& vj : j["vertices"])
            geo.vertices.push_back(jsonToVertex(vj));
    }
    if (j.contains("edges")) {
        for (const auto& ej : j["edges"])
            geo.edges.push_back({ej[0].get<uint32_t>(), ej[1].get<uint32_t>()});
    }
    if (j.contains("triangles")) {
        for (const auto& tj : j["triangles"])
            geo.triangles.push_back({tj[0].get<uint32_t>(), tj[1].get<uint32_t>(), tj[2].get<uint32_t>()});
    }
}

json entityToJson(const Entity& e) {
    json j;
    j["id"] = e.id();
    j["name"] = e.name();
    j["scriptId"] = e.scriptId();
    j["transform"] = matrix4ToJson(e.transform());
    j["attrs"] = json::object();
    for (const auto& [k, v] : e.attrs())
        j["attrs"][k] = attrValueToJson(v);
    j["geoData"] = geoDataToJson(e.geoData());
    return j;
}

std::unique_ptr<Entity> jsonToEntity(const json& j) {
    uint64_t id = j.value("id", 0ull);
    std::string name = j.value("name", "Entity");
    auto e = std::make_unique<Entity>(name, id);
    
    if (j.contains("scriptId"))
        e->setScriptId(j["scriptId"].get<std::string>());
    if (j.contains("transform"))
        e->setTransform(jsonToMatrix4(j["transform"]));
    if (j.contains("attrs")) {
        for (auto& [k, v] : j["attrs"].items())
            e->setAttr(k, jsonToAttrValue(v));
    }
    if (j.contains("geoData"))
        jsonToGeoData(j["geoData"], e->geoData());
    
    return e;
}

} // anonymous namespace

std::string Document::toJsonString(int indent) const {
    json j;
    j["version"] = 1;
    j["type"] = "ScriptGeometry";
    j["entities"] = json::array();
    
    for (const auto& e : entities_)
        j["entities"].push_back(entityToJson(*e));
    
    return j.dump(indent);
}

bool Document::fromJsonString(const std::string& str) {
    try {
        json j = json::parse(str);
        
        if (j.value("type", "") != "ScriptGeometry")
            return false;
        
        entities_.clear();
        nextId_ = 1;
        
        if (j.contains("entities")) {
            for (const auto& ej : j["entities"]) {
                auto e = jsonToEntity(ej);
                nextId_ = std::max(nextId_, e->id() + 1);
                entities_.push_back(std::move(e));
            }
        }
        
        modified_ = false;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Document::saveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << toJsonString(2);
    return true;
}

bool Document::loadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    if (fromJsonString(ss.str())) {
        filePath_ = filepath;
        return true;
    }
    return false;
}

// ============================================================
//  STL导入导出
// ============================================================

namespace StlIo {

// 计算三角形法线
static Vector3 computeNormal(const Point3& a, const Point3& b, const Point3& c) {
    Vector3 ab = b - a;
    Vector3 ac = c - a;
    Vector3 n = glm::cross(ab, ac);
    float len = glm::length(n);
    return len > EPS ? n / len : Vector3(0, 0, 1);
}

bool exportAscii(const GeoData& geo, const std::string& filepath, const std::string& name) {
    if (geo.triangles.empty()) return false;
    
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    
    f << "solid " << name << "\n";
    
    for (const auto& tri : geo.triangles) {
        if (tri.a >= geo.vertices.size() || tri.b >= geo.vertices.size() || tri.c >= geo.vertices.size())
            continue;
        
        const auto& va = geo.vertices[tri.a];
        const auto& vb = geo.vertices[tri.b];
        const auto& vc = geo.vertices[tri.c];
        
        // 计算法线（如果顶点法线为默认值）
        Vector3 n = va.normal;
        if (glm::length(n) < EPS)
            n = computeNormal(va.pos, vb.pos, vc.pos);
        
        f << "  facet normal " << n.x << " " << n.y << " " << n.z << "\n";
        f << "    outer loop\n";
        f << "      vertex " << va.pos.x << " " << va.pos.y << " " << va.pos.z << "\n";
        f << "      vertex " << vb.pos.x << " " << vb.pos.y << " " << vb.pos.z << "\n";
        f << "      vertex " << vc.pos.x << " " << vc.pos.y << " " << vc.pos.z << "\n";
        f << "    endloop\n";
        f << "  endfacet\n";
    }
    
    f << "endsolid " << name << "\n";
    return true;
}

#pragma pack(push, 1)
struct StlTriangle {
    float normal[3];
    float v1[3], v2[3], v3[3];
    uint16_t attr;
};
#pragma pack(pop)

bool exportBinary(const GeoData& geo, const std::string& filepath, const std::string& name) {
    if (geo.triangles.empty()) return false;
    
    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;
    
    // 80字节头
    char header[80] = {0};
    std::strncpy(header, name.c_str(), 79);
    f.write(header, 80);
    
    // 三角形数量
    uint32_t numTris = static_cast<uint32_t>(geo.triangles.size());
    f.write(reinterpret_cast<const char*>(&numTris), 4);
    
    for (const auto& tri : geo.triangles) {
        if (tri.a >= geo.vertices.size() || tri.b >= geo.vertices.size() || tri.c >= geo.vertices.size())
            continue;
        
        const auto& va = geo.vertices[tri.a];
        const auto& vb = geo.vertices[tri.b];
        const auto& vc = geo.vertices[tri.c];
        
        Vector3 n = va.normal;
        if (glm::length(n) < EPS)
            n = computeNormal(va.pos, vb.pos, vc.pos);
        
        StlTriangle stl;
        stl.normal[0] = n.x; stl.normal[1] = n.y; stl.normal[2] = n.z;
        stl.v1[0] = va.pos.x; stl.v1[1] = va.pos.y; stl.v1[2] = va.pos.z;
        stl.v2[0] = vb.pos.x; stl.v2[1] = vb.pos.y; stl.v2[2] = vb.pos.z;
        stl.v3[0] = vc.pos.x; stl.v3[1] = vc.pos.y; stl.v3[2] = vc.pos.z;
        stl.attr = 0;
        
        f.write(reinterpret_cast<const char*>(&stl), sizeof(StlTriangle));
    }
    
    return true;
}

// 检测是否为ASCII STL
static bool isAsciiStl(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    
    std::string firstWord;
    f >> firstWord;
    return (firstWord == "solid");
}

bool import(GeoData& geo, const std::string& filepath) {
    geo.clear();
    
    if (isAsciiStl(filepath)) {
        // ASCII STL
        std::ifstream f(filepath);
        if (!f.is_open()) return false;
        
        std::string line;
        while (std::getline(f, line)) {
            // 跳过 solid/endsolid/outer loop/endloop/endfacet
            if (line.find("facet normal") != std::string::npos) {
                float nx, ny, nz;
                if (sscanf(line.c_str(), " facet normal %f %f %f", &nx, &ny, &nz) == 3) {
                    Vector3 normal(nx, ny, nz);
                    
                    // 读取三个顶点
                    Point3 pts[3];
                    int idx = 0;
                    while (idx < 3 && std::getline(f, line)) {
                        float x, y, z;
                        if (sscanf(line.c_str(), " vertex %f %f %f", &x, &y, &z) == 3) {
                            pts[idx++] = Point3(x, y, z);
                        }
                    }
                    
                    if (idx == 3) {
                        uint32_t a = geo.addVertex(pts[0], normal);
                        uint32_t b = geo.addVertex(pts[1], normal);
                        uint32_t c = geo.addVertex(pts[2], normal);
                        geo.addTriangle(a, b, c);
                    }
                }
            }
        }
        return !geo.triangles.empty();
    } else {
        // Binary STL
        std::ifstream f(filepath, std::ios::binary);
        if (!f.is_open()) return false;
        
        // 跳过80字节头
        f.seekg(80);
        
        uint32_t numTris;
        f.read(reinterpret_cast<char*>(&numTris), 4);
        
        for (uint32_t i = 0; i < numTris; ++i) {
            StlTriangle stl;
            f.read(reinterpret_cast<char*>(&stl), sizeof(StlTriangle));
            
            Vector3 normal(stl.normal[0], stl.normal[1], stl.normal[2]);
            Point3 p1(stl.v1[0], stl.v1[1], stl.v1[2]);
            Point3 p2(stl.v2[0], stl.v2[1], stl.v2[2]);
            Point3 p3(stl.v3[0], stl.v3[1], stl.v3[2]);
            
            uint32_t a = geo.addVertex(p1, normal);
            uint32_t b = geo.addVertex(p2, normal);
            uint32_t c = geo.addVertex(p3, normal);
            geo.addTriangle(a, b, c);
        }
        
        return !geo.triangles.empty();
    }
}

bool exportEntityAscii(const Entity& entity, const std::string& filepath) {
    return exportAscii(entity.geoData(), filepath, entity.name());
}

bool exportEntityBinary(const Entity& entity, const std::string& filepath) {
    return exportBinary(entity.geoData(), filepath, entity.name());
}

bool importEntity(Entity& entity, const std::string& filepath) {
    if (import(entity.geoData(), filepath)) {
        entity.markDirty();
        return true;
    }
    return false;
}

} // namespace StlIo

} // namespace geo
