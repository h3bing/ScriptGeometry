/**
 * @file chat.cpp
 * @brief AI Chat Assistant implementation
 *
 * Uses libcurl for HTTP requests.
 * Link with: -lcurl
 */

#include "chat.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

#include <curl/curl.h>

namespace chat {

// ============================================================
//  Timestamp helper
// ============================================================

static std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// ============================================================
//  PromptBuilder
// ============================================================

void PromptBuilder::setRole(const std::string& r)                { role_               = r; }
void PromptBuilder::setRequirements(const std::string& r)        { requirements_       = r; }
void PromptBuilder::setContext(const std::string& c)             { context_            = c; }
void PromptBuilder::setRules(const std::string& r)               { rules_              = r; }
void PromptBuilder::setAvailableFunctions(const std::string& f)  { availableFunctions_ = f; }
void PromptBuilder::addExample(const std::string& e)             { examples_.push_back(e); }

std::string PromptBuilder::build() const {
    std::ostringstream ss;

    auto section = [&](const char* tag, const std::string& content) {
        if (content.empty()) return;
        ss << "## [" << tag << "]\n" << content << "\n\n";
    };

    section("ROLE",               role_);
    section("REQUIREMENTS",       requirements_);
    section("CONTEXT",            context_);
    section("RULES",              rules_);
    section("AVAILABLE_FUNCTIONS",availableFunctions_);

    if (!examples_.empty()) {
        ss << "## [EXAMPLES]\n";
        for (size_t i = 0; i < examples_.size(); ++i) {
            ss << "### Example " << (i + 1) << "\n";
            ss << examples_[i] << "\n\n";
        }
    }
    return ss.str();
}

// ============================================================
//  Conversation
// ============================================================

void Conversation::addMessage(Role r, const std::string& c) {
    messages_.emplace_back(r, c, nowIso());
}
void Conversation::addSystemMessage(const std::string& c) { addMessage(Role::System, c); }
void Conversation::addTccMessage(const std::string& c)    { addMessage(Role::TCC,    c); }
void Conversation::addAiMessage(const std::string& c)     { addMessage(Role::AI,     c); }
void Conversation::addUserMessage(const std::string& c)   { addMessage(Role::User,   c); }

void Conversation::clear() { messages_.clear(); }

// JSON-escape a string
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string Conversation::toJson() const {
    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < messages_.size(); ++i) {
        const auto& m = messages_[i];
        // Map TCC messages to "user" role for API compatibility
        const char* apiRole = (m.role == Role::AI) ? "assistant" : "user";
        ss << "  {\"role\": \"" << apiRole << "\","
           << " \"content\": \"" << jsonEscape(roleLabel(m.role))
           << " " << jsonEscape(m.content) << "\"}";
        if (i + 1 < messages_.size()) ss << ",";
        ss << "\n";
    }
    ss << "]";
    return ss.str();
}

// ============================================================
//  ChatClient – libcurl integration
// ============================================================

static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = reinterpret_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

ChatClient::ChatClient(ModelConfig config)
    : config_(std::move(config))
{}

std::string ChatClient::buildRequestJson(const std::string& systemPrompt,
                                          const Conversation& conv) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"model\": \"" << jsonEscape(config_.name) << "\",\n";
    ss << "  \"temperature\": " << config_.temperature << ",\n";
    ss << "  \"max_tokens\": " << config_.maxTokens << ",\n";
    ss << "  \"messages\": [\n";
    ss << "    {\"role\": \"system\", \"content\": \"" << jsonEscape(systemPrompt) << "\"}";

    for (const auto& m : conv.messages()) {
        const char* apiRole = (m.role == Role::AI) ? "assistant" : "user";
        ss << ",\n    {\"role\": \"" << apiRole << "\","
           << " \"content\": \"" << jsonEscape(roleLabel(m.role))
           << " " << jsonEscape(m.content) << "\"}";
    }
    ss << "\n  ]\n}";
    return ss.str();
}

std::string ChatClient::httpPost(const std::string& url,
                                  const std::string& body,
                                  const std::string& authHeader) const {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;
    std::string errBuf(CURL_ERROR_SIZE, '\0');

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf.data());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)config_.timeoutSec);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        const_cast<ChatClient*>(this)->lastError_ = errBuf;
        return {};
    }
    return response;
}

