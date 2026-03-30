/**
 * @file tccengine.h
 * @brief 基于TCC的内存C脚本编译器和执行器
 *
 * 设计原则：
 * - 脚本只需编译一次，可被多次执行（多实例）
 * - 编译后的脚本缓存在内存中，按脚本ID索引
 * - 脚本通过 EntityHandle 访问实体属性
 * - 脚本直接输出几何数据到 GeoData
 *
 * 脚本-实体关系：
 *   Script A (编译一次) ──► Entity 1 (属性: r=1)
 *                    ├──► Entity 2 (属性: r=2)
 *                    └──► Entity 3 (属性: r=3)
 */

#pragma once

#include "geolib.h"
#include "capi.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明 TCC 状态
struct TCCState;

namespace tcc_engine {

// ============================================================
//  编译后的脚本
// ============================================================

struct CompiledScript {
    using BuildFn = void (*)(geo::Entity* entity, geo::GeoData* geoData);

    std::string              id;           ///< 脚本ID
    std::string              sourceHash;   ///< 源码哈希（用于检测变化）
    BuildFn                  buildFn{nullptr};
    TCCState*                state{nullptr};
    std::vector<std::string> attrNames;    ///< 属性名列表

    ~CompiledScript();

    bool isValid() const { return buildFn != nullptr; }
};

// ============================================================
//  TccEngine
// ============================================================

class TccEngine {
public:
    TccEngine();
    ~TccEngine();

    // 禁止拷贝
    TccEngine(const TccEngine&) = delete;
    TccEngine& operator=(const TccEngine&) = delete;

    // --------------------------------------------------------
    //  编译
    // --------------------------------------------------------

    /**
     * 编译脚本
     * @param id        脚本ID（用于缓存查找）
     * @param source    C源代码
     * @param attrNames 属性名列表（用于生成属性访问函数声明）
     * @return          编译后的脚本指针，失败返回nullptr
     *
     * 如果脚本ID已存在且源码哈希相同，直接返回缓存的脚本。
     */
    const CompiledScript* compile(const std::string& id,
                                   const std::string& source,
                                   const std::vector<std::string>& attrNames = {});

    /**
     * 检查脚本是否已编译
     */
    bool isCompiled(const std::string& id) const;

    /**
     * 获取已编译的脚本
     */
    const CompiledScript* getCompiled(const std::string& id) const;

    // --------------------------------------------------------
    //  执行
    // --------------------------------------------------------

    /**
     * 执行脚本
     * @param id       脚本ID
     * @param entity   实体指针（脚本从中读取属性）
     * @param geoData  几何数据输出缓冲区
     * @return         成功返回true
     *
     * 脚本的 build(Entity* entity, GeoData* geoData) 函数会被调用。
     * 脚本通过 entity_getFloat(entity, "name") 等函数读取属性。
     */
    bool execute(const std::string& id, geo::Entity* entity, geo::GeoData& geoData);

    /**
     * 执行已编译的脚本
     */
    bool execute(const CompiledScript* script, geo::Entity* entity, geo::GeoData& geoData);

    // --------------------------------------------------------
    //  缓存管理
    // --------------------------------------------------------

    /// 移除缓存的脚本
    bool evict(const std::string& id);

    /// 清空所有缓存
    void clearCache();

    /// 缓存大小
    size_t cacheSize() const { return cache_.size(); }

    /// 最后的错误信息
    const std::string& lastError() const { return lastError_; }

private:
    std::unordered_map<std::string, std::unique_ptr<CompiledScript>> cache_;
    std::string lastError_;

    /// 生成包装代码
    std::string wrapSource(const std::string& source,
                           const std::vector<std::string>& attrNames) const;

    /// 注册符号
    void registerSymbols(TCCState* state) const;

    /// 计算源码哈希
    std::string hashSource(const std::string& s) const;
};

} // namespace tcc_engine
