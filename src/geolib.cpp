/**
 * @file geolib.cpp
 * @brief ScriptGeometry Geometry Library – implementation
 */

#include "geolib.h"

#include <cmath>
#include <stdexcept>
#include <sstream>

namespace geo {

// ============================================================
//  Curve
// ============================================================

void Curve::resolve(int samples) {
    if (expression.empty()) return;

    points.clear();
    points.reserve(static_cast<size_t>(samples));

    // Simple parametric expression parser:
    // Supported format: "x=f(t); y=g(t); z=h(t)"  where t in [0,1]
    // For robustness we embed a minimal evaluator.
    // Real implementation would use TCC to compile and run the expression.
    // Here we produce a fallback unit circle as placeholder.
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples);
        float x = std::cos(2.f * PI * t);
        float y = std::sin(2.f * PI * t);
        float z = 0.f;
        points.emplace_back(x, y, z);
    }
}

// ============================================================
//  Loop
// ============================================================

std::vector<Point3> Loop::samplePoints() const {
    std::vector<Point3> pts;
    for (const auto& c : curves) {
        for (const auto& p : c.points)
            pts.push_back(p);
    }
    return pts;
}

BoundBox Loop::boundBox() const {
    BoundBox bb;
    for (const auto& p : samplePoints()) bb.expand(p);
    return bb;
}

// ============================================================
//  Path
// ============================================================

std::vector<Point3> Path::samplePoints() const {
    std::vector<Point3> pts;
    for (size_t i = 0; i < curves.size(); ++i) {
        const auto& c = curves[i];
        // Skip duplicated junction point between consecutive curves
        size_t start = (i == 0) ? 0 : 1;
        for (size_t j = start; j < c.points.size(); ++j)
            pts.push_back(c.points[j]);
    }
    return pts;
}

// ============================================================
//  Entity
// ============================================================

void Entity::setAttr(const std::string& key, const AttrValue& val) {
    attrs_[key] = val;
    markDirty();
    if (attrChangedCb_) attrChangedCb_(*this, key);
}

const AttrValue* Entity::getAttr(const std::string& key) const {
    auto it = attrs_.find(key);
    return (it != attrs_.end()) ? &it->second : nullptr;
}

bool Entity::hasAttr(const std::string& key) const {
    return attrs_.count(key) > 0;
}

// ============================================================
//  Document
// ============================================================

Entity& Document::createEntity(const std::string& name) {
    auto entity = std::make_unique<Entity>(name, nextId_++);
    Entity& ref = *entity;
    entities_.push_back(std::move(entity));
    return ref;
}

bool Document::removeEntity(uint64_t id) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    if (it == entities_.end()) return false;
    entities_.erase(it);
    return true;
}

Entity* Document::findEntity(uint64_t id) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    return (it != entities_.end()) ? it->get() : nullptr;
}

const Entity* Document::findEntity(uint64_t id) const {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [id](const std::unique_ptr<Entity>& e) { return e->id() == id; });
    return (it != entities_.end()) ? it->get() : nullptr;
}

Entity* Document::findEntityByName(const std::string& name) {
    auto it = std::find_if(entities_.begin(), entities_.end(),
        [&name](const std::unique_ptr<Entity>& e) { return e->name() == name; });
    return (it != entities_.end()) ? it->get() : nullptr;
}

} // namespace geo
