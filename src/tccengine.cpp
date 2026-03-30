/**
 * @file tccengine.cpp
 * @brief TCC引擎实现
 */

#include "tccengine.h"

#include <cstring>
#include <iostream>
#include <sstream>

#include <libtcc.h>

namespace tcc_engine {

// ============================================================
//  CompiledScript
// ============================================================

CompiledScript::~CompiledScript() {
    if (state) {
        tcc_delete(state);
        state = nullptr;
    }
}

// ============================================================
//  TccEngine
// ============================================================

TccEngine::TccEngine() = default;
TccEngine::~TccEngine() = default;

// --------------------------------------------------------
//  编译
// --------------------------------------------------------

std::string TccEngine::hashSource(const std::string& s) const {
    uint64_t hash = 5381;
    for (unsigned char c : s)
        hash = ((hash << 5) + hash) + c;
    return std::to_string(hash);
}

std::string TccEngine::wrapSource(const std::string& source,
                                   const std::vector<std::string>& attrNames) const {
    std::ostringstream ss;

    // 前置声明
    ss << "/* === 自动生成的前置代码 === */\n"

       // 类型定义
       << "typedef void* Entity;\n"
       << "typedef void* GeoData;\n"
       << "typedef unsigned int uint32_t;\n"
       << "typedef unsigned long uint64_t;\n"

       // 常量
       << "extern float CAPI_PI;\n"
       << "extern float CAPI_E;\n"

       // Entity 属性访问函数
       << "/* === Entity 属性访问 === */\n"
       << "float entity_getFloat(Entity e, const char* name);\n"
       << "int entity_getInt(Entity e, const char* name);\n"
       << "int entity_getBool(Entity e, const char* name);\n"
       << "int entity_hasAttr(Entity e, const char* name);\n"
       << "const char* entity_getName(Entity e);\n"
       << "uint64_t entity_getId(Entity e);\n"

       // GeoData 输出函数
       << "/* === GeoData 输出 === */\n"
       << "uint32_t geo_addVertex(GeoData g, float x, float y, float z, float nx, float ny, float nz, float u, float v);\n"
       << "void geo_addEdge(GeoData g, uint32_t a, uint32_t b);\n"
       << "void geo_addTriangle(GeoData g, uint32_t a, uint32_t b, uint32_t c);\n"

       // math_ 函数
       << "/* === 数学函数 === */\n"
       << "float math_sin(float); float math_cos(float); float math_tan(float);\n"
       << "float math_asin(float); float math_acos(float); float math_atan(float);\n"
       << "float math_atan2(float,float); float math_sqrt(float);\n"
       << "float math_pow(float,float); float math_exp(float);\n"
       << "float math_log(float); float math_log2(float); float math_log10(float);\n"
       << "float math_abs(float); float math_floor(float); float math_ceil(float);\n"
       << "float math_round(float); float math_fmod(float,float);\n"
       << "float math_clamp(float,float,float); float math_lerp(float,float,float);\n"
       << "float math_min2(float,float); float math_max2(float,float);\n"
       << "float math_min3(float,float,float); float math_max3(float,float,float);\n"
       << "float math_deg2rad(float); float math_rad2deg(float);\n"

       // curve_ 函数
       << "/* === 曲线函数 === */\n"
       << "void curve_line(GeoData,float,float,float,float,float,float);\n"
       << "void curve_polyline(GeoData,const float*,int);\n"
       << "void curve_arc(GeoData,float,float,float,float,float,float,int);\n"
       << "void curve_circle(GeoData,float,float,float,float,int);\n"
       << "void curve_bezier(GeoData,const float*,int,int);\n"
       << "void curve_catmull(GeoData,const float*,int,int);\n"

       // loop_ 函数
       << "/* === 循环函数 === */\n"
       << "void loop_rect(GeoData,float,float,float,float);\n"
       << "void loop_polygon(GeoData,float,float,float,int);\n"
       << "void loop_circle(GeoData,float,float,float,int);\n"
       << "void loop_ellipse(GeoData,float,float,float,float,int);\n"
       << "void loop_roundrect(GeoData,float,float,float,float,float,int);\n"
       << "void loop_union(GeoData,const float*,int,const float*,int);\n"
       << "void loop_intersect(GeoData,const float*,int,const float*,int);\n"
       << "void loop_difference(GeoData,const float*,int,const float*,int);\n"
       << "void loop_xor(GeoData,const float*,int,const float*,int);\n"
       << "void loop_offset(GeoData,const float*,int,float,int);\n"
       << "void loop_fill(GeoData,const float*,int);\n"
       << "void loop_fill_with_holes(GeoData,const float*,int,const float*,const int*,int);\n"
       << "void loop_simplify(GeoData,const float*,int,float);\n"

       // path_ 函数
       << "/* === 路径函数 === */\n"
       << "void path_line(GeoData,float,float,float,float,float,float);\n"
       << "void path_arc(GeoData,float,float,float,float,float,float,int);\n"
       << "void path_spline(GeoData,const float*,int,int);\n"

       // surface_ 函数
       << "/* === 表面函数 === */\n"
       << "void surface_plane(GeoData,float,float,float,float,float,int,int);\n"
       << "void surface_disk(GeoData,float,float,float,float,int,int);\n"
       << "void surface_fill(GeoData,const float*,int);\n"

       // solid_ 函数
       << "/* === 实体函数 === */\n"
       << "void solid_box(GeoData,float,float,float,float,float,float);\n"
       << "void solid_sphere(GeoData,float,float,float,float,int,int);\n"
       << "void solid_cylinder(GeoData,float,float,float,float,float,int,int);\n"
       << "void solid_cone(GeoData,float,float,float,float,float,int);\n"
       << "void solid_torus(GeoData,float,float,float,float,float,int,int);\n"
       << "void solid_capsule(GeoData,float,float,float,float,float,int,int);\n"

       << "/* === 用户代码 === */\n"
       << source << "\n";

    return ss.str();
}

static void tccErrorHandler(void* opaque, const char* msg) {
    auto* errStr = reinterpret_cast<std::string*>(opaque);
    if (errStr) *errStr += msg;
    *errStr += '\n';
}

const CompiledScript* TccEngine::compile(const std::string& id,
                                          const std::string& source,
                                          const std::vector<std::string>& attrNames)
{
    std::string hash = hashSource(source);

    // 检查缓存
    auto it = cache_.find(id);
    if (it != cache_.end() && it->second->sourceHash == hash) {
        return it->second.get();
    }

    // 清除旧条目
    if (it != cache_.end()) cache_.erase(it);

    std::string wrapped = wrapSource(source, attrNames);
    lastError_.clear();

    TCCState* s = tcc_new();
    if (!s) {
        lastError_ = "无法创建TCC状态";
        return nullptr;
    }

    tcc_set_error_func(s, &lastError_, tccErrorHandler);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    if (tcc_compile_string(s, wrapped.c_str()) < 0) {
        tcc_delete(s);
        return nullptr;
    }

    // 注册符号
    capi_register_symbols(s);

    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0) {
        lastError_ += "\n重定位失败";
        tcc_delete(s);
        return nullptr;
    }

