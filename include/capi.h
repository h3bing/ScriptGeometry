/**
 * @file capi.h
 * @brief C API exposed to TCC scripts
 *
 * All symbols use flat C linkage so TCC scripts can call them without
 * including any headers.  Functions are grouped by prefix:
 *   math_    – generic math helpers
 *   curve_   – curve construction
 *   loop_    – closed loop / boundary construction
 *   path_    – C1 path construction
 *   surface_ – surface / planar region construction
 *   solid_   – 3-D solid construction
 *
 * Every function receives a GeoData* as its first argument so TCC scripts
 * never need to allocate memory; the host's GeoData buffers are used directly.
 */

#pragma once

#include "geolib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
//  Opaque handle type exposed to TCC
// ============================================================

typedef void* GeoDataHandle;

// ============================================================
//  Constants (exposed as global variables for TCC scripts)
// ============================================================

extern const float CAPI_PI;
extern const float CAPI_E;

// ============================================================
//  math_ functions
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
//  curve_ functions
//  Each function appends geometry into the GeoDataHandle.
// ============================================================

/** Straight line segment from (x0,y0,z0) to (x1,y1,z1) */
void curve_line(GeoDataHandle h,
                float x0, float y0, float z0,
                float x1, float y1, float z1);

/** Polyline: pts is array of floats [x0,y0,z0, x1,y1,z1, ...], count = number of points */
void curve_polyline(GeoDataHandle h, const float* pts, int count);

/** Circle arc: center (cx,cy,cz), radius r, start/end angles in radians, samples */
void curve_arc(GeoDataHandle h,
               float cx, float cy, float cz,
               float r, float startAngle, float endAngle, int samples);

/** Full circle */
void curve_circle(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int samples);

/** Bezier curve: control points array [x0,y0,z0, ...], count = number of points */
void curve_bezier(GeoDataHandle h, const float* pts, int count, int samples);

/** Catmull-Rom spline through points */
void curve_catmull(GeoDataHandle h, const float* pts, int count, int samples);

// ============================================================
//  loop_ functions
// ============================================================

/** Rectangle loop */
void loop_rect(GeoDataHandle h,
               float cx, float cy,
               float width, float height);

/** Regular polygon loop */
void loop_polygon(GeoDataHandle h,
                  float cx, float cy, float r, int sides);

/** Circle loop */
void loop_circle(GeoDataHandle h,
                 float cx, float cy, float r, int samples);

/** Ellipse loop */
void loop_ellipse(GeoDataHandle h,
                  float cx, float cy, float rx, float ry, int samples);

/** Rounded rectangle */
void loop_roundrect(GeoDataHandle h,
                    float cx, float cy, float width, float height,
                    float radius, int cornerSamples);

// ============================================================
//  path_ functions
// ============================================================

/** Line path (same as curve_line but added as a Path) */
void path_line(GeoDataHandle h,
               float x0, float y0, float z0,
               float x1, float y1, float z1);

/** Arc path */
void path_arc(GeoDataHandle h,
              float cx, float cy, float cz,
              float r, float startAngle, float endAngle, int samples);

/** Spline path */
void path_spline(GeoDataHandle h, const float* pts, int count, int samples);

// ============================================================
//  surface_ functions
// ============================================================

/** Flat plane: centered at (cx,cy,cz), width×height, subdivisions */
void surface_plane(GeoDataHandle h,
                   float cx, float cy, float cz,
                   float width, float height,
                   int subdX, int subdY);

/** Disk: center (cx,cy,cz), radius, ring/sector counts */
void surface_disk(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int rings, int sectors);

/** Triangulated polygon fill from loop */
void surface_fill(GeoDataHandle h, const float* pts2d, int count);

// ============================================================
//  solid_ functions
// ============================================================

/** Box / cuboid */
void solid_box(GeoDataHandle h,
               float cx, float cy, float cz,
               float wx, float wy, float wz);

/** Sphere */
void solid_sphere(GeoDataHandle h,
                  float cx, float cy, float cz,
                  float r, int rings, int sectors);

/** Cylinder */
void solid_cylinder(GeoDataHandle h,
                    float cx, float cy, float cz,
                    float r, float height,
                    int sectors, int rings);

/** Cone */
void solid_cone(GeoDataHandle h,
                float cx, float cy, float cz,
                float r, float height,
                int sectors);

/** Torus */
void solid_torus(GeoDataHandle h,
                 float cx, float cy, float cz,
                 float R, float r,
                 int rings, int sectors);

/** Capsule (cylinder with hemisphere caps) */
void solid_capsule(GeoDataHandle h,
                   float cx, float cy, float cz,
                   float r, float height,
                   int rings, int sectors);

// ============================================================
//  Registration helper (host-side only, not called from TCC)
// ============================================================

#ifdef __cplusplus
} // extern "C"

namespace geo {

/**
 * Register all capi symbols into a TCC state so scripts can call them.
 * Called by TccEngine before compiling each script.
 */
struct TCCState;
void capi_register_symbols(TCCState* state);

} // namespace geo
#endif
