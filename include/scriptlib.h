/**
 * @file scriptlib.h
 * @brief Script library – hot-pluggable collection of TOON scripts
 *
 * ScriptLib loads all *.toon files from a directory and keeps them in memory.
 * It supports hot-plug: calling reload() or watching file-system events will
 * update the in-memory list without restarting the host program.
 *
 * ScriptMeta holds both the attribute definitions and the compiled script code;
 * these two aspects are inseparable by design.
 */

#pragma once

#include "geolib.h"
#include "toon.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace script {

// ============================================================
//  ScriptMeta  – metadata + source code for one script
// ============================================================

struct ScriptMeta {
    std::string          id;
    std::string          name;
    std::string          category;
    std::string          version;
    std::string          author;
    std::string          description;

    std::vector<toon::AttrDef> attrDefs;   ///< attribute schema
    std::string                code;        ///< raw C source

    std::filesystem::path sourcePath;       ///< originating .toon file

    bool isValid() const { return !id.empty() && !code.empty(); }

    /// Convert toon::AttrDef defaults into an AttrMap suitable for Entity
    geo::Entity::AttrMap defaultAttrs() const;
};

// ============================================================
//  ScriptLib  – manages a collection of ScriptMeta
// ============================================================

class ScriptLib {
public:
    using ChangeCallback = std::function<void(const std::string& scriptId)>;

    ScriptLib() = default;

    /// Load all .toon files from the given directory
    bool loadDirectory(const std::filesystem::path& dir);

    /// Add a single .toon file
    bool loadFile(const std::filesystem::path& path);

    /// Remove script by id (hot-unplug)
    bool unloadScript(const std::string& id);

    /// Reload all scripts from their source paths
    void reload();

    /// Reload a single script by id
    bool reloadScript(const std::string& id);

    /// Find script by id
    const ScriptMeta* findScript(const std::string& id) const;

    /// All scripts
    const std::vector<ScriptMeta>& scripts() const { return scripts_; }

    /// Scripts in a given category
    std::vector<const ScriptMeta*> byCategory(const std::string& cat) const;

    /// Register callback called when scripts change
    void setChangeCallback(ChangeCallback cb) { changeCb_ = std::move(cb); }

    size_t size() const { return scripts_.size(); }

private:
    std::vector<ScriptMeta>             scripts_;
    std::map<std::string, size_t>       idIndex_;   ///< id -> index in scripts_
    ChangeCallback                      changeCb_;

    ScriptMeta fromToon(const toon::ToonDocument& doc,
                        const std::filesystem::path& path) const;
    void notifyChange(const std::string& id);
    void rebuildIndex();
};

} // namespace script
