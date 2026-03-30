# Entity 位姿功能

## 概述

Entity类现已支持完整的位姿（位置、旋转、缩放）操作接口，方便GUI进行位姿调整。

## API 参考

### 位置操作

```cpp
// 获取位置
geo::Point3 pos = entity.position();

// 设置位置
entity.setPosition(geo::Point3(x, y, z));
entity.setPosition(x, y, z);  // 简化形式

// 平移（相对移动）
entity.translate(geo::Vector3(dx, dy, dz));
entity.translate(dx, dy, dz);  // 简化形式
```

### 旋转操作

```cpp
// 获取旋转（欧拉角，弧度）
geo::Vector3 rot = entity.rotation();

// 设置旋转（欧拉角，弧度）
entity.setRotation(geo::Vector3(rx, ry, rz));

// 设置旋转（轴角）
entity.setRotationAxisAngle(axis, angle);

// 设置旋转（四元数）
entity.setRotationQuat(w, x, y, z);

// 相对旋转（绕自身轴）
entity.rotate(geo::Vector3(rx, ry, rz));
entity.rotateX(angle);
entity.rotateY(angle);
entity.rotateZ(angle);
```

### 缩放操作

```cpp
// 获取缩放
geo::Vector3 scl = entity.scale();

// 设置缩放
entity.setScale(geo::Vector3(sx, sy, sz));
entity.setScale(uniformScale);  // 均匀缩放

// 相对缩放
entity.scaleUniform(factor);
```

### 组合操作

```cpp
// 组合变换：位置、旋转、缩放 -> 矩阵
entity.composeTransform(pos, rot, scl);

// 分解变换：矩阵 -> 位置、旋转、缩放
entity.decomposeTransform(pos, rot, scl);

// 重置变换为单位矩阵
entity.resetTransform();
```

### 包围盒

```cpp
// 局部包围盒（不考虑变换）
geo::BoundBox bb = entity.boundBox();

// 世界包围盒（考虑变换）
geo::BoundBox worldBb = entity.worldBoundBox();
```

## 使用示例

### GUI位姿调整

```cpp
void drawTransformUI(geo::Entity& entity) {
    // 位置
    geo::Point3 pos = entity.position();
    float p[3] = {pos.x, pos.y, pos.z};
    if (ImGui::DragFloat3("Position", p, 0.01f)) {
        entity.setPosition(p[0], p[1], p[2]);
    }

    // 旋转（度数显示）
    geo::Vector3 rot = entity.rotation();
    float r[3] = {
        rot.x * 180.f / geo::PI,
        rot.y * 180.f / geo::PI,
        rot.z * 180.f / geo::PI
    };
    if (ImGui::DragFloat3("Rotation", r, 0.5f)) {
        entity.setRotation(geo::Vector3(
            r[0] * geo::PI / 180.f,
            r[1] * geo::PI / 180.f,
            r[2] * geo::PI / 180.f
        ));
    }

    // 缩放
    geo::Vector3 scl = entity.scale();
    float s[3] = {scl.x, scl.y, scl.z};
    if (ImGui::DragFloat3("Scale", s, 0.01f)) {
        entity.setScale(geo::Vector3(s[0], s[1], s[2]));
    }
}
```

### 创建物体并设置位姿

```cpp
// 创建球体
geo::Entity& sphere = doc.createEntity("Sphere");
sphere.setScriptId("sphere_solid");
sphere.setAttr("radius", 1.0f);

// 设置位置
sphere.setPosition(5.0f, 0.0f, 0.0f);

// 旋转45度绕Y轴
sphere.setRotation(geo::Vector3(0, geo::PI / 4, 0));

// 缩放为2倍
sphere.setScale(2.0f);
```

### 动画示例

```cpp
void animateEntity(geo::Entity& entity, float time) {
    // 绕Y轴旋转
    entity.setRotationY(time);

    // 上下浮动
    float y = std::sin(time) * 0.5f;
    entity.setPosition(entity.position().x, y, entity.position().z);

    // 呼吸缩放
    float scale = 1.0f + std::sin(time * 2) * 0.1f;
    entity.setScale(scale);
}
```

## 内部实现

### 变换顺序

变换矩阵按 **TRS** 顺序组合：先缩放(Scale)，再旋转(Rotate)，最后平移(Translate)。

```
M = T * R * S
```

这意味着：
- 缩放是相对于原点的
- 旋转是相对于原点的
- 平移是在世界空间中的

### 欧拉角约定

使用 **XYZ** 欧拉角顺序（Pitch-Yaw-Roll）：
- X轴旋转 = Pitch（俯仰）
- Y轴旋转 = Yaw（偏航）
- Z轴旋转 = Roll（翻滚）

### 依赖

使用GLM库的扩展功能：
- `glm/gtx/euler_angles.hpp` - 欧拉角转换
- `glm/gtx/quaternion.hpp` - 四元数支持
- `glm/gtx/matrix_decompose.hpp` - 矩阵分解

## 注意事项

1. **旋转单位**：内部使用弧度，GUI显示时转换为度数
2. **万向节锁**：欧拉角存在万向节锁问题，如需避免请使用四元数
3. **脏标记**：位姿变化会设置dirty标志，触发几何重建
4. **世界包围盒**：计算时会变换局部包围盒的8个角点
