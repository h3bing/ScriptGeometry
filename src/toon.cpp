/**
 * @file toon.cpp
 * @brief TOON parser/serializer implementation
 */

#include "toon.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace toon {

std::string ToonParser::lastError_;

const std::string& ToonParser::lastError() { return lastError_; }

// ============================================================
//  Helpers
// ============================================================

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

// Split by first occurrence of delimiter
static std::pair<std::string,std::string> splitFirst(const std::string& s, char delim) {
    size_t pos = s.find(delim);
    if (pos == std::string::npos) return {s, {}};
    return {trim(s.substr(0, pos)), trim(s.substr(pos + 1))};
}

static AttrType parseAttrType(const std::string& t) {
    if (t == "float")  return AttrType::Float;
    if (t == "int")    return AttrType::Int;
    if (t == "bool")   return AttrType::Bool;
    if (t == "string") return AttrType::String;
    if (t == "enum")   return AttrType::Enum;
    if (t == "color")  return AttrType::Color;
    return AttrType::Float;
}

static std::string attrTypeName(AttrType t) {
    switch (t) {
        case AttrType::Float:  return "float";
        case AttrType::Int:    return "int";
        case AttrType::Bool:   return "bool";
        case AttrType::String: return "string";
        case AttrType::Enum:   return "enum";
        case AttrType::Color:  return "color";
    }
    return "float";
}

// ============================================================
//  Parse
// ============================================================

std::optional<ToonDocument> ToonParser::parse(const std::string& source) {
    ToonDocument doc;

    enum class Section { None, Meta, Attrs, Code };
    Section section = Section::None;

    std::istringstream ss(source);
    std::string line;
    std::ostringstream codeStream;

    while (std::getline(ss, line)) {
        std::string t = trim(line);

        // Skip blank lines and comments outside code block
        if (section != Section::Code) {
            if (t.empty() || t[0] == '#') continue;
        }

        // Section header
        if (t == "[meta]")  { section = Section::Meta;  continue; }
        if (t == "[attrs]") { section = Section::Attrs; continue; }
        if (t == "[code]")  { section = Section::Code;  continue; }

        switch (section) {
            case Section::Meta: {
                auto [key, val] = splitFirst(t, '=');
                val = stripQuotes(trim(val));
                if      (key == "id")          doc.id          = val;
                else if (key == "name")        doc.name        = val;
                else if (key == "category")    doc.category    = val;
                else if (key == "version")     doc.version     = val;
                else if (key == "author")      doc.author      = val;
                else if (key == "description") doc.description = val;
                break;
            }
            case Section::Attrs: {
                // Format: name  type  default  "label"  "description"
                std::istringstream ls(t);
                AttrDef def;
                std::string typeStr;
                ls >> def.name >> typeStr >> def.defaultStr;
                def.type = parseAttrType(typeStr);

                // Remaining: "label" "description"
                std::string rest;
                std::getline(ls, rest);
                rest = trim(rest);
                // Extract quoted strings
                auto extractQuoted = [](std::string& s, std::string& out) {
                    size_t b = s.find('"');
                    if (b == std::string::npos) return false;
                    size_t e = s.find('"', b + 1);
                    if (e == std::string::npos) return false;
                    out = s.substr(b + 1, e - b - 1);
                    s   = trim(s.substr(e + 1));
                    return true;
                };
                extractQuoted(rest, def.label);
                extractQuoted(rest, def.description);

                if (!def.name.empty()) doc.attrs.push_back(def);
                break;
            }
            case Section::Code: {
                codeStream << line << '\n';
                break;
            }
            default: break;
        }
    }

    doc.code = codeStream.str();

    if (doc.id.empty()) {
        lastError_ = "Missing 'id' in [meta] section";
        return std::nullopt;
    }
    return doc;
}

std::optional<ToonDocument> ToonParser::loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        lastError_ = "Cannot open file: " + path;
        return std::nullopt;
    }
    std::string source((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    return parse(source);
}

// ============================================================
//  Serialize
// ============================================================

std::string ToonSerializer::serialize(const ToonDocument& doc) {
    std::ostringstream ss;

    ss << "[meta]\n";
    ss << "id          = \"" << doc.id          << "\"\n";
    ss << "name        = \"" << doc.name        << "\"\n";
    ss << "category    = \"" << doc.category    << "\"\n";
    ss << "version     = \"" << doc.version     << "\"\n";
    if (!doc.author.empty())
        ss << "author      = \"" << doc.author  << "\"\n";
    if (!doc.description.empty())
        ss << "description = \"" << doc.description << "\"\n";

    ss << "\n[attrs]\n";
    for (const auto& a : doc.attrs) {
        ss << a.name << "  " << attrTypeName(a.type) << "  "
           << a.defaultStr << "  \"" << a.label << "\"";
        if (!a.description.empty())
            ss << "  \"" << a.description << "\"";
        ss << "\n";
    }

    ss << "\n[code]\n";
    ss << doc.code;

    return ss.str();
}

bool ToonSerializer::saveFile(const ToonDocument& doc, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << serialize(doc);
    return f.good();
}

} // namespace toon
