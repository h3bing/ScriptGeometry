/**
 * @file capi.h
 * @brief 暴露给TCC脚本的C API
 *
 * 设计原则：
 * - 所有函数使用扁平C接口，TCC脚本可直接调用
 * - 脚本接收 Entity* 指针，直接从实体读取属性
 * - 脚本输出几何数据到 GeoData* 缓冲区
 *
 * 函数分组：
 *   entity_   - 实体属性访问
 *   math_     - 数学函数
 *   curve_    - 曲线构造
 *   loop_     - 闭合循环构造
 *   path_     - 路径构造
 *   surface_  - 表面构造
 *   solid_    - 实体构造
 */

#pragma once

#include "geolib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
//  常量
// ============================================================

extern const float CAPI_PI;
extern const float CAPI_E;

// ============================================================
//  Entity 属性访问 (核心 - 脚本直接访问实体属性)
// ============================================================

/**
 * 获取实体的浮点属性值
 * @param entity   实体指针
 * @param attrName 属性名
 * @return         属性值，不存在返回0
 */
float entity_getFloat(const geo::Entity* entity, const char* attrName);

/**
 * 获取实体的整数属性值
 */
int entity_getInt(const geo::Entity* entity, const char* attrName);

/**
 * 获取实体的布尔属性值
 */
int entity_getBool(const geo::Entity* entity, const char* attrName);

/**
 * 检查实体是否有指定属性
 */
int entity_hasAttr(const geo::Entity* entity, const char* attrName);

/**
 * 获取实体名称
 */
const char* entity_getName(const geo::Entity* entity);

/**
 * 获取实体ID
 */
uint64_t entity_getId(const geo::Entity* entity);

// ============================================================
//  GeoData 几何输出 (脚本输出几何数据)
// ============================================================

/**
 * 添加顶点到几何数据
 * @param geoData  几何数据指针
 * @param x,y,z    顶点坐标
 * @param nx,ny,nz 法线方向
 * @param u,v      UV坐标
 * @return         顶点索引
 */
uint32_t geo_addVertex(geo::GeoData* geoData,
                        float x, float y, float z,
                        float nx, float ny, float nz,
                        float u, float v);

/**
 * 添加边
 */
void geo_addEdge(geo::GeoData* geoData, uint32_t a, uint32_t b);

/**
 * 添加三角形
 */
void geo_addTriangle(geo::GeoData* geoData, uint32_t a, uint32_t b, uint32_t c);

// ============================================================
//  math_ 数学函数
// ============================================================

float math_sin(float x);
float math_cos(float x);
float math_tan(float x);
float math_asin(float x);
float math_acos(float x);
float math_atan(float x);
float math_atan2(float y, float x);
float math_sqrt(float x);
float math_pow(float base, float exp);
float math_exp(float x);
float math_log(float x);
float math_log2(float x);
float math_log10(float x);
float math_abs(float x);
float math_floor(float x);
float math_ceil(float x);
float math_round(float x);
float math_fmod(float x, float y);
float math_clamp(float v, float lo, float hi);
float math_lerp(float a, float b, float t);
float math_min2(float a, float b);
float math_max2(float a, float b);
float math_min3(float a, float b, float c);
float math_max3(float a, float b, float c);
float math_deg2rad(float deg);
float math_rad2deg(float rad);

// ============================================================
//  curve_ 曲线函数
// ============================================================

void curve_line(geo::GeoData* geoData,
                 float x0, float y0, float z0,
                 float x1, float y1, float z1);

void curve_polyline(geo::GeoData* geoData, const float* pts, int count);

void curve_arc(geo::GeoData* geoData,
                float cx, float cy, float cz,
                float r, float startAngle, float endAngle, int samples);

void curve_circle(geo::GeoData* geoData,
                   float cx, float cy, float cz,
                   float r, int samples);

void curve_bezier(geo::GeoData* geoData, const float* pts, int count, int samples);

void curve_catmull(geo::GeoData* geoData, const float* pts, int count, int samples);

