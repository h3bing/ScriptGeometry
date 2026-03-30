/**
 * @file scriptlib.h
 * @brief 脚本库 - 支持注册制和热插拔的脚本管理系统
 *
 * 设计原则：
 * - 脚本可以从文件加载，也可以从内存字符串注册（AI生成）
 * - 脚本只需解析和编译一次，可被多个Entity实例使用
 * - 支持热插拔：运行时添加/删除/更新脚本
 * - 每个脚本有唯一的ID，用于查找和缓存
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
//  ScriptMeta - 脚本元数据
// ============================================================

struct ScriptMeta {
    // [meta] 元数据
    std::string id;           ///< 唯一标识符
    std::string name;         ///< 显示名称
    std::string category;     ///< 类别: solid/surface/curve
    std::string version{"1.0"};
    std::string author;
    std::string description;

    // [attrs] 属性定义
    std::vector<toon::AttrDef> attrDefs;

    // [code] 源代码
    std::string code;         ///< 原始C源码

    // 来源信息
    std::filesystem::path sourcePath;  ///< 文件路径（文件加载时）
    bool fromMemory{false};            ///< 是否来自内存（AI生成）
    std::string sourceTag;             ///< 来源标识（如 "ai_generated"）

    bool isValid() const { return !id.empty() && !code.empty(); }

    /// 获取属性默认值映射
    geo::Entity::AttrMap defaultAttrs() const;

    /// 检查是否有属性定义
    bool hasAttrs() const { return !attrDefs.empty(); }
};

// ============================================================
//  ScriptLib - 脚本库管理器
// ============================================================

class ScriptLib {
public:
    using ChangeCallback = std::function<void(const std::string& scriptId, bool added)>;

    ScriptLib() = default;

    // --------------------------------------------------------
    //  脚本注册（核心功能）
    // --------------------------------------------------------

    /**
     * 从文件加载脚本并注册
     * @param path TOON文件路径
     * @return 成功返回true
     */
    bool loadFile(const std::filesystem::path& path);

    /**
     * 从目录加载所有脚本
     * @param dir 目录路径
     * @return 加载的脚本数量
     */
    int loadDirectory(const std::filesystem::path& dir);

    /**
     * 从内存字符串注册脚本（AI生成）
     * @param toonSource TOON格式的源码字符串
     * @param tag 来源标识（可选）
     * @return 注册成功返回脚本ID，失败返回空字符串
     */
    std::string registerFromSource(const std::string& toonSource,
                                    const std::string& tag = "memory");

    /**
     * 直接注册脚本元数据
     * @param meta 脚本元数据
     * @return 成功返回true
     */
    bool registerScript(const ScriptMeta& meta);

    /**
     * 取消注册脚本
     * @param id 脚本ID
     * @return 成功返回true
     */
    bool unregister(const std::string& id);

    // --------------------------------------------------------
    //  查询
    // --------------------------------------------------------

    /// 按ID查找脚本
    const ScriptMeta* findScript(const std::string& id) const;

    /// 检查脚本是否存在
    bool hasScript(const std::string& id) const { return idIndex_.count(id) > 0; }

    /// 获取所有脚本
    const std::vector<ScriptMeta>& scripts() const { return scripts_; }

    /// 获取指定类别的脚本
    std::vector<const ScriptMeta*> byCategory(const std::string& cat) const;

    /// 获取脚本数量
    size_t size() const { return scripts_.size(); }

    /// 获取脚本ID列表
    std::vector<std::string> scriptIds() const;

    // --------------------------------------------------------
    //  热插拔
    // --------------------------------------------------------

    /// 重新加载所有脚本（从原文件）
    void reloadAll();

    /// 重新加载指定脚本
    bool reloadScript(const std::string& id);

    /// 设置变更回调
    void setChangeCallback(ChangeCallback cb) { changeCb_ = std::move(cb); }

    /// 清空所有脚本
    void clear();

private:
    std::vector<ScriptMeta>       scripts_;
    std::map<std::string, size_t> idIndex_;
    ChangeCallback                changeCb_;

    ScriptMeta fromToon(const toon::ToonDocument& doc,
                        const std::filesystem::path& path,
                        bool fromMemory) const;

    void rebuildIndex();
    void notifyChange(const std::string& id, bool added);
};

} // namespace script
