/**
 * @file geolib.h
 * @brief ScriptGeometry Geometry Library
 *
 * Provides core geometry data structures, entity system with property support,
 * and document management for the ScriptGeometry project.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

namespace geo {

// ============================================================
//  Constants
// ============================================================

constexpr float PI  = 3.14159265358979323846f;
constexpr float E   = 2.71828182845904523536f;
constexpr float EPS = 1e-6f;   // float precision epsilon

// ============================================================
//  GLM Type Aliases
// ============================================================

using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Point2  = glm::vec2;
using Point3  = glm::vec3;
using Matrix4 = glm::mat4;

// ============================================================
//  Utility Math Functions
// ============================================================

inline bool  approxEqual(float a, float b, float eps = EPS) { return std::fabs(a - b) < eps; }
inline float min2(float a, float b)                         { return a < b ? a : b; }
inline float max2(float a, float b)                         { return a > b ? a : b; }
inline float min3(float a, float b, float c)                { return min2(a, min2(b, c)); }
inline float max3(float a, float b, float c)                { return max2(a, max2(b, c)); }

// ============================================================
//  Color
// ============================================================

struct Color {
    float r{1.f}, g{1.f}, b{1.f}, a{1.f};

    Color() = default;
    Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}

    static Color fromHex(uint32_t hex) {
        return {
            ((hex >> 16) & 0xFF) / 255.f,
            ((hex >>  8) & 0xFF) / 255.f,
            ((hex >>  0) & 0xFF) / 255.f,
            1.f
        };
    }

    bool operator==(const Color& o) const {
        return approxEqual(r, o.r) && approxEqual(g, o.g) &&
               approxEqual(b, o.b) && approxEqual(a, o.a);
    }
};

// ============================================================
//  BoundBox
// ============================================================

struct BoundBox {
    Point3 min{  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max() };
    Point3 max{ -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max() };

    void reset() {
        min = Point3( std::numeric_limits<float>::max());
        max = Point3(-std::numeric_limits<float>::max());
    }

    void expand(const Point3& p) {
        min.x = min2(min.x, p.x); min.y = min2(min.y, p.y); min.z = min2(min.z, p.z);
        max.x = max2(max.x, p.x); max.y = max2(max.y, p.y); max.z = max2(max.z, p.z);
    }

    bool isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    Point3 center() const { return (min + max) * 0.5f; }
    Vector3 size()  const { return max - min; }
};

// ============================================================
//  Vertex
// ============================================================

struct Vertex {
    Point3  pos     {0.f, 0.f, 0.f};
    Vector3 tangent {1.f, 0.f, 0.f};
    Vector3 normal  {0.f, 0.f, 1.f};
    Vector2 uv      {0.f, 0.f};

    Vertex() = default;
    Vertex(const Point3& p, const Vector3& n = {0.f, 0.f, 1.f})
        : pos(p), normal(n) {}
};

// ============================================================
//  Curve
// ============================================================

/**
 * A Curve works in two modes:
 *  - Polyline mode (expression empty): stores an explicit list of points.
 *  - Expression mode: stores a parametric expression, resolves to sample points.
 */
struct Curve {
    std::string        expression;   ///< parametric expression (empty = polyline mode)
    std::vector<Point3> points;       ///< control / sample points

    bool isExpression() const { return !expression.empty(); }

    /// Resolve expression into sample points (simple placeholder evaluation)
    void resolve(int samples = 64);

    /// Append a point (polyline mode)
    void addPoint(const Point3& p) { points.push_back(p); }

    bool isClosed() const {
        if (points.size() < 2) return false;
        return glm::distance(points.front(), points.back()) < EPS;
    }
};

// ============================================================
//  Loop  – n Curves forming a closed boundary
// ============================================================

struct Loop {
    std::vector<Curve> curves;

    void addCurve(const Curve& c) { curves.push_back(c); }

    /// Collect all sample points in order
    std::vector<Point3> samplePoints() const;

    BoundBox boundBox() const;
};

// ============================================================
//  Path  – n Curves with C1 continuity
// ============================================================

struct Path {
    std::vector<Curve> curves;

    void addCurve(const Curve& c) { curves.push_back(c); }

    std::vector<Point3> samplePoints() const;
};

