/**
 * @file scriptlib.cpp
 * @brief Script library implementation
 */

#include "scriptlib.h"

#include <iostream>

namespace script {

// ============================================================
//  ScriptMeta
// ============================================================

geo::Entity::AttrMap ScriptMeta::defaultAttrs() const {
    geo::Entity::AttrMap map;
    for (const auto& def : attrDefs) {
        geo::AttrValue val;
        switch (def.type) {
            case toon::AttrType::Float:
                try { val = std::stof(def.defaultStr); }
                catch (...) { val = 0.f; }
                break;
            case toon::AttrType::Int:
                try { val = std::stoi(def.defaultStr); }
                catch (...) { val = 0; }
                break;
            case toon::AttrType::Bool:
                val = (def.defaultStr == "true" || def.defaultStr == "1");
                break;
            case toon::AttrType::Color: {
                // Format: "r,g,b,a"
                float r=1,g=1,b=1,a=1;
                sscanf(def.defaultStr.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a);
                val = geo::Color{r,g,b,a};
                break;
            }
            default:
                val = def.defaultStr;
                break;
        }
        map[def.name] = val;
    }
    return map;
}

// ============================================================
//  ScriptLib
// ============================================================

ScriptMeta ScriptLib::fromToon(const toon::ToonDocument& doc,
                                const std::filesystem::path& path) const {
    ScriptMeta m;
    m.id          = doc.id;
    m.name        = doc.name;
    m.category    = doc.category;
    m.version     = doc.version;
    m.author      = doc.author;
    m.description = doc.description;
    m.attrDefs    = doc.attrs;
    m.code        = doc.code;
    m.sourcePath  = path;
    return m;
}

void ScriptLib::rebuildIndex() {
    idIndex_.clear();
    for (size_t i = 0; i < scripts_.size(); ++i)
        idIndex_[scripts_[i].id] = i;
}

void ScriptLib::notifyChange(const std::string& id) {
    if (changeCb_) changeCb_(id);
}

bool ScriptLib::loadDirectory(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::cerr << "[ScriptLib] Directory not found: " << dir << "\n";
        return false;
    }
    bool anyLoaded = false;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".toon") {
            if (loadFile(entry.path())) anyLoaded = true;
        }
    }
    return anyLoaded;
}

bool ScriptLib::loadFile(const std::filesystem::path& path) {
    auto doc = toon::ToonParser::loadFile(path.string());
    if (!doc) {
        std::cerr << "[ScriptLib] Parse error (" << path.filename() << "): "
                  << toon::ToonParser::lastError() << "\n";
        return false;
    }
    // Remove existing script with same id (hot-plug replace)
    unloadScript(doc->id);

    ScriptMeta meta = fromToon(*doc, path);
    scripts_.push_back(std::move(meta));
    rebuildIndex();
    notifyChange(doc->id);
    return true;
}

bool ScriptLib::unloadScript(const std::string& id) {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return false;
    scripts_.erase(scripts_.begin() + static_cast<ptrdiff_t>(it->second));
    rebuildIndex();
    notifyChange(id);
    return true;
}

void ScriptLib::reload() {
    // Collect all source paths
    std::vector<std::filesystem::path> paths;
    for (const auto& m : scripts_)
        paths.push_back(m.sourcePath);
    scripts_.clear();
    idIndex_.clear();
    for (const auto& p : paths)
        loadFile(p);
}

bool ScriptLib::reloadScript(const std::string& id) {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return false;
    std::filesystem::path path = scripts_[it->second].sourcePath;
    unloadScript(id);
    return loadFile(path);
}

const ScriptMeta* ScriptLib::findScript(const std::string& id) const {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return nullptr;
    return &scripts_[it->second];
}

std::vector<const ScriptMeta*> ScriptLib::byCategory(const std::string& cat) const {
    std::vector<const ScriptMeta*> result;
    for (const auto& m : scripts_)
        if (m.category == cat) result.push_back(&m);
    return result;
}

} // namespace script
