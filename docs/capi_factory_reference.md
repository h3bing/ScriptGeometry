# 几何工厂方法 C API 参考

本文档介绍 ScriptGeometry 几何库提供的工厂方法，这些方法已封装为扁平C接口，可供TCC脚本直接调用。

## 函数命名规范

- `entity_*` - 实体属性访问
- `math_*` - 数学函数
- `curve_*` - 曲线构造（基础）
- `loop_*` - 循环/截面构造（基础）
- `path_*` - 路径构造
- `surface_*` - 表面构造
- `solid_*` - 实体构造（基础）
- `curve_factory_*` - 曲线工厂方法（高级）
- `loop_factory_*` - 截面工厂方法（高级）
- `solid_factory_*` - 实体工厂方法（高级）

---

## 曲线工厂 (curve_factory_*)

### 直线曲线
```c
void curve_factory_line(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1);
```

### 圆弧曲线
```c
void curve_factory_arc(geo::GeoData* geo, float cx, float cy, float cz, float r, float startAngle, float endAngle, int samples);
```

### 整圆曲线
```c
void curve_factory_circle(geo::GeoData* geo, float cx, float cy, float cz, float r, int samples);
```

### 椭圆弧曲线
```c
void curve_factory_ellipse_arc(geo::GeoData* geo, float cx, float cy, float cz, float rx, float ry, float startAngle, float endAngle, int samples);
```

### 整椭圆曲线
```c
void curve_factory_ellipse(geo::GeoData* geo, float cx, float cy, float cz, float rx, float ry, int samples);
```

### 二次贝塞尔曲线
```c
void curve_factory_quadratic_bezier(geo::GeoData* geo, 
    float x0, float y0, float z0,  // 起点
    float x1, float y1, float z1,  // 控制点
    float x2, float y2, float z2,  // 终点
    int samples);
```

### 三次贝塞尔曲线
```c
void curve_factory_cubic_bezier(geo::GeoData* geo,
    float x0, float y0, float z0,  // 起点
    float x1, float y1, float z1,  // 控制点1
    float x2, float y2, float z2,  // 控制点2
    float x3, float y3, float z3,  // 终点
    int samples);
```

### 螺旋线
```c
void curve_factory_helix(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float turns, int samples);
```

### 圆锥螺旋线
```c
void curve_factory_conic_helix(geo::GeoData* geo, float cx, float cy, float cz, float startRadius, float endRadius, float height, float turns, int samples);
```

### 正弦波曲线
```c
void curve_factory_sine_wave(geo::GeoData* geo, float x0, float y0, float z0, float x1, float y1, float z1, float amplitude, float frequency, int samples);
```

---

## 截面工厂 (loop_factory_*)

### 矩形截面
```c
void loop_factory_rect(geo::GeoData* geo, float cx, float cy, float width, float height);
```

### 正方形截面
```c
void loop_factory_square(geo::GeoData* geo, float cx, float cy, float size);
```

### 圆形截面
```c
void loop_factory_circle(geo::GeoData* geo, float cx, float cy, float radius, int samples);
```

### 椭圆截面
```c
void loop_factory_ellipse(geo::GeoData* geo, float cx, float cy, float rx, float ry, int samples);
```

### 圆角矩形截面
```c
void loop_factory_rounded_rect(geo::GeoData* geo, float cx, float cy, float width, float height, float radius, int cornerSamples);
```

### 正多边形截面
```c
void loop_factory_regular_polygon(geo::GeoData* geo, float cx, float cy, int sides, float radius);
```

### 星形截面
```c
void loop_factory_star(geo::GeoData* geo, float cx, float cy, int points, float outerRadius, float innerRadius);
```

### 齿轮截面
```c
void loop_factory_gear(geo::GeoData* geo, float cx, float cy, int teeth, float outerRadius, float innerRadius, float toothDepth);
```

### 圆弧段截面
```c
void loop_factory_arc_segment(geo::GeoData* geo, float cx, float cy, float innerRadius, float outerRadius, float startAngle, float endAngle, int samples);
```

---

## 实体工厂 (solid_factory_*)

### 基础实体

#### 立方体
```c
void solid_factory_box(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float depth);
```

#### 球体
```c
void solid_factory_sphere(geo::GeoData* geo, float cx, float cy, float cz, float radius, int rings, int sectors);
```

