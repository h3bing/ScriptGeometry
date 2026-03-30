/**
 * @file geolib.h
 * @brief ScriptGeometry 几何库
 *
 * 提供核心几何数据结构、实体系统与属性支持、文档管理。
 * 
 * 设计原则：
 * - Entity 是场景中的独立实体，拥有独立的属性和几何数据
 * - 一个 Script 可以被多个 Entity 使用，每个 Entity 有不同的属性值
 * - Entity 通过 EntityHandle 传递给脚本，脚本从中读取属性
 * 
 * 工厂方法：
 * - CurveFactory: 曲线工厂（直线、圆弧、椭圆弧、贝塞尔、螺旋线等）
 * - LoopFactory: 截面工厂（矩形、圆、圆角矩形、多边形等）
 * - SolidFactory: 实体工厂（锥体、锥台、拉伸体、旋转体、扫掠体等）
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
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace geo {

// ============================================================
//  常量
// ============================================================

constexpr float PI  = 3.14159265358979323846f;
constexpr float E   = 2.71828182845904523536f;
constexpr float EPS = 1e-6f;

// ============================================================
//  GLM 类型别名
// ============================================================

using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Vector4 = glm::vec4;
using Point2  = glm::vec2;
using Point3  = glm::vec3;
using Matrix4 = glm::mat4;

// ============================================================
//  工具数学函数
// ============================================================

inline bool  approxEqual(float a, float b, float eps = EPS) { return std::fabs(a - b) < eps; }
inline float min2(float a, float b)                         { return a < b ? a : b; }
inline float max2(float a, float b)                         { return a > b ? a : b; }
inline float min3(float a, float b, float c)                { return min2(a, min2(b, c)); }
inline float max3(float a, float b, float c)                { return max2(a, max2(b, c)); }

// ============================================================
//  颜色
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
//  包围盒
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
//  顶点
// ============================================================

struct Vertex {
    Point3  pos     {0.f, 0.f, 0.f};
    Vector3 tangent {1.f, 0.f, 0.f};
    Vector3 normal  {0.f, 0.f, 1.f};
    Vector2 uv      {0.f, 0.f};

    Vertex() = default;
    Vertex(const Point3& p, const Vector3& n = {0.f, 0.f, 1.f})
        : pos(p), normal(n) {}
    Vertex(const Point3& p, const Vector3& t, const Vector3& n, const Vector2& u)
        : pos(p), tangent(t), normal(n), uv(u) {}
};

// ============================================================
//  索引结构
// ============================================================

struct EdgeIndex { 
    uint32_t a{0}, b{0}; 
    EdgeIndex() = default;
    EdgeIndex(uint32_t a, uint32_t b) : a(a), b(b) {}
};

struct TriangleIndex { 
    uint32_t a{0}, b{0}, c{0}; 
    TriangleIndex() = default;
    TriangleIndex(uint32_t a, uint32_t b, uint32_t c) : a(a), b(b), c(c) {}
};

// ============================================================
//  GeoData - GPU就绪的几何缓冲区
// ============================================================

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

    // --------------------------------------------------------
    //  几何数据添加方法
    // --------------------------------------------------------

    /// 添加顶点，返回索引
    uint32_t addVertex(const Point3& pos, 
                        const Vector3& normal = {0.f, 0.f, 1.f},
                        const Vector2& uv = {0.f, 0.f}) {
        vertices.push_back({pos, {1,0,0}, normal, uv});
        return static_cast<uint32_t>(vertices.size() - 1);
    }

    /// 添加顶点（完整参数）
    uint32_t addVertex(float x, float y, float z,
                        float nx, float ny, float nz,
                        float u = 0.f, float v = 0.f) {
        return addVertex({x, y, z}, {nx, ny, nz}, {u, v});
    }

    /// 添加边
    void addEdge(uint32_t a, uint32_t b) {
        edges.push_back({a, b});
    }

    /// 添加三角形
    void addTriangle(uint32_t a, uint32_t b, uint32_t c) {
        triangles.push_back({a, b, c});
    }

    /// 添加四边形（分割为两个三角形）
    void addQuad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        triangles.push_back({a, b, c});
        triangles.push_back({a, c, d});
    }
};

// ============================================================
//  曲线
// ============================================================

struct Curve {
    std::string        expression;
    std::vector<Point3> points;

    bool isExpression() const { return !expression.empty(); }
    void addPoint(const Point3& p) { points.push_back(p); }
    void addPoint(float x, float y, float z) { points.emplace_back(x, y, z); }

    bool isClosed() const {
        if (points.size() < 2) return false;
        return glm::distance(points.front(), points.back()) < EPS;
    }

    size_t size() const { return points.size(); }
    bool empty() const { return points.empty(); }
    const Point3& front() const { return points.front(); }
    const Point3& back() const { return points.back(); }
};

// ============================================================
//  循环 (闭合边界/截面)
// ============================================================

enum class JoinType { Square = 0, Round = 1, Miter = 2 };
enum class EndType { Polygon = 0, Joined = 1, Butt = 2, Square = 3, Round = 4 };

struct Loop {
    std::vector<Curve> curves;

    void addCurve(const Curve& c) { curves.push_back(c); }
    void addCurve(Curve&& c) { curves.push_back(std::move(c)); }

    std::vector<Point3> samplePoints() const;
    BoundBox boundBox() const;
    bool isClosed() const;

    // 2D投影
    std::vector<Point2> toPath2D() const;
    static Loop fromPath2D(const std::vector<Point2>& pts);
    static Loop fromFloatArray(const float* pts, int count);

    // 布尔运算
    std::vector<Loop> booleanUnion(const Loop& other) const;
    std::vector<Loop> booleanIntersect(const Loop& other) const;
    std::vector<Loop> booleanDifference(const Loop& other) const;
    std::vector<Loop> booleanXor(const Loop& other) const;
    std::vector<Loop> offset(float delta, JoinType joinType = JoinType::Round) const;

    // 三角剖分
    void triangulate(std::vector<TriangleIndex>& outTriangles) const;
};

// ============================================================
//  路径
// ============================================================

struct Path {
    std::vector<Curve> curves;

    void addCurve(const Curve& c) { curves.push_back(c); }
    bool empty() const { return curves.empty(); }
    std::vector<Point3> samplePoints() const;
    Point3 pointAt(float t) const;  // t in [0,1]
    Vector3 tangentAt(float t) const;
    float length() const;
};

// ============================================================
//  区域
// ============================================================

struct Region {
    Loop              outer;
    std::vector<Loop> holes;

    void setOuter(const Loop& l) { outer = l; }
    void addHole(const Loop& l)  { holes.push_back(l); }
    BoundBox boundBox() const { return outer.boundBox(); }
};

struct MultiRegion {
    std::vector<Region> regions;
    void addRegion(const Region& r) { regions.push_back(r); }
};

// ============================================================
//  CurveFactory - 曲线工厂
// ============================================================

namespace CurveFactory {

/// 直线
Curve line(const Point3& start, const Point3& end);
Curve line(float x0, float y0, float z0, float x1, float y1, float z1);

/// 圆弧
Curve arc(const Point3& center, float radius, float startAngle, float endAngle, int samples = 32);
Curve arc(float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples = 32);

/// 整圆
Curve circle(const Point3& center, float radius, int samples = 64);
Curve circle(float cx, float cy, float cz, float r, int samples = 64);

/// 椭圆弧
Curve ellipseArc(const Point3& center, float rx, float ry, float startAngle, float endAngle, int samples = 32);

/// 整椭圆
Curve ellipse(const Point3& center, float rx, float ry, int samples = 64);

/// 二次贝塞尔曲线
Curve quadraticBezier(const Point3& p0, const Point3& p1, const Point3& p2, int samples = 32);

/// 三次贝塞尔曲线
Curve cubicBezier(const Point3& p0, const Point3& p1, const Point3& p2, const Point3& p3, int samples = 32);

/// 贝塞尔曲线（多点）
Curve bezier(const std::vector<Point3>& controlPoints, int samples = 32);

/// Catmull-Rom 样条
Curve catmullRom(const std::vector<Point3>& controlPoints, int samples = 32);

/// 螺旋线
Curve helix(const Point3& center, float radius, float height, float turns, int samples = 200);

/// 圆锥螺旋线
Curve conicHelix(const Point3& center, float startRadius, float endRadius, float height, float turns, int samples = 200);

/// 正弦波曲线
Curve sineWave(const Point3& start, const Point3& end, float amplitude, float frequency, int samples = 100);

/// 折线
Curve polyline(const std::vector<Point3>& points);

} // namespace CurveFactory

// ============================================================
//  LoopFactory - 截面工厂
// ============================================================

namespace LoopFactory {

// --------------------------------------------------------
//  基础形状
// --------------------------------------------------------

/// 矩形
Loop rectangle(float width, float height);
Loop rectangle(float cx, float cy, float width, float height);

/// 正方形
Loop square(float size);
Loop square(float cx, float cy, float size);

/// 圆
Loop circle(float radius, int samples = 64);
Loop circle(float cx, float cy, float radius, int samples = 64);

/// 椭圆
Loop ellipse(float rx, float ry, int samples = 64);
Loop ellipse(float cx, float cy, float rx, float ry, int samples = 64);

/// 圆角矩形
Loop roundedRectangle(float width, float height, float radius, int cornerSamples = 8);
Loop roundedRectangle(float cx, float cy, float width, float height, float radius, int cornerSamples = 8);

/// 正多边形
Loop regularPolygon(int sides, float radius);
Loop regularPolygon(float cx, float cy, int sides, float radius);

/// 星形
Loop star(int points, float outerRadius, float innerRadius);
Loop star(float cx, float cy, int points, float outerRadius, float innerRadius);

/// 齿轮轮廓
Loop gear(int teeth, float outerRadius, float innerRadius, float toothDepth);

/// 圆弧段（非闭合）
Loop arcSegment(float cx, float cy, float innerRadius, float outerRadius, float startAngle, float endAngle, int samples = 32);

/// 从点列表创建
Loop fromPoints(const std::vector<Point2>& points);
Loop fromPoints(const std::vector<Point3>& points);
Loop fromFloatArray(const float* pts, int count);

// --------------------------------------------------------
//  布尔运算
// --------------------------------------------------------

/// 并集
std::vector<Loop> booleanUnion(const Loop& a, const Loop& b);

/// 交集
std::vector<Loop> booleanIntersect(const Loop& a, const Loop& b);

/// 差集
std::vector<Loop> booleanDifference(const Loop& a, const Loop& b);

/// 异或
std::vector<Loop> booleanXor(const Loop& a, const Loop& b);

/// 偏移
std::vector<Loop> offset(const Loop& loop, float delta, JoinType joinType = JoinType::Round);

} // namespace LoopFactory

// ============================================================
//  PathFactory - 路径工厂
// ============================================================

namespace PathFactory {

/// 直线路径
Path line(const Point3& start, const Point3& end);

/// 圆弧路径
Path arc(const Point3& center, float radius, float startAngle, float endAngle, int samples = 32);

/// 样条路径
Path spline(const std::vector<Point3>& controlPoints, int samples = 32);

/// 螺旋路径
Path helix(const Point3& center, float radius, float height, float turns, int samples = 200);

/// 从曲线列表创建
Path fromCurves(const std::vector<Curve>& curves);

} // namespace PathFactory

// ============================================================
//  SolidFactory - 实体工厂
// ============================================================

namespace SolidFactory {

// --------------------------------------------------------
//  基础实体
// --------------------------------------------------------

/// 立方体
void box(GeoData& geo, float width, float height, float depth);
void box(GeoData& geo, float cx, float cy, float cz, float width, float height, float depth);

/// 球体
void sphere(GeoData& geo, float radius, int rings = 24, int sectors = 48);
void sphere(GeoData& geo, float cx, float cy, float cz, float radius, int rings = 24, int sectors = 48);

/// 圆柱体
void cylinder(GeoData& geo, float radius, float height, int sectors = 32, int rings = 1);
void cylinder(GeoData& geo, float cx, float cy, float cz, float radius, float height, int sectors = 32, int rings = 1);

/// 圆锥体（从Loop生成）
void cone(GeoData& geo, const Loop& baseLoop, float height);
void cone(GeoData& geo, float baseRadius, float height, int sectors = 32);
void cone(GeoData& geo, float cx, float cy, float cz, float baseRadius, float height, int sectors = 32);

/// 圆台/锥台（两个Loop，不同大小）
void coneFrustum(GeoData& geo, const Loop& bottomLoop, const Loop& topLoop, float height);
void coneFrustum(GeoData& geo, float bottomRadius, float topRadius, float height, int sectors = 32);

/// 圆环体
void torus(GeoData& geo, float majorRadius, float minorRadius, int rings = 32, int sectors = 24);
void torus(GeoData& geo, float cx, float cy, float cz, float majorRadius, float minorRadius, int rings = 32, int sectors = 24);

/// 胶囊体
void capsule(GeoData& geo, float radius, float height, int rings = 16, int sectors = 32);

// --------------------------------------------------------
//  复杂实体构造
// --------------------------------------------------------

/// 拉伸体（Loop + 高度）
void extrude(GeoData& geo, const Loop& profile, float height, int subdivisions = 1);

/// 双向拉伸（Loop + 正负高度）
void extrudeBoth(GeoData& geo, const Loop& profile, float heightPositive, float heightNegative, int subdivisions = 1);

/// 锥度拉伸（Loop + 高度 + 顶部缩放比例）
void extrudeTapered(GeoData& geo, const Loop& profile, float height, float topScale, int subdivisions = 1);

/// 扭曲拉伸（Loop + 高度 + 扭曲角度）
void extrudeTwisted(GeoData& geo, const Loop& profile, float height, float twistAngle, int subdivisions = 16);

/// 旋转体（Loop + 旋转轴 + 角度）
void revolve(GeoData& geo, const Loop& profile, float angle = 360.f, int segments = 64);
void revolve(GeoData& geo, const Loop& profile, const Point3& axisStart, const Point3& axisEnd, float angle = 360.f, int segments = 64);

/// 扫掠体（Loop截面 + Path路径）
void sweep(GeoData& geo, const Loop& profile, const Path& path, int subdivisions = 32);

/// 扫掠体（带扭曲）
void sweepTwisted(GeoData& geo, const Loop& profile, const Path& path, float twistAngle, int subdivisions = 32);

/// 放样体（多个Loop之间的过渡）
void loft(GeoData& geo, const std::vector<Loop>& profiles, int subdivisions = 16);

/// 厚壳（从已有GeoData创建壳体）
void thickShell(GeoData& geo, float thickness);

// --------------------------------------------------------
//  带孔洞的实体
// --------------------------------------------------------

/// 带孔的拉伸体
void extrudeWithHoles(GeoData& geo, const Loop& outer, const std::vector<Loop>& holes, float height);

/// 带孔的旋转体
void revolveWithHoles(GeoData& geo, const Loop& outer, const std::vector<Loop>& holes, float angle = 360.f, int segments = 64);

// --------------------------------------------------------
//  特殊几何体
// --------------------------------------------------------

/// 管道（内径+外径+路径）
void pipe(GeoData& geo, const Path& path, float outerRadius, float innerRadius, int sectors = 24);

/// 弹簧（螺旋扫掠圆截面）
void spring(GeoData& geo, float radius, float wireRadius, float height, float turns, int segments = 200, int wireSegments = 16);

/// 螺纹
void thread(GeoData& geo, float radius, float pitch, float depth, float height, int segments = 64);

/// 齿轮（简化版）
void gear(GeoData& geo, int teeth, float outerRadius, float innerRadius, float thickness, float toothDepth);

} // namespace SolidFactory

// ============================================================
//  属性值 (变体类型)
// ============================================================

enum class AttrEnum : int32_t {};

using AttrValue = std::variant<float, int, bool, std::string, AttrEnum, Color>;

struct AttrDef {
    std::string name;
    AttrValue   defaultValue;
    std::string label;
    std::string description;
};

// ============================================================
//  Entity - 场景实体
// ============================================================

class Entity {
public:
    using AttrMap = std::map<std::string, AttrValue>;

    explicit Entity(const std::string& name, uint64_t id)
        : name_(name), id_(id) {}

    uint64_t           id()   const { return id_; }
    const std::string& name() const { return name_; }
    void               setName(const std::string& n) { name_ = n; }

    const std::string& scriptId() const { return scriptId_; }
    void               setScriptId(const std::string& sid) { scriptId_ = sid; markDirty(); }

    void               setAttr(const std::string& key, const AttrValue& val);
    const AttrValue*   getAttr(const std::string& key) const;
    float              getFloat(const std::string& key, float defVal = 0.f) const;
    int                getInt(const std::string& key, int defVal = 0) const;
    bool               getBool(const std::string& key, bool defVal = false) const;
    bool               hasAttr(const std::string& key) const;
    const AttrMap&     attrs()  const { return attrs_; }

    // --------------------------------------------------------
    //  变换矩阵（完整变换）
    // --------------------------------------------------------

    const Matrix4& transform()    const { return transform_; }
    void           setTransform(const Matrix4& m) { transform_ = m; markDirty(); }

    // --------------------------------------------------------
    //  位姿接口（位置、旋转、缩放）
    // --------------------------------------------------------

    /// 获取位置
    Point3 position() const;

    /// 设置位置
    void setPosition(const Point3& pos);
    void setPosition(float x, float y, float z) { setPosition(Point3(x, y, z)); }

    /// 平移（相对移动）
    void translate(const Vector3& delta);
    void translate(float dx, float dy, float dz) { translate(Vector3(dx, dy, dz)); }

    /// 获取旋转（欧拉角，弧度）
    Vector3 rotation() const;

    /// 设置旋转（欧拉角，弧度）
    void setRotation(const Vector3& eulerAngles);

    /// 设置旋转（轴角）
    void setRotationAxisAngle(const Vector3& axis, float angle);

    /// 设置旋转（四元数）
    void setRotationQuat(float w, float x, float y, float z);

    /// 旋转（相对旋转，绕自身轴）
    void rotate(const Vector3& eulerAngles);
    void rotateX(float angle);
    void rotateY(float angle);
    void rotateZ(float angle);

    /// 获取缩放
    Vector3 scale() const;

    /// 设置缩放
    void setScale(const Vector3& s);
    void setScale(float uniformScale) { setScale(Vector3(uniformScale)); }

    /// 均匀缩放
    void scaleUniform(float factor);

    /// 重置变换为单位矩阵
    void resetTransform();

    /// 组合变换：位置、旋转、缩放 -> 矩阵
    void composeTransform(const Point3& pos, const Vector3& rot, const Vector3& scl);

    /// 分解变换：矩阵 -> 位置、旋转、缩放
    void decomposeTransform(Point3& pos, Vector3& rot, Vector3& scl) const;

    // --------------------------------------------------------
    //  几何数据
    // --------------------------------------------------------

    const GeoData& geoData()      const { return geoData_; }
    GeoData&       geoData()            { return geoData_; }
    void           setGeoData(const GeoData& d) { geoData_ = d; }

    // --------------------------------------------------------
    //  脏标记
    // --------------------------------------------------------

    bool isDirty()   const { return dirty_; }
    void markDirty()       { dirty_ = true; }
    void clearDirty()      { dirty_ = false; }

    BoundBox boundBox() const { return geoData_.computeBoundBox(); }

    /// 获取世界包围盒（考虑变换）
    BoundBox worldBoundBox() const;

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
//  Document - 场景文档
// ============================================================

class Document {
public:
    Document() = default;

    Entity& createEntity(const std::string& name);
    bool    removeEntity(uint64_t id);
    Entity* findEntity(uint64_t id);
    const Entity* findEntity(uint64_t id) const;
    Entity* findEntityByName(const std::string& name);

    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }
    size_t size() const { return entities_.size(); }
    void clear() { entities_.clear(); nextId_ = 1; }

    // --------------------------------------------------------
    //  JSON序列化
    // --------------------------------------------------------

    /// 序列化到JSON字符串
    std::string toJsonString(int indent = -1) const;

    /// 从JSON字符串反序列化
    bool fromJsonString(const std::string& json);

    /// 保存到文件
    bool saveToFile(const std::string& filepath) const;

    /// 从文件加载
    bool loadFromFile(const std::string& filepath);

    /// 获取文件路径
    const std::string& filePath() const { return filePath_; }
    void setFilePath(const std::string& path) { filePath_ = path; }

    /// 是否有未保存的修改
    bool isModified() const { return modified_; }
    void markModified() { modified_ = true; }
    void clearModified() { modified_ = false; }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    uint64_t nextId_{1};
    std::string filePath_;
    bool modified_{false};
};

// ============================================================
//  STL导入导出
// ============================================================

namespace StlIo {

/// 导出GeoData为ASCII STL格式
bool exportAscii(const GeoData& geo, const std::string& filepath, const std::string& name = "solid");

/// 导出GeoData为二进制STL格式
bool exportBinary(const GeoData& geo, const std::string& filepath, const std::string& name = "solid");

/// 导入STL文件到GeoData（自动检测ASCII/二进制）
bool import(GeoData& geo, const std::string& filepath);

/// 从Entity导出STL
bool exportEntityAscii(const Entity& entity, const std::string& filepath);
bool exportEntityBinary(const Entity& entity, const std::string& filepath);

/// 导入STL到Entity
bool importEntity(Entity& entity, const std::string& filepath);

} // namespace StlIo

} // namespace geo
