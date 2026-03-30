# ScriptGeometry 功能更新文档

## 本次更新内容

### 1. STL导入导出功能

#### Entity STL支持

在 `geo::StlIo` 命名空间中提供了完整的STL文件导入导出功能：

```cpp
namespace geo::StlIo {

// 导出GeoData为ASCII STL格式
bool exportAscii(const GeoData& geo, const std::string& filepath, const std::string& name = "solid");

// 导出GeoData为二进制STL格式
bool exportBinary(const GeoData& geo, const std::string& filepath, const std::string& name = "solid");

// 导入STL文件到GeoData（自动检测ASCII/二进制）
bool import(GeoData& geo, const std::string& filepath);

// 从Entity导出STL
bool exportEntityAscii(const Entity& entity, const std::string& filepath);
bool exportEntityBinary(const Entity& entity, const std::string& filepath);

// 导入STL到Entity
bool importEntity(Entity& entity, const std::string& filepath);

} // namespace geo::StlIo
```

#### 使用示例

```cpp
// 导出实体为ASCII STL
geo::Entity& sphere = doc.createEntity("Sphere");
// ... 生成几何数据 ...
geo::StlIo::exportEntityAscii(sphere, "output/sphere.stl");

// 导入STL文件
geo::Entity& imported = doc.createEntity("Imported");
geo::StlIo::importEntity(imported, "input/part.stl");
```

---

### 2. JSON序列化功能

#### Document JSON支持

`geo::Document` 类现在支持完整的JSON序列化：

```cpp
class Document {
public:
    // 序列化到JSON字符串
    std::string toJsonString(int indent = -1) const;

    // 从JSON字符串反序列化
    bool fromJsonString(const std::string& json);

    // 保存到文件
    bool saveToFile(const std::string& filepath) const;

    // 从文件加载
    bool loadFromFile(const std::string& filepath);

    // 获取/设置文件路径
    const std::string& filePath() const;
    void setFilePath(const std::string& path);

    // 修改状态追踪
    bool isModified() const;
    void markModified();
    void clearModified();
};
```

#### JSON格式

```json
{
  "version": 1,
  "type": "ScriptGeometry",
  "entities": [
    {
      "id": 1,
      "name": "Sphere_1",
      "scriptId": "sphere_solid",
      "transform": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
      "attrs": {
        "radius": ["float", 1.5],
        "segments": ["int", 32]
      },
      "geoData": {
        "vertices": [...],
        "edges": [...],
        "triangles": [...]
      }
    }
  ]
}
```

---

### 3. UI文档管理

#### 新增按钮

在文档面板中添加了以下按钮：

| 按钮 | 功能 |
|------|------|
| **New** | 新建文档，清除所有实体 |
| **Open** | 打开JSON文档 |
| **Save** | 保存文档（如有路径则直接保存） |
| **SaveAs** | 另存为新文件 |
| **Import STL** | 导入STL文件到选中实体 |
| **Export STL** | 导出选中实体为STL |

#### 对话框

- **导出对话框**：可选择ASCII或Binary格式
- **导入对话框**：输入STL文件路径导入

---

### 4. 几何库完整性检查

#### CurveFactory（曲线工厂）

| 函数 | 功能 | 参数 |
|------|------|------|
| `line` | 直线 | 起点, 终点 |
| `arc` | 圆弧 | 圆心, 半径, 起止角 |
| `circle` | 整圆 | 圆心, 半径 |
| `ellipseArc` | 椭圆弧 | 圆心, rx, ry, 起止角 |
| `ellipse` | 整椭圆 | 圆心, rx, ry |
| `quadraticBezier` | 二次贝塞尔 | 3控制点 |
| `cubicBezier` | 三次贝塞尔 | 4控制点 |
| `catmullRom` | Catmull-Rom样条 | 控制点序列 |
| `helix` | 螺旋线 | 圆心, 半径, 高度, 圈数 |
| `conicHelix` | 圆锥螺旋 | 圆心, 起止半径, 高度, 圈数 |
| `sineWave` | 正弦波 | 起点, 终点, 振幅, 频率 |

#### LoopFactory（截面工厂）

| 函数 | 功能 |
|------|------|
| `rectangle` | 矩形 |
| `square` | 正方形 |
| `circle` | 圆 |
| `ellipse` | 椭圆 |
| `roundedRectangle` | 圆角矩形 |
| `regularPolygon` | 正多边形 |
| `star` | 星形 |
| `gear` | 齿轮轮廓 |
| `arcSegment` | 圆弧段 |
| `fromPoints` | 从点创建 |
| `booleanUnion/Intersect/Difference/Xor` | 布尔运算 |
| `offset` | 偏移 |

#### SolidFactory（实体工厂）

| 类别 | 函数 |
|------|------|
| 基础实体 | `box`, `sphere`, `cylinder`, `cone`, `coneFrustum`, `torus`, `capsule` |
| 拉伸体 | `extrude`, `extrudeBoth`, `extrudeTapered`, `extrudeTwisted` |
| 旋转体 | `revolve` |
| 扫掠体 | `sweep`, `sweepTwisted` |
| 放样 | `loft` |
| 特殊 | `spring`, `thread`, `gear`, `pipe`, `thickShell` |

---

### 5. C API完整性

所有工厂方法已封装到C接口层，TCC脚本可直接调用：

```c
// 曲线工厂
void curve_factory_line(geo, x0, y0, z0, x1, y1, z1);
void curve_factory_arc(geo, cx, cy, cz, r, startA, endA, samples);
void curve_factory_helix(geo, cx, cy, cz, radius, height, turns, samples);

// 截面工厂
void loop_factory_rect(geo, cx, cy, w, h);
void loop_factory_circle(geo, cx, cy, r, samples);
void loop_factory_star(geo, cx, cy, points, outerR, innerR);

// 实体工厂
void solid_factory_box(geo, cx, cy, cz, w, h, d);
void solid_factory_sphere(geo, cx, cy, cz, r, rings, sectors);
void solid_factory_extrude_circle(geo, cx, cy, cz, r, h, samples, subd);
void solid_factory_spring(geo, cx, cy, cz, radius, wireR, h, turns, segs, wireSegs);
```

---

## 依赖更新

新增依赖：
- **nlohmann/json** - JSON序列化库（header-only）

已添加到 `third_party/json/` 目录。

## CMakeLists.txt 更新

```cmake
# nlohmann/json (JSON serialization, header-only)
set(JSON_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/json)

target_include_directories(ScriptGeometry PRIVATE
    ...
    ${JSON_INCLUDE}
)
```

---

## 文件变更列表

| 文件 | 变更 |
|------|------|
| `include/geolib.h` | 添加StlIo命名空间、Document JSON方法 |
| `src/geolib.cpp` | 实现STL导入导出、JSON序列化 |
| `include/mainwindow.h` | 添加文档管理状态和方法 |
| `src/mainwindow.cpp` | 实现文档管理UI和功能 |
| `include/glview.h` | 添加clearAll方法 |
| `src/glview.cpp` | 实现clearAll方法 |
| `CMakeLists.txt` | 添加json库路径 |
| `third_party/json/nlohmann/json.hpp` | 新增nlohmann/json库 |
