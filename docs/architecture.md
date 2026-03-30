# ScriptGeometry 系统架构

## 概述

ScriptGeometry是一个程序化几何系统，将AI聊天助手、TCC内存C编译器和OpenGL渲染器整合为一个统一的工作流程。

```
┌──────────────────────────────────────────────────────────────────┐
│                        宿主应用程序                               │
│                                                                    │
│  ┌──────────┐   ┌──────────────┐  ┌────────────┐  ┌──────────┐  │
│  │ Document │   │  ScriptLib   │  │ TccEngine  │  │ GlView   │  │
│  │ (geolib) │◄──│  (.toon)     │──►│ (libtcc)   │──►│ (OpenGL) │  │
│  └──────────┘   └──────────────┘  └────────────┘  └──────────┘  │
│       ▲                ▲                                          │
│       │                │                                          │
│  ┌────┴───────────────┴──────────────────────────────────────┐   │
│  │                    C API (capi.h)                          │   │
│  │  math_  curve_  loop_  path_  surface_  solid_            │   │
│  └──────────────────────────────────────────────────────────┘   │
│                            ▲                                      │
│                      ┌─────┴──────┐                              │
│                      │    Chat    │                               │
│                      │ Assistant  │                               │
│                      │  (curl)    │                               │
│                      └────────────┘                              │
└──────────────────────────────────────────────────────────────────┘
```

## 模块说明

### geolib (几何库)

核心数据结构和实体系统。所有几何数据都在这里管理；TCC脚本从不直接分配内存。关键类型：`Vertex`（顶点）、`GeoData`（几何数据）、`Entity`（实体）、`Document`（文档）。

### capi (C API)

暴露给TCC脚本的扁平C接口。所有函数按前缀分组（`math_`、`curve_`、`loop_`、`path_`、`surface_`、`solid_`）。脚本内部不需要包含头文件。

### toon (TOON格式)

`.toon`文件格式（文本面向对象表示法）的解析器和序列化器。每个文件包含元数据（`[meta]`）、属性模式（`[attrs]`）和C源码（`[code]`）。

### scriptlib (脚本库)

管理从`.toon`文件加载的`ScriptMeta`对象集合，支持热插拔。支持目录扫描、单文件重载和变更回调。

### tccengine (TCC引擎)

使用libtcc在内存中编译C脚本。编译后的脚本按源码哈希缓存。脚本在沙箱中运行：无`#include`、无`malloc`、无系统调用。

### chat (AI聊天助手)

四方对话系统（系统/TCC/AI/用户）。从角色、需求、上下文、规则、函数和示例构建结构化Prompt。通过libcurl与任何兼容OpenAI的LLM API通信。

### glview (OpenGL视图)

OpenGL 3.3渲染器。将`GeoData`上传到GPU缓冲区，支持实体/线框/点渲染模式，处理相机控制（轨道/平移/缩放）。

## 数据流

### 脚本编译与执行

```
TOON文件 ──► ToonParser ──► ScriptMeta
                                │
                                ▼
                          ScriptLib.register()
                                │
                                ▼
                          TccEngine.compile()
                                │
                                ▼
                          缓存的CompiledScript
                                │
            ┌───────────────────┴───────────────────┐
            ▼                                       ▼
    Entity实例1 (属性A)                      Entity实例2 (属性B)
            │                                       │
            ▼                                       ▼
    TccEngine.execute()                       TccEngine.execute()
            │                                       │
            ▼                                       ▼
    GeoData输出                               GeoData输出
            │                                       │
            └───────────────────┬───────────────────┘
                                ▼
                          GlView.uploadEntity()
                                │
                                ▼
                          OpenGL渲染
```

### Entity属性访问

TCC脚本通过C API访问Entity属性：

```c
void build(Entity entity, GeoData geo) {
    // 从Entity读取属性
    float radius = entity_getFloat(entity, "radius");
    int segments = entity_getInt(entity, "segments");
    
    // 生成几何
    solid_sphere(geo, 0, 0, 0, radius, segments/2, segments);
}
```

### AI对话流程

```
用户消息 ──► PromptBuilder构建系统提示
                    │
                    ▼
              Conversation.addMessage()
                    │
                    ▼
              ChatClient.sendAsync()
                    │
                    ▼
              LLM API请求
                    │
                    ▼
              AI响应文本
                    │
                    ▼
              extractToonBlock()
                    │
                    ▼
              ScriptLib.registerFromSource()
                    │
                    ▼
              新脚本注册完成
```

## 关键设计决策

### 1. Entity-Script分离

一个编译后的脚本可以服务多个Entity实例：

```
CompiledScript (编译一次)
    │
    ├── Entity 1 (radius=1.0, segments=16)
    ├── Entity 2 (radius=2.0, segments=24)
    └── Entity 3 (radius=1.5, segments=32)
```

### 2. 脚本沙箱

TCC脚本在受限环境中运行：
- 无文件系统访问
- 无网络访问
- 无动态内存分配
- 只能调用注册的C API函数

### 3. 几何数据所有权

`GeoData`始终由C++层管理：
- Entity拥有其GeoData
- 脚本通过指针写入GeoData
- GlView从GeoData读取并上传GPU

### 4. 脏标记机制

```cpp
class Entity {
    bool dirty_{true};  // 几何是否需要重建
    
    void markDirty() { dirty_ = true; }
    void clearDirty() { dirty_ = false; }
};
```

当属性变化时标记dirty，主循环检测并重建几何。

## 扩展点

### 添加新的几何工厂

1. 在`geolib.h`的工厂命名空间中声明函数
2. 在`geolib.cpp`中实现
3. 在`capi.h`中添加C接口声明
4. 在`capi.cpp`中实现封装
5. 在`capi_register_symbols()`中注册符号

### 添加新的属性类型

1. 扩展`AttrValue`变体类型
2. 更新JSON序列化代码
3. 添加对应的`entity_get*`函数
4. 更新TOON解析器

### 添加新的文件格式

1. 实现`import函数`
2. 实现`export函数`
3. 在UI中添加按钮调用

## 性能考虑

### 脚本编译

- 首次编译约10-50ms
- 缓存命中直接返回，约0.1ms
- 建议预编译常用脚本

### 几何生成

- 避免过高的细分参数
- 使用合理的顶点数量（建议<100K）
- 利用脏标记避免重复计算

### 渲染

- VBO/EBO静态绘制模式
- 实体按ID分离上传
- 支持线框模式简化显示
