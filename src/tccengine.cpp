/**
 * @file tccengine.cpp
 * @brief TCC engine implementation
 *
 * Depends on libtcc (Tiny C Compiler library).
 * Link with: -ltcc
 */

#include "tccengine.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>

// TCC public header
#include <libtcc.h>

namespace tcc_engine {

// ============================================================
//  CompiledScript destructor
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

// ---- Symbol registration -----------------------------------------------

void TccEngine::registerSymbols(TCCState* s) const {
    // Constants
    tcc_add_symbol(s, "CAPI_PI", &CAPI_PI);
    tcc_add_symbol(s, "CAPI_E",  &CAPI_E);

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

    // Iteration guard
    tcc_add_symbol(s, "__sg_iter_limit", (void*)&maxIterations_);
}

// ---- Source wrapper -----------------------------------------------

std::string TccEngine::wrapSource(const std::string& source) const {
    // Inject forward declarations of all exposed functions
    // and an iteration counter guard.
    std::ostringstream ss;

    ss << "/* === Auto-generated preamble === */\n"
       << "typedef void* GeoDataHandle;\n"
       << "typedef unsigned int uint32_t;\n"
       << "extern float CAPI_PI;\n"
       << "extern float CAPI_E;\n"
       << "/* math_ */\n"
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
       << "/* curve_ */\n"
       << "void curve_line(GeoDataHandle,float,float,float,float,float,float);\n"
       << "void curve_polyline(GeoDataHandle,const float*,int);\n"
       << "void curve_arc(GeoDataHandle,float,float,float,float,float,float,int);\n"
       << "void curve_circle(GeoDataHandle,float,float,float,float,int);\n"
       << "void curve_bezier(GeoDataHandle,const float*,int,int);\n"
       << "void curve_catmull(GeoDataHandle,const float*,int,int);\n"
       << "/* loop_ */\n"
       << "void loop_rect(GeoDataHandle,float,float,float,float);\n"
       << "void loop_polygon(GeoDataHandle,float,float,float,int);\n"
       << "void loop_circle(GeoDataHandle,float,float,float,int);\n"
       << "void loop_ellipse(GeoDataHandle,float,float,float,float,int);\n"
       << "void loop_roundrect(GeoDataHandle,float,float,float,float,float,int);\n"
       << "/* path_ */\n"
       << "void path_line(GeoDataHandle,float,float,float,float,float,float);\n"
       << "void path_arc(GeoDataHandle,float,float,float,float,float,float,int);\n"
       << "void path_spline(GeoDataHandle,const float*,int,int);\n"
       << "/* surface_ */\n"
       << "void surface_plane(GeoDataHandle,float,float,float,float,float,int,int);\n"
       << "void surface_disk(GeoDataHandle,float,float,float,float,int,int);\n"
       << "void surface_fill(GeoDataHandle,const float*,int);\n"
       << "/* solid_ */\n"
       << "void solid_box(GeoDataHandle,float,float,float,float,float,float);\n"
       << "void solid_sphere(GeoDataHandle,float,float,float,float,int,int);\n"
       << "void solid_cylinder(GeoDataHandle,float,float,float,float,float,int,int);\n"
       << "void solid_cone(GeoDataHandle,float,float,float,float,float,int);\n"
       << "void solid_torus(GeoDataHandle,float,float,float,float,float,int,int);\n"
       << "void solid_capsule(GeoDataHandle,float,float,float,float,float,int,int);\n"
       << "/* === User code === */\n"
       << source
       << "\n";

    return ss.str();
}

// ---- Hash helper -----------------------------------------------

std::string TccEngine::hashSource(const std::string& s) const {
    // Simple djb2 hash for cache key
    uint64_t hash = 5381;
    for (unsigned char c : s)
        hash = ((hash << 5) + hash) + c;
    return std::to_string(hash);
}

// ---- Compile -----------------------------------------------

static void tccErrorHandler(void* opaque, const char* msg) {
    auto* errStr = reinterpret_cast<std::string*>(opaque);
    if (errStr) *errStr += msg;
    *errStr += '\n';
}

const CompiledScript* TccEngine::compile(const std::string& id,
                                          const std::string& source)
{
    std::string hash = hashSource(source);

    // Cache hit
    auto it = cache_.find(id);
    if (it != cache_.end() && it->second->sourceHash == hash)
        return it->second.get();

    // Evict stale entry
    if (it != cache_.end()) cache_.erase(it);

    std::string wrapped = wrapSource(source);
    lastError_.clear();

    TCCState* s = tcc_new();
    if (!s) {
        lastError_ = "Failed to create TCC state";
        return nullptr;
    }

    tcc_set_error_func(s, &lastError_, tccErrorHandler);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    if (tcc_compile_string(s, wrapped.c_str()) < 0) {
        tcc_delete(s);
        return nullptr;
    }

    registerSymbols(s);

    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0) {
        lastError_ += "\nRelocation failed";
        tcc_delete(s);
        return nullptr;
    }

    void* buildSym = tcc_get_symbol(s, "build");
    if (!buildSym) {
        lastError_ = "Script missing 'build(GeoDataHandle)' function";
        tcc_delete(s);
        return nullptr;
    }

    auto cs       = std::make_unique<CompiledScript>();
    cs->id         = id;
    cs->sourceHash = hash;
    cs->buildFn    = reinterpret_cast<CompiledScript::BuildFn>(buildSym);
    cs->state      = s;

    const CompiledScript* ptr = cs.get();
    cache_[id] = std::move(cs);
    return ptr;
}

// ---- Execute -----------------------------------------------

bool TccEngine::execute(const std::string& id, geo::GeoData& geoData) {
    auto it = cache_.find(id);
    if (it == cache_.end()) {
        lastError_ = "Script not compiled: " + id;
        return false;
    }

    const auto& cs = it->second;
    if (!cs->isValid()) {
        lastError_ = "Invalid compiled script: " + id;
        return false;
    }

    geoData.clear();
    cs->buildFn(reinterpret_cast<GeoDataHandle>(&geoData));
    return true;
}

bool TccEngine::evict(const std::string& id) {
    return cache_.erase(id) > 0;
}

void TccEngine::clearCache() {
    cache_.clear();
}

} // namespace tcc_engine