// ============================================================
//  Region  – outer Loop + n inner Loops (holes)
// ============================================================

struct Region {
    Loop              outer;
    std::vector<Loop> holes;

    void setOuter(const Loop& l) { outer = l; }
    void addHole(const Loop& l)  { holes.push_back(l); }

    BoundBox boundBox() const { return outer.boundBox(); }
};

// ============================================================
//  MultiRegion  – n Regions
// ============================================================

struct MultiRegion {
    std::vector<Region> regions;

    void addRegion(const Region& r) { regions.push_back(r); }
};

// ============================================================
//  GeoData  – GPU-ready geometry buffer
// ============================================================

struct EdgeIndex {
    uint32_t a{0}, b{0};
};

struct TriangleIndex {
    uint32_t a{0}, b{0}, c{0};
};

struct GeoData {
    std::vector<Vertex>        vertices;
    std::vector<EdgeIndex>     edges;
    std::vector<TriangleIndex> triangles;

    void clear() {
        vertices.clear();
        edges.clear();
        triangles.clear();
    }

    bool isEmpty() const { return vertices.empty(); }

    BoundBox computeBoundBox() const {
        BoundBox bb;
        for (const auto& v : vertices) bb.expand(v.pos);
        return bb;
    }
};

// ============================================================
//  AttrValue  – variant attribute type
// ============================================================

enum class AttrEnum : int32_t {};

using AttrValue = std::variant<
    float,
    int,
    bool,
    std::string,
    AttrEnum,
    Color
>;

struct AttrDef {
    std::string name;
    AttrValue   defaultValue;
    std::string label;
    std::string description;
};

// ============================================================
//  Entity
// ============================================================

class Entity {
public:
    using AttrMap = std::map<std::string, AttrValue>;

    explicit Entity(const std::string& name, uint64_t id)
        : name_(name), id_(id) {}

    // --- Identification ---
    uint64_t           id()   const { return id_; }
    const std::string& name() const { return name_; }
    void               setName(const std::string& n) { name_ = n; }

    // --- Script ---
    const std::string& scriptId() const { return scriptId_; }
    void               setScriptId(const std::string& sid) { scriptId_ = sid; markDirty(); }

    // --- Attributes ---
    void               setAttr(const std::string& key, const AttrValue& val);
    const AttrValue*   getAttr(const std::string& key) const;
    bool               hasAttr(const std::string& key) const;
    const AttrMap&     attrs()  const { return attrs_; }

    // --- Transform ---
    const Matrix4& transform()    const { return transform_; }
    void           setTransform(const Matrix4& m) { transform_ = m; markDirty(); }

    // --- Geometry ---
    const GeoData& geoData()      const { return geoData_; }
    GeoData&       geoData()            { return geoData_; }
    void           setGeoData(const GeoData& d) { geoData_ = d; }

    // --- Dirty flag ---
    bool isDirty()   const { return dirty_; }
    void markDirty()       { dirty_ = true; }
    void clearDirty()      { dirty_ = false; }

    // --- BoundBox ---
    BoundBox boundBox() const { return geoData_.computeBoundBox(); }

    // --- Attribute change callback ---
    using AttrChangedCallback = std::function<void(Entity&, const std::string&)>;
    void setAttrChangedCallback(AttrChangedCallback cb) { attrChangedCb_ = std::move(cb); }

private:
    std::string  name_;
    uint64_t     id_{0};
    std::string  scriptId_;
    AttrMap      attrs_;
    Matrix4      transform_{1.f};
    GeoData      geoData_;
    bool         dirty_{true};
    AttrChangedCallback attrChangedCb_;
};

// ============================================================
//  Document
// ============================================================

class Document {
public:
    Document() = default;

    /// Create a new entity and return a reference
    Entity& createEntity(const std::string& name);

    /// Remove entity by id
    bool    removeEntity(uint64_t id);

    /// Find entity by id (returns nullptr if not found)
    Entity* findEntity(uint64_t id);
    const Entity* findEntity(uint64_t id) const;

    /// Find entity by name
    Entity* findEntityByName(const std::string& name);

    /// All entities
    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }

    /// Count
    size_t size() const { return entities_.size(); }

    void clear() { entities_.clear(); nextId_ = 1; }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    uint64_t nextId_{1};
};

} // namespace geo
