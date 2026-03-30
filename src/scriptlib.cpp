/**
 * @file scriptlib.cpp
 * @brief 脚本库实现
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
                                const std::filesystem::path& path,
                                bool fromMemory) const {
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
    m.fromMemory  = fromMemory;
    return m;
}

void ScriptLib::rebuildIndex() {
    idIndex_.clear();
    for (size_t i = 0; i < scripts_.size(); ++i)
        idIndex_[scripts_[i].id] = i;
}

void ScriptLib::notifyChange(const std::string& id, bool added) {
    if (changeCb_) changeCb_(id, added);
}

// --------------------------------------------------------
//  文件加载
// --------------------------------------------------------

bool ScriptLib::loadFile(const std::filesystem::path& path) {
    auto doc = toon::ToonParser::loadFile(path.string());
    if (!doc) {
        std::cerr << "[ScriptLib] 解析失败 (" << path.filename() << "): "
                  << toon::ToonParser::lastError() << "\n";
        return false;
    }

    // 如果ID已存在，先移除
    bool isUpdate = hasScript(doc->id);
    unregister(doc->id);

    ScriptMeta meta = fromToon(*doc, path, false);
    scripts_.push_back(std::move(meta));
    rebuildIndex();
    notifyChange(doc->id, !isUpdate);
    return true;
}

int ScriptLib::loadDirectory(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::cerr << "[ScriptLib] 目录不存在: " << dir << "\n";
        return 0;
    }

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".toon") {
            if (loadFile(entry.path())) count++;
        }
    }
    return count;
}

// --------------------------------------------------------
//  内存注册（AI生成）
// --------------------------------------------------------

std::string ScriptLib::registerFromSource(const std::string& toonSource,
                                           const std::string& tag) {
    auto doc = toon::ToonParser::parse(toonSource);
    if (!doc) {
        std::cerr << "[ScriptLib] 内存脚本解析失败: "
                  << toon::ToonParser::lastError() << "\n";
        return "";
    }

    bool isUpdate = hasScript(doc->id);
    unregister(doc->id);

    ScriptMeta meta = fromToon(*doc, "", true);
    meta.sourceTag = tag;
    scripts_.push_back(std::move(meta));
    rebuildIndex();
    notifyChange(doc->id, !isUpdate);

    return doc->id;
}

bool ScriptLib::registerScript(const ScriptMeta& meta) {
    if (!meta.isValid()) return false;

    bool isUpdate = hasScript(meta.id);
    unregister(meta.id);

    scripts_.push_back(meta);
    rebuildIndex();
    notifyChange(meta.id, !isUpdate);
    return true;
}

// --------------------------------------------------------
//  取消注册
// --------------------------------------------------------

bool ScriptLib::unregister(const std::string& id) {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return false;

    scripts_.erase(scripts_.begin() + static_cast<ptrdiff_t>(it->second));
    rebuildIndex();
    notifyChange(id, false);
    return true;
}

// --------------------------------------------------------
//  查询
// --------------------------------------------------------

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

std::vector<std::string> ScriptLib::scriptIds() const {
    std::vector<std::string> ids;
    ids.reserve(scripts_.size());
    for (const auto& m : scripts_)
        ids.push_back(m.id);
    return ids;
}

// --------------------------------------------------------
//  热插拔
// --------------------------------------------------------

void ScriptLib::reloadAll() {
    // 收集所有文件路径
    std::vector<std::filesystem::path> paths;
    for (const auto& m : scripts_)
        if (!m.fromMemory && !m.sourcePath.empty())
            paths.push_back(m.sourcePath);

    clear();

    for (const auto& p : paths)
        loadFile(p);
}

bool ScriptLib::reloadScript(const std::string& id) {
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return false;

    const auto& meta = scripts_[it->second];
    if (meta.fromMemory || meta.sourcePath.empty()) return false;

    std::filesystem::path path = meta.sourcePath;
    return loadFile(path);
}

void ScriptLib::clear() {
    scripts_.clear();
    idIndex_.clear();
}

} // namespace script