    void* buildSym = tcc_get_symbol(s, "build");
    if (!buildSym) {
        lastError_ = "脚本缺少 'build(Entity, GeoData)' 函数";
        tcc_delete(s);
        return nullptr;
    }

    auto cs = std::make_unique<CompiledScript>();
    cs->id = id;
    cs->sourceHash = hash;
    cs->buildFn = reinterpret_cast<CompiledScript::BuildFn>(buildSym);
    cs->state = s;
    cs->attrNames = attrNames;

    const CompiledScript* ptr = cs.get();
    cache_[id] = std::move(cs);
    return ptr;
}

bool TccEngine::isCompiled(const std::string& id) const {
    return cache_.find(id) != cache_.end();
}

const CompiledScript* TccEngine::getCompiled(const std::string& id) const {
    auto it = cache_.find(id);
    return it != cache_.end() ? it->second.get() : nullptr;
}

// --------------------------------------------------------
//  执行
// --------------------------------------------------------

bool TccEngine::execute(const std::string& id, geo::Entity* entity, geo::GeoData& geoData) {
    auto* script = getCompiled(id);
    if (!script) {
        lastError_ = "脚本未编译: " + id;
        return false;
    }
    return execute(script, entity, geoData);
}

bool TccEngine::execute(const CompiledScript* script, geo::Entity* entity, geo::GeoData& geoData) {
    if (!script || !script->isValid()) {
        lastError_ = "无效的脚本";
        return false;
    }
    if (!entity) {
        lastError_ = "实体指针为空";
        return false;
    }

    geoData.clear();
    script->buildFn(entity, &geoData);
    return true;
}

// --------------------------------------------------------
//  缓存管理
// --------------------------------------------------------

bool TccEngine::evict(const std::string& id) {
    return cache_.erase(id) > 0;
}

void TccEngine::clearCache() {
    cache_.clear();
}

} // namespace tcc_engine
