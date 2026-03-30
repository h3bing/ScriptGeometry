/**
 * @file toon.h
 * @brief TOON (Text Object-Oriented Notation) parser/serializer
 *
 * TOON is a simple, human-readable format combining metadata and code blocks.
 * A TOON file has the following structure:
 *
 *   [meta]
 *   id       = "cone_solid"
 *   name     = "Cone"
 *   category = "solid"
 *   version  = "1.0"
 *
 *   [attrs]
 *   radius   float  1.0   "Radius"
 *   height   float  2.0   "Height"
 *   sectors  int    32    "Sectors"
 *   color    color  "1,0,0,1"  "Color"
 *
 *   [code]
 *   void build(GeoDataHandle h, float radius, float height, int sectors) {
 *       solid_cone(h, 0,0,0, radius, height, sectors);
 *   }
 */

#pragma once

#include <map>
#include <string>
#include <vector>
#include <optional>

namespace toon {

// ============================================================
//  Attribute definition (mirrors geo::AttrDef)
// ============================================================

enum class AttrType { Float, Int, Bool, String, Enum, Color };

struct AttrDef {
    std::string name;
    AttrType    type   {AttrType::Float};
    std::string defaultStr;   ///< raw default value as string
    std::string label;
    std::string description;
};

// ============================================================
//  ToonDocument – parsed representation of one .toon file
// ============================================================

struct ToonDocument {
    // [meta]
    std::string id;
    std::string name;
    std::string category;
    std::string version   {"1.0"};
    std::string author;
    std::string description;

    // [attrs]
    std::vector<AttrDef> attrs;

    // [code]
    std::string code;   ///< raw C source

    bool isValid() const { return !id.empty() && !code.empty(); }
};

// ============================================================
//  Parser
// ============================================================

class ToonParser {
public:
    /// Parse TOON source text; returns nullopt on failure
    static std::optional<ToonDocument> parse(const std::string& source);

    /// Load from file path
    static std::optional<ToonDocument> loadFile(const std::string& path);

    /// Last error message
    static const std::string& lastError();

private:
    static std::string lastError_;
};

// ============================================================
//  Serializer
// ============================================================

class ToonSerializer {
public:
    static std::string serialize(const ToonDocument& doc);
    static bool        saveFile(const ToonDocument& doc, const std::string& path);
};

} // namespace toon