// ============================================================
//  loop_ 循环函数 (2D多边形)
// ============================================================

void loop_rect(geo::GeoData* geoData, float cx, float cy, float width, float height);
void loop_polygon(geo::GeoData* geoData, float cx, float cy, float r, int sides);
void loop_circle(geo::GeoData* geoData, float cx, float cy, float r, int samples);
void loop_ellipse(geo::GeoData* geoData, float cx, float cy, float rx, float ry, int samples);
void loop_roundrect(geo::GeoData* geoData, float cx, float cy, float w, float h, float radius, int samples);

// 布尔运算 (Clipper2)
void loop_union(geo::GeoData* geoData, const float* loop1, int count1, const float* loop2, int count2);
void loop_intersect(geo::GeoData* geoData, const float* loop1, int count1, const float* loop2, int count2);
void loop_difference(geo::GeoData* geoData, const float* loop1, int count1, const float* loop2, int count2);
void loop_xor(geo::GeoData* geoData, const float* loop1, int count1, const float* loop2, int count2);

// 偏移
void loop_offset(geo::GeoData* geoData, const float* pts, int count, float delta, int joinType);

// 三角剖分填充
void loop_fill(geo::GeoData* geoData, const float* pts, int count);
void loop_fill_with_holes(geo::GeoData* geoData,
                           const float* outer, int outerCount,
                           const float* holes, const int* holeCounts, int numHoles);

// 简化
void loop_simplify(geo::GeoData* geoData, const float* pts, int count, float epsilon);

// ============================================================
//  path_ 路径函数
// ============================================================

void path_line(geo::GeoData* geoData, float x0, float y0, float z0, float x1, float y1, float z1);
void path_arc(geo::GeoData* geoData, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples);
void path_spline(geo::GeoData* geoData, const float* pts, int count, int samples);

// ============================================================
//  surface_ 表面函数
// ============================================================

void surface_plane(geo::GeoData* geoData, float cx, float cy, float cz, float width, float height, int subdX, int subdY);
void surface_disk(geo::GeoData* geoData, float cx, float cy, float cz, float r, int rings, int sectors);
void surface_fill(geo::GeoData* geoData, const float* pts2d, int count);

// ============================================================
//  solid_ 实体函数 (3D)
// ============================================================

void solid_box(geo::GeoData* geoData, float cx, float cy, float cz, float wx, float wy, float wz);
void solid_sphere(geo::GeoData* geoData, float cx, float cy, float cz, float r, int rings, int sectors);
void solid_cylinder(geo::GeoData* geoData, float cx, float cy, float cz, float r, float height, int sectors, int rings);
void solid_cone(geo::GeoData* geoData, float cx, float cy, float cz, float r, float height, int sectors);
void solid_torus(geo::GeoData* geoData, float cx, float cy, float cz, float R, float r, int rings, int sectors);
void solid_capsule(geo::GeoData* geoData, float cx, float cy, float cz, float r, float height, int rings, int sectors);

// ============================================================
//  curve_factory_ 曲线工厂函数
// ============================================================

/// 直线曲线
void curve_factory_line(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1);

/// 圆弧曲线
void curve_factory_arc(geo::GeoData* geo, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples);

/// 整圆曲线
void curve_factory_circle(geo::GeoData* geo, float cx, float cy, float cz, float r, int samples);

/// 椭圆弧曲线
void curve_factory_ellipse_arc(geo::GeoData* geo, float cx, float cy, float cz, float rx, float ry, float startAngle, float endAngle, int samples);

/// 整椭圆曲线
void curve_factory_ellipse(geo::GeoData* geo, float cx, float cy, float cz, float rx, float ry, int samples);

/// 二次贝塞尔曲线
void curve_factory_quadratic_bezier(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, int samples);

/// 三次贝塞尔曲线
void curve_factory_cubic_bezier(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, int samples);

/// 螺旋线
void curve_factory_helix(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float turns, int samples);