#### 圆柱体
```c
void solid_factory_cylinder(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int sectors, int rings);
```

#### 圆锥体
```c
void solid_factory_cone(geo::GeoData* geo, float cx, float cy, float cz, float baseRadius, float height, int sectors);
```

#### 圆台/锥台
```c
void solid_factory_cone_frustum(geo::GeoData* geo, float cx, float cy, float cz, float bottomRadius, float topRadius, float height, int sectors);
```

#### 圆环体
```c
void solid_factory_torus(geo::GeoData* geo, float cx, float cy, float cz, float majorRadius, float minorRadius, int rings, int sectors);
```

#### 胶囊体
```c
void solid_factory_capsule(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int rings, int sectors);
```

### 拉伸体

#### 矩形拉伸
```c
void solid_factory_extrude_rect(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float depth, int subdivisions);
```

#### 圆形拉伸
```c
void solid_factory_extrude_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, int samples, int subdivisions);
```

#### 多边形拉伸
```c
void solid_factory_extrude_polygon(geo::GeoData* geo, float cx, float cy, float cz, int sides, float radius, float height, int subdivisions);
```

#### 星形拉伸
```c
void solid_factory_extrude_star(geo::GeoData* geo, float cx, float cy, float cz, int points, float outerRadius, float innerRadius, float height, int subdivisions);
```

### 复杂实体

#### 锥度拉伸体
```c
void solid_factory_extrude_tapered_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float topScale, int samples, int subdivisions);
```

#### 扭曲拉伸体
```c
void solid_factory_extrude_twisted_circle(geo::GeoData* geo, float cx, float cy, float cz, float radius, float height, float twistAngle, int samples, int subdivisions);
```

#### 旋转体
```c
void solid_factory_revolve_rect(geo::GeoData* geo, float cx, float cy, float cz, float width, float height, float angle, int segments);
```

#### 弹簧
```c
void solid_factory_spring(geo::GeoData* geo, float cx, float cy, float cz, float radius, float wireRadius, float height, float turns, int segments, int wireSegments);
```

#### 齿轮
```c
void solid_factory_gear(geo::GeoData* geo, float cx, float cy, float cz, int teeth, float outerRadius, float innerRadius, float thickness, float toothDepth);
```

---

## 使用示例

### TCC脚本示例：创建一个带孔的齿轮

```c
void build(Entity entity, GeoData geo) {
    int teeth = entity_getInt(entity, "teeth");
    float outerRadius = entity_getFloat(entity, "outerRadius");
    float innerRadius = entity_getFloat(entity, "innerRadius");
    float thickness = entity_getFloat(entity, "thickness");
    float toothDepth = entity_getFloat(entity, "toothDepth");
    
    solid_factory_gear(geo, 0.0f, 0.0f, 0.0f, teeth, outerRadius, innerRadius, thickness, toothDepth);
}
```

### TCC脚本示例：创建一个螺旋楼梯

```c
void build(Entity entity, GeoData geo) {
    float radius = entity_getFloat(entity, "radius");
    float height = entity_getFloat(entity, "height");
    float turns = entity_getFloat(entity, "turns");
    int steps = entity_getInt(entity, "steps");
    
    // 创建螺旋线作为路径
    curve_factory_helix(geo, 0.0f, 0.0f, 0.0f, radius, height, turns, steps * 10);
    
    // 沿路径创建台阶（简化示例）
    for (int i = 0; i < steps; i++) {
        float t = (float)i / steps;
        float y = height * t;
        float angle = turns * 2 * CAPI_PI * t;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        
        solid_factory_box(geo, x, y, z, 1.0f, 0.2f, 0.5f);
    }
}
```

---

## 设计理念

工厂方法的设计遵循以下原则：

1. **简洁性**：一个函数调用即可创建复杂几何体
2. **可组合性**：通过组合基础形状构建复杂结构
3. **参数化**：所有参数都可以从Entity属性中读取
4. **一致性**：命名和参数顺序保持统一风格

## 内部实现

工厂方法内部调用 C++ 层的 `CurveFactory`、`LoopFactory`、`SolidFactory` 命名空间中的方法，然后将生成的几何数据转换为 GeoData 格式输出给TCC脚本。
