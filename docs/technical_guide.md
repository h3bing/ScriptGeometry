# ScriptGeometry 技术文档

## 目录

1. [系统概述](#1-系统概述)
2. [架构设计](#2-架构设计)
3. [核心模块详解](#3-核心模块详解)
4. [数据流程](#4-数据流程)
5. [TOON脚本格式](#5-toon脚本格式)
6. [C API参考](#6-c-api参考)
7. [属性绑定机制](#7-属性绑定机制)
8. [扩展开发指南](#8-扩展开发指南)

---

## 1. 系统概述

ScriptGeometry 是一个 **AI驱动的程序化几何系统**，结合了：

- **AI对话生成**: 通过自然语言与LLM交互生成几何脚本
- **实时编译**: TCC (Tiny C Compiler) 内存编译，亚毫秒级重建
- **OpenGL渲染**: 高性能3D可视化
- **属性热更新**: 修改参数即时看到效果

### 1.1 核心特性

| 特性 | 说明 |
|------|------|
| AI代码生成 | 与LLM对话生成TOON格式C脚本 |
| 内存编译 | TCC运行时编译，无需磁盘I/O |
| 属性系统 | 实时编辑实体参数，几何体重建即时生效 |
| 热插拔脚本 | 运行时添加/删除 .toon 文件 |
| OpenGL渲染 | 实体/线框/混合/点渲染模式，透视/正交投影 |
| 四方对话 | System/TCC/AI/User 对话模型 |
| 布尔运算 | Clipper2支持并集/交集/差集/异或 |
| 三角剖分 | earcut算法支持带孔洞多边形 |

### 1.2 技术栈

- **语言**: C++20
- **GUI**: Dear ImGui
- **渲染**: OpenGL 3.3
- **编译**: TCC (Tiny C Compiler)
- **布尔运算**: Clipper2
- **三角剖分**: earcut.hpp

---

## 2. 架构设计

### 2.1 模块关系图

```
┌────────────────────────────────────────────────────────────────────┐
│                          主应用程序                                  │
│                                                                    │
│  ┌──────────┐   ┌──────────────┐   ┌────────────┐   ┌──────────┐  │
│  │ Document │   │  ScriptLib   │   │ TccEngine  │   │  GlView  │  │
│  │ (geolib) │◄──│  (.toon)     │──►│ (libtcc)   │──►│ (OpenGL) │  │
│  └──────────┘   └──────────────┘   └────────────┘   └──────────┘  │
│       ▲                ▲                                          │
│       │                │                                          │
│  ┌────┴────────────────┴──────────────────────────────────────┐   │
│  │                      C API (capi.h)                         │   │
│  │  math_  curve_  loop_  path_  surface_  solid_             │   │
│  └────────────────────────────────────────────────────────────┘   │
│                            ▲                                       │
│                      ┌─────┴──────┐                               │
│                      │    Chat    │                                │
│                      │ Assistant  │                                │
│                      │  (libcurl) │                                │
│                      └────────────┘                                │
└────────────────────────────────────────────────────────────────────┘
```

### 2.2 文件结构

```
ScriptGeometry/
├── include/                    # 公共头文件
│   ├── geolib.h               # 几何数据结构 + Entity + Document
│   ├── capi.h                 # 暴露给TCC脚本的C API
│   ├── toon.h                 # TOON文件格式解析器/序列化器
│   ├── scriptlib.h            # 热插拔脚本库
│   ├── tccengine.h            # TCC内存编译器
│   ├── chat.h                 # AI聊天助手 (四方对话)
│   ├── glview.h               # OpenGL视口
│   └── mainwindow.h           # ImGui布局管理器
├── src/                        # 实现文件
├── scripts/                    # 内置 .toon 脚本
├── shaders/                    # GLSL着色器
├── third_party/               # 第三方库
│   ├── clipper2/              # 多边形布尔运算
│   ├── earcut/                # 三角剖分
│   ├── imgui/                 # GUI框架
│   ├── glad/                  # OpenGL加载器
│   └── tcc/                   # TCC编译器
├── docs/
│   └── architecture.md        # 架构文档
├── LICENSE                    # MIT许可证
└── CMakeLists.txt
```

---

## 3. 核心模块详解

### 3.1 geolib - 几何库

核心数据结构定义：

```cpp
namespace geo {

// 顶点
struct Vertex {
    Point3  pos;      // 位置
    Vector3 tangent;  // 切线
    Vector3 normal;   // 法线
    Vector2 uv;       // 纹理坐标
};

// 几何数据缓冲区 (GPU就绪)
struct GeoData {
    std::vector<Vertex>        vertices;   // 顶点数组
    std::vector<EdgeIndex>     edges;      // 边索引
    std::vector<TriangleIndex> triangles;  // 三角形索引
};

// 实体 (场景对象)
class Entity {
    std::string  name_;       // 名称
    uint64_t     id_;         // 唯一ID
    std::string  scriptId_;   // 关联的脚本ID
    AttrMap      attrs_;      // 属性映射
    Matrix4      transform_;  // 变换矩阵
    GeoData      geoData_;    // 几何数据
    bool         dirty_;      // 脏标记
};

// 文档 (场景)
class Document {
    std::vector<std::unique_ptr<Entity>> entities_;
    uint64_t nextId_;
};

} // namespace geo
```

### 3.2 capi - C API

暴露给TCC脚本的扁平C接口：

```cpp
// 不透明句柄
typedef void* GeoDataHandle;

// 数学函数
float math_sin(float x);
float math_cos(float x);
float math_clamp(float v, float lo, float hi);
float math_lerp(float a, float b, float t);

// 曲线函数
void curve_line(GeoDataHandle h, float x0, float y0, float z0, float x1, float y1, float z1);
void curve_circle(GeoDataHandle h, float cx, float cy, float cz, float r, int samples);

// 循环函数 (2D多边形)
void loop_rect(GeoDataHandle h, float cx, float cy, float width, float height);
void loop_union(GeoDataHandle h, const float* loop1, int count1, const float* loop2, int count2);
void loop_fill(GeoDataHandle h, const float* pts, int count);

// 实体函数 (3D)
void solid_box(GeoDataHandle h, float cx, float cy, float cz, float wx, float wy, float wz);
void solid_sphere(GeoDataHandle h, float cx, float cy, float cz, float r, int rings, int sectors);
```

### 3.3 tccengine - TCC引擎

```cpp
namespace tcc_engine {

// 属性参数
struct AttrParam {
    std::string name;
    enum Type { Float, Int, Bool } type;
    float  floatVal;
    int    intVal;
    bool   boolVal;
};

// 编译后的脚本
struct CompiledScript {
    using BuildFn = void (*)(GeoDataHandle);
    BuildFn buildFn;
    TCCState* state;
    std::vector<AttrParam> attrDefs;
};

// TCC引擎
class TccEngine {
    // 编译脚本（带属性定义）
    const CompiledScript* compile(const std::string& id,
                                   const std::string& source,
                                   const std::vector<AttrParam>& attrs);

    // 执行脚本（带属性值）
    bool execute(const std::string& id, geo::GeoData& geoData,
                  const std::vector<AttrParam>& attrValues);
};

} // namespace tcc_engine
```

### 3.4 chat - AI聊天助手

```cpp
namespace chat {

// 模型配置
struct ModelConfig {
    std::string name{"gpt-4o"};
    std::string baseUrl{"https://api.openai.com/v1"};
    std::string apiKey;
    float temperature{0.3f};
    int maxTokens{4096};
};

// 消息角色
enum class Role { System, TCC, AI, User };

// 对话
class Conversation {
    void addUserMessage(const std::string& c);
    void addAiMessage(const std::string& c);
    void addTccMessage(const std::string& c);
    std::string toJson() const;
};

// 聊天客户端
class ChatClient {
    std::string sendSync(const std::string& systemPrompt, const Conversation& conv);
    void sendAsync(...);
};

// 聊天助手
class ChatAssistant {
    std::string chat(const std::string& userMessage);
    void chatAsync(const std::string& userMessage, Callback onReply);
};

// 从AI回复中提取TOON块
std::optional<std::string> extractToonBlock(const std::string& aiReply);

} // namespace chat
```

---

## 4. 数据流程

### 4.1 AI生成流程

```
用户输入自然语言
       │
       ▼
┌──────────────────┐
│ PromptBuilder    │ ← 构建系统提示词
│ (角色/规则/函数) │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ ChatClient       │ ← HTTP POST到LLM API
│ (libcurl)        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ AI回复           │ ← 包含 ```toon ... ``` 代码块
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ extractToonBlock │ ← 提取TOON内容
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ ToonParser       │ ← 解析[meta]/[attrs]/[code]
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ ScriptLib        │ ← 注册脚本
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ TccEngine        │ ← 编译C代码
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ Entity创建       │ ← 应用默认属性
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ GlView渲染       │ ← 上传GPU并显示
└──────────────────┘
```

### 4.2 属性修改流程

```
用户修改属性值
       │
       ▼
┌──────────────────┐
│ onAttrChanged()  │ ← 触发脏标记
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ update()循环检测 │ ← 发现dirty实体
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ rebuildEntity    │
│ Geometry()       │
└────────┬─────────┘
         │
         ├─► 获取Entity属性
         │
         ├─► 转换为AttrParam列表
         │
         ├─► TccEngine.execute(id, geoData, attrs)
         │   │
         │   └─► 更新脚本中的全局变量 attr_xxx
         │       调用 build(geoDataHandle)
         │
         ▼
┌──────────────────┐
│ GlView.upload    │ ← 上传新几何数据到GPU
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ 下一帧渲染       │ ← 显示更新后的几何体
└──────────────────┘
```

---

## 5. TOON脚本格式

### 5.1 格式规范

```toon
[meta]
id          = "sphere_solid"      # 唯一标识符
name        = "球体"               # 显示名称
category    = "solid"             # 类别: solid/surface/curve
version     = "1.0"
author      = "ScriptGeometry"
description = "UV球体"

[attrs]
# 名称  类型    默认值    "标签"     "描述"
radius   float   1.0      "半径"     "球体半径"
rings    int     24       "环数"     "纬度细分"
sectors  int     48       "扇区数"   "经度细分"

[code]
void build(GeoDataHandle h) {
    // 使用 attr_xxx 全局变量访问属性值
    solid_sphere(h, 0.0f, 0.0f, 0.0f, attr_radius, attr_rings, attr_sectors);
}
```

### 5.2 属性类型

| 类型 | C变量类型 | 示例默认值 |
|------|----------|-----------|
| float | float | `1.0` |
| int | int | `32` |
| bool | int | `true` 或 `1` |
| string | const char* | `"hello"` |
| color | float[4] | `"1,0,0,1"` |
| enum | int | `0` |

### 5.3 脚本规则

1. **必须**定义 `void build(GeoDataHandle h)` 函数
2. **禁止**使用 `#include`
3. **禁止**使用 `malloc`/`free`
4. **禁止**系统/网络调用
5. 只能使用 `capi.h` 中声明的函数
6. 属性值通过 `attr_<属性名>` 全局变量访问

---

## 6. C API参考

### 6.1 数学函数

```c
float math_sin(float x);
float math_cos(float x);
float math_tan(float x);
float math_sqrt(float x);
float math_pow(float base, float exp);
float math_abs(float x);
float math_floor(float x);
float math_ceil(float x);
float math_round(float x);
float math_clamp(float v, float lo, float hi);
float math_lerp(float a, float b, float t);
float math_deg2rad(float deg);
float math_rad2deg(float rad);
```

### 6.2 曲线函数

```c
// 直线
void curve_line(GeoDataHandle h,
                 float x0, float y0, float z0,
                 float x1, float y1, float z1);

// 折线
void curve_polyline(GeoDataHandle h, const float* pts, int count);

// 弧
void curve_arc(GeoDataHandle h,
                float cx, float cy, float cz,
                float r, float startAngle, float endAngle, int samples);

// 圆
void curve_circle(GeoDataHandle h,
                   float cx, float cy, float cz,
                   float r, int samples);

// 贝塞尔曲线
void curve_bezier(GeoDataHandle h, const float* pts, int count, int samples);

// Catmull-Rom样条
void curve_catmull(GeoDataHandle h, const float* pts, int count, int samples);
```

### 6.3 循环函数 (2D多边形)

```c
// 基础形状
void loop_rect(GeoDataHandle h, float cx, float cy, float width, float height);
void loop_polygon(GeoDataHandle h, float cx, float cy, float r, int sides);
void loop_circle(GeoDataHandle h, float cx, float cy, float r, int samples);
void loop_ellipse(GeoDataHandle h, float cx, float cy, float rx, float ry, int samples);
void loop_roundrect(GeoDataHandle h, float cx, float cy, float w, float h, float radius, int samples);

// 布尔运算 (Clipper2)
void loop_union(GeoDataHandle h, const float* loop1, int count1, const float* loop2, int count2);
void loop_intersect(GeoDataHandle h, const float* loop1, int count1, const float* loop2, int count2);
void loop_difference(GeoDataHandle h, const float* loop1, int count1, const float* loop2, int count2);
void loop_xor(GeoDataHandle h, const float* loop1, int count1, const float* loop2, int count2);

// 偏移
void loop_offset(GeoDataHandle h, const float* pts, int count, float delta, int joinType);

// 三角剖分填充 (earcut)
void loop_fill(GeoDataHandle h, const float* pts, int count);
void loop_fill_with_holes(GeoDataHandle h, const float* outer, int outerCount,
                           const float* holes, const int* holeCounts, int numHoles);

// 简化
void loop_simplify(GeoDataHandle h, const float* pts, int count, float epsilon);
```

### 6.4 实体函数 (3D)

```c
void solid_box(GeoDataHandle h, float cx, float cy, float cz, float wx, float wy, float wz);
void solid_sphere(GeoDataHandle h, float cx, float cy, float cz, float r, int rings, int sectors);
void solid_cylinder(GeoDataHandle h, float cx, float cy, float cz, float r, float height, int sectors, int rings);
void solid_cone(GeoDataHandle h, float cx, float cy, float cz, float r, float height, int sectors);
void solid_torus(GeoDataHandle h, float cx, float cy, float cz, float R, float r, int rings, int sectors);
void solid_capsule(GeoDataHandle h, float cx, float cy, float cz, float r, float height, int rings, int sectors);
```

---

## 7. 属性绑定机制

### 7.1 工作原理

属性绑定通过以下步骤实现：

1. **编译时**: TOON脚本的 `[attrs]` 部分被解析为 `AttrParam` 列表
2. **代码注入**: TCC引擎在脚本前添加全局变量声明：
   ```c
   float attr_radius = 1.0f;
   int attr_rings = 24;
   ```
3. **符号注册**: 这些变量的地址被注册到TCC状态
4. **执行时**: 属性值更新时，直接修改这些全局变量的值

### 7.2 示例

TOON脚本：
```toon
[attrs]
radius  float  1.0  "半径"

[code]
void build(GeoDataHandle h) {
    solid_sphere(h, 0, 0, 0, attr_radius, 16, 32);
}
```

用户在UI中修改 `radius` 为 `2.0`：
```cpp
// TccEngine::execute() 内部
float* pRadius = (float*)tcc_get_symbol(state, "attr_radius");
*pRadius = 2.0f;  // 更新全局变量
build(geoDataHandle);  // 调用build函数
```

---

## 8. 扩展开发指南

### 8.1 添加新的C API函数

1. **声明函数** (`include/capi.h`):
   ```c
   void loop_myfunction(GeoDataHandle h, float param);
   ```

2. **实现函数** (`src/capi.cpp`):
   ```cpp
   void loop_myfunction(GeoDataHandle h, float param) {
       auto* gd = toGD(h);
       // 实现逻辑
   }
   ```

3. **注册符号** (`src/tccengine.cpp`):
   ```cpp
   tcc_add_symbol(s, "loop_myfunction", (void*)loop_myfunction);
   ```

4. **添加声明** (`src/tccengine.cpp` wrapSource):
   ```cpp
   << "void loop_myfunction(GeoDataHandle,float);\n"
   ```

5. **更新AI提示词** (`src/chat.cpp` buildDefaultFunctions):
   ```cpp
   "Loop: ... loop_myfunction\n"
   ```

### 8.2 添加新的属性类型

1. 扩展 `AttrParam::Type` 枚举
2. 更新 `wrapSource()` 中的变量声明
3. 更新 `execute()` 中的值设置逻辑
4. 更新 `ToonParser` 的类型解析

### 8.3 构建项目

```bash
# 安装依赖 (vcpkg推荐)
vcpkg install glfw3 glm libcurl

# 配置
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build --parallel

# 运行
./build/bin/ScriptGeometry
```

---

## 附录

### A. 错误排查

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| TCC编译失败 | 语法错误/未声明符号 | 检查 lastError_ 字符串 |
| 属性不生效 | 属性名不匹配 | 确保 `attr_<name>` 与 `[attrs]` 中定义一致 |
| 渲染空白 | GeoData为空 | 检查 build() 函数是否正确调用 API |
| AI回复无TOON块 | 提示词不清晰 | 检查规则设置，确保要求返回 ```toon``` 格式 |

### B. 性能建议

- 使用 `evict()` 清理不再使用的脚本
- 对于复杂模型，适当降低细分参数
- 布尔运算对复杂多边形较慢，考虑预处理简化

### C. 许可证

本项目采用 MIT 许可证，第三方库许可证：

| 库 | 许可证 |
|---|---|
| Clipper2 | BSL-1.0 |
| earcut.hpp | ISC |
| ImGui | MIT |
| GLM | MIT |
| GLFW | zlib |
