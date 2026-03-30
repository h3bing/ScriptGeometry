/**
 * @file tccengine.h
 * @brief TCC-based in-memory C script compiler and executor
 *
 * Design constraints:
 *  - Scripts may NOT include headers (all symbols are pre-registered).
 *  - Scripts may NOT allocate memory; they receive a GeoDataHandle.
 *  - Scripts may NOT perform system or network calls.
 *  - Compiled scripts are cached by source hash.
 *  - Infinite-loop protection via a compile-time iteration counter injection.
 */

#pragma once

#include "geolib.h"
#include "capi.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

// Forward-declare TCC state opaquely (libtcc.h is only included in the .cpp)
struct TCCState;

namespace tcc_engine {

// ============================================================
//  Compiled script handle
// ============================================================

struct CompiledScript {
    using BuildFn = void (*)(GeoDataHandle);

    std::string  id;
    std::string  sourceHash;
    BuildFn      buildFn {nullptr};
    TCCState*    state   {nullptr};   ///< owning TCC state

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

    // Non-copyable
    TccEngine(const TccEngine&) = delete;
    TccEngine& operator=(const TccEngine&) = delete;

    /**
     * Compile a C script.
     * @param id       Script identifier (used for cache lookup).
     * @param source   Raw C source code (no #include allowed).
     * @return         Pointer to compiled script, or nullptr on error.
     */
    const CompiledScript* compile(const std::string& id,
                                  const std::string& source);

    /**
     * Execute the "build" function of a compiled script against the given GeoData.
     * @return true on success, false if script not found or error.
     */
    bool execute(const std::string& id, geo::GeoData& geoData);

    /// Remove a cached script
    bool evict(const std::string& id);

    /// Clear all cached scripts
    void clearCache();

    /// Number of cached scripts
    size_t cacheSize() const { return cache_.size(); }

    /// Last error string
    const std::string& lastError() const { return lastError_; }

    /// Maximum iterations guard (injected as a global counter reset per build call)
    void setMaxIterations(uint64_t n) { maxIterations_ = n; }

private:
    std::unordered_map<std::string, std::unique_ptr<CompiledScript>> cache_;
    std::string lastError_;
    uint64_t    maxIterations_{10'000'000};

    std::string wrapSource(const std::string& source) const;
    void        registerSymbols(TCCState* state) const;
    std::string hashSource(const std::string& s) const;
};

} // namespace tcc_engine