/// 圆锥螺旋线
void curve_factory_conic_helix(geo::GeoData* geo, float cx, float cy, float cz, float startRadius, float endRadius, float height, float turns, int samples);

/// 正弦波曲线
void curve_factory_sine_wave(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1, float amplitude, float frequency, int samples);

// ============================================================
//  loop_factory_ 截面工厂函数
// ============================================================

/// 矩形截面
void loop_factory_rect(geo::GeoData* geo, float cx, float cy, float width, float height);

/// 正方形截面
void loop_factory_square(geo::GeoData* geo, float cx, float cy, float size);

/// 圆形截面
void loop_factory_circle(geo::GeoData* geo, float cx, float cy, float radius, int samples);

/// 椭圆截面
void loop_factory_ellipse(geo::GeoData* geo, float cx, float cy, float rx, float ry, int samples);

/// 圆角矩形截面
void loop_factory_rounded_rect(geo::GeoData* geo, float cx, float cy, float width, float height, float radius, int cornerSamples);

/// 正多边形截面
void loop_factory_regular_polygon(geo::GeoData* geo, float cx, float cy, int sides, float radius);

/// 星形截面
void loop_factory_star(geo::GeoData* geo, float cx, float cy, int points, float outerRadius, float innerRadius);

/// 齿轮截面
void loop_factory_gear(geo::GeoData* geo, float cx, float cy, int teeth, float outerRadius, float innerRadius, float toothDepth);

/// 圆弧段截面
void loop_factory_arc_segment(geo::GeoData* geo, float cx, float cy, float innerRadius, float outerRadius, float startAngle, float endAngle, int samples);

// ============================================================
//  solid_factory_ 实体工厂函数
// ============================================================

/// 立方体
void solid_factory_box(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float depth);

/// 球体
void solid_factory_sphere(geo::GeoData* geo, float cx, float cy, float cz, float radius, int rings, int sectors);

/// 圆柱体
void solid_factory_cylinder(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int sectors, int rings);

/// 圆锥体
void solid_factory_cone(geo::GeoData* geo, float cx, float cy, float cz, float baseRadius, float height, int sectors);

/// 圆台/锥台
void solid_factory_cone_frustum(geo::GeoData* geo, float cx, float cy, float cz, float bottomRadius, float topRadius, float height, int sectors);

/// 圆环体
void solid_factory_torus(geo::GeoData* geo, float cx, float cy, float cz, float majorRadius, float minorRadius, int rings, int sectors);

/// 胶囊体
void solid_factory_capsule(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int rings, int sectors);

// 拉伸体（从截面）
void solid_factory_extrude_rect(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float depth, int subdivisions);
void solid_factory_extrude_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int samples, int subdivisions);
void solid_factory_extrude_polygon(geo::GeoData* geo, float cx, float cy, float cz, int sides, float radius, float height, int subdivisions);
void solid_factory_extrude_star(geo::GeoData* geo, float cx, float cy, float cz, int points, float outerRadius, float innerRadius, float height, int subdivisions);

/// 锥度拉伸体
void solid_factory_extrude_tapered_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float topScale, int samples, int subdivisions);

/// 扭曲拉伸体
void solid_factory_extrude_twisted_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float twistAngle, int samples, int subdivisions);

/// 旋转体（从截面旋转）
void solid_factory_revolve_rect(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float angle, int segments);

/// 弹簧
void solid_factory_spring(geo::GeoData* geo, float cx, float cy, float cz, float radius, float wireRadius, float height, float turns, int segments, int wireSegments);

/// 齿轮
void solid_factory_gear(geo::GeoData* geo, float cx, float cy, float cz, int teeth, float outerRadius, float innerRadius, float thickness, float toothDepth);

// ============================================================
//  符号注册 (供TccEngine调用)
// ============================================================

#ifdef __cplusplus
} // extern "C"

// 前向声明 - 使用libtcc.h中的真实类型
struct TCCState;

namespace tcc_engine {
void capi_register_symbols(TCCState* state);
} // namespace tcc_engine

#endif