std::string ChatClient::extractContent(const std::string& json) const {
    // Minimal JSON extraction: find "content":"..."
    // A proper implementation would use a JSON library.
    static const std::regex re(R"("content"\s*:\s*"((?:[^"\\]|\\.)*)"\s*[,\}])");
    std::smatch m;
    // Search for the last occurrence (AI reply)
    std::string lastContent;
    auto searchStart = json.cbegin();
    while (std::regex_search(searchStart, json.cend(), m, re)) {
        lastContent  = m[1].str();
        searchStart  = m.suffix().first;
    }
    // Un-escape
    std::string result;
    for (size_t i = 0; i < lastContent.size(); ++i) {
        if (lastContent[i] == '\\' && i + 1 < lastContent.size()) {
            switch (lastContent[i+1]) {
                case 'n':  result += '\n'; i++; break;
                case 't':  result += '\t'; i++; break;
                case '"':  result += '"';  i++; break;
                case '\\': result += '\\'; i++; break;
                default:   result += lastContent[i];
            }
        } else {
            result += lastContent[i];
        }
    }
    return result;
}

std::string ChatClient::sendSync(const std::string& systemPrompt,
                                  const Conversation& conv)
{
    std::string url     = config_.baseUrl + "/chat/completions";
    std::string body    = buildRequestJson(systemPrompt, conv);
    std::string authHdr = "Authorization: Bearer " + config_.apiKey;

    std::string resp = httpPost(url, body, authHdr);
    if (resp.empty()) return {};
    return extractContent(resp);
}

void ChatClient::sendAsync(const std::string& systemPrompt,
                            const Conversation& conv,
                            ResponseCallback onDone,
                            ErrorCallback    onError)
{
    std::thread([this, systemPrompt, conv, onDone, onError]() {
        std::string reply = sendSync(systemPrompt, conv);
        if (reply.empty() && !lastError_.empty()) {
            if (onError) onError(lastError_);
        } else {
            if (onDone) onDone(reply);
        }
    }).detach();
}

// ============================================================
//  ChatAssistant
// ============================================================

ChatAssistant::ChatAssistant(ModelConfig cfg)
    : config_(std::move(cfg))
    , client_(config_)
{
    prompt_.setRole(
        "You are ScriptGeometry AI, an expert in procedural 3D geometry generation. "
        "You generate C scripts that use the ScriptGeometry C API to produce geometry.");
    prompt_.setRules(buildDefaultRules());
    prompt_.setAvailableFunctions(buildDefaultFunctions());
}

void ChatAssistant::setModelConfig(const ModelConfig& cfg) {
    config_ = cfg;
    client_.setConfig(cfg);
}

void ChatAssistant::setSystemRole(const std::string& desc) { prompt_.setRole(desc); }
void ChatAssistant::setRules(const std::string& r)         { prompt_.setRules(r); }
void ChatAssistant::setAvailableFunctions(const std::string& f) { prompt_.setAvailableFunctions(f); }
void ChatAssistant::addExample(const std::string& e)       { prompt_.addExample(e); }

void ChatAssistant::startNewConversation() { conv_.clear(); }

void ChatAssistant::appendTccResult(const std::string& result) {
    conv_.addTccMessage(result);
}

std::string ChatAssistant::chat(const std::string& userMessage) {
    conv_.addUserMessage(userMessage);
    std::string sp    = prompt_.build();
    std::string reply = client_.sendSync(sp, conv_);
    if (!reply.empty())
        conv_.addAiMessage(reply);
    return reply;
}

void ChatAssistant::chatAsync(const std::string& userMessage,
                               std::function<void(const std::string&)> onReply,
                               std::function<void(const std::string&)> onError)
{
    conv_.addUserMessage(userMessage);
    std::string sp = prompt_.build();
    Conversation convCopy = conv_;   // snapshot for thread

    client_.sendAsync(sp, convCopy,
        [this, onReply](const std::string& reply) {
            conv_.addAiMessage(reply);
            if (onReply) onReply(reply);
        },
        onError);
}

std::string ChatAssistant::buildDefaultRules() {
    return
        "1. Always respond with a valid TOON block enclosed in ```toon ... ```.\n"
        "2. The TOON block must contain [meta], [attrs], and [code] sections.\n"
        "3. In [code], define exactly one function: void build(GeoDataHandle h) {...}\n"
        "4. Do NOT use #include, malloc, free, or any system/network calls.\n"
        "5. Only use the functions listed in [AVAILABLE_FUNCTIONS].\n"
        "6. Attribute values are passed as individual parameters after GeoDataHandle.\n"
        "7. Keep code concise and well-commented.\n"
        "8. If the user asks a non-geometry question, respond politely but stay on topic.\n";
}

std::string ChatAssistant::buildDefaultFunctions() {
    return
        "Math: math_sin, math_cos, math_tan, math_sqrt, math_pow, math_abs,\n"
        "      math_floor, math_ceil, math_round, math_clamp, math_lerp,\n"
        "      math_min2, math_max2, math_deg2rad, math_rad2deg\n"
        "Curve: curve_line, curve_polyline, curve_arc, curve_circle,\n"
        "       curve_bezier, curve_catmull\n"
        "Loop:  loop_rect, loop_polygon, loop_circle, loop_ellipse, loop_roundrect,\n"
        "       loop_union, loop_intersect, loop_difference, loop_xor,\n"
        "       loop_offset, loop_fill, loop_fill_with_holes, loop_simplify\n"
        "Path:  path_line, path_arc, path_spline\n"
        "Surface: surface_plane, surface_disk, surface_fill\n"
        "Solid: solid_box, solid_sphere, solid_cylinder, solid_cone,\n"
        "       solid_torus, solid_capsule\n"
        "Constants: CAPI_PI, CAPI_E\n";
}

// ============================================================
//  extractToonBlock
// ============================================================

std::optional<std::string> extractToonBlock(const std::string& aiReply) {
    static const std::string BEGIN = "```toon";
    static const std::string END   = "```";

    size_t b = aiReply.find(BEGIN);
    if (b == std::string::npos) return std::nullopt;

    size_t codeStart = aiReply.find('\n', b);
    if (codeStart == std::string::npos) return std::nullopt;
    codeStart++;

    size_t e = aiReply.find(END, codeStart);
    if (e == std::string::npos) return std::nullopt;

    return aiReply.substr(codeStart, e - codeStart);
}

} // namespace chat
