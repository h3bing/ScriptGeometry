/**
 * @file chat.h
 * @brief AI Chat Assistant – four-party dialogue system
 *
 * Supports four roles:
 *   System  – static context injected at conversation start
 *   TCC     – messages from the TCC engine (errors, results)
 *   AI      – responses from the large-language model
 *   User    – messages typed by the human operator
 *
 * The assistant builds a structured prompt from:
 *   [Role][Requirements][Context][Rules][AvailableFunctions][Examples]
 * and sends it to a configurable LLM API endpoint.
 *
 * AI responses are expected in TOON format so they can be fed directly
 * into the ScriptLib / TccEngine pipeline.
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chat {

// ============================================================
//  LLM Model Configuration
// ============================================================

struct ModelConfig {
    std::string name       {"gpt-4o"};
    std::string baseUrl    {"https://api.openai.com/v1"};
    std::string apiKey;
    float       temperature{0.3f};
    int         maxTokens  {4096};
    int         timeoutSec {60};
};

// ============================================================
//  Message roles
// ============================================================

enum class Role { System, TCC, AI, User };

inline const char* roleName(Role r) {
    switch (r) {
        case Role::System: return "system";
        case Role::TCC:    return "tcc";
        case Role::AI:     return "assistant";
        case Role::User:   return "user";
    }
    return "user";
}

inline const char* roleLabel(Role r) {
    switch (r) {
        case Role::System: return "[SYSTEM]";
        case Role::TCC:    return "[TCC]";
        case Role::AI:     return "[AI]";
        case Role::User:   return "[USER]";
    }
    return "[USER]";
}

// ============================================================
//  ChatMessage
// ============================================================

struct ChatMessage {
    Role        role;
    std::string content;
    std::string timestamp;   ///< ISO-8601 or empty

    ChatMessage(Role r, std::string c, std::string ts = {})
        : role(r), content(std::move(c)), timestamp(std::move(ts)) {}
};

// ============================================================
//  PromptSection  – building block for the structured prompt
// ============================================================

struct PromptSection {
    std::string tag;      ///< e.g. "role", "requirements", "rules"
    std::string content;
};

// ============================================================
//  PromptBuilder
// ============================================================

class PromptBuilder {
public:
    void setRole(const std::string& roleDesc);
    void setRequirements(const std::string& req);
    void setContext(const std::string& ctx);
    void setRules(const std::string& rules);
    void setAvailableFunctions(const std::string& funcs);
    void addExample(const std::string& example);

    /// Build the complete system prompt string
    std::string build() const;

private:
    std::string                role_;
    std::string                requirements_;
    std::string                context_;
    std::string                rules_;
    std::string                availableFunctions_;
    std::vector<std::string>   examples_;
};

// ============================================================
//  Conversation
// ============================================================

class Conversation {
public:
    void addMessage(Role role, const std::string& content);
    void addSystemMessage(const std::string& content);
    void addTccMessage(const std::string& content);
    void addAiMessage(const std::string& content);
    void addUserMessage(const std::string& content);
    void clear();

    const std::vector<ChatMessage>& messages() const { return messages_; }
    size_t size() const { return messages_.size(); }

    /// Serialize to JSON array suitable for LLM API
    std::string toJson() const;

private:
    std::vector<ChatMessage> messages_;
};

// ============================================================
//  ChatClient  – sends requests to the LLM
// ============================================================

class ChatClient {
public:
    using ResponseCallback  = std::function<void(const std::string& reply)>;
    using ErrorCallback     = std::function<void(const std::string& error)>;
    using StreamCallback    = std::function<void(const std::string& delta, bool done)>;

    explicit ChatClient(ModelConfig config = {});

    void setConfig(const ModelConfig& cfg) { config_ = cfg; }
    const ModelConfig& config() const      { return config_; }

    /**
     * Send a synchronous request.
     * @param systemPrompt  Built by PromptBuilder::build()
     * @param conv          Conversation history
     * @return              AI reply text, or empty on error
     */
    std::string sendSync(const std::string& systemPrompt,
                         const Conversation& conv);

    /**
     * Send an asynchronous request (runs on a background thread).
     */
    void sendAsync(const std::string& systemPrompt,
                   const Conversation& conv,
                   ResponseCallback onDone,
                   ErrorCallback    onError);

    const std::string& lastError() const { return lastError_; }

private:
    ModelConfig  config_;
    std::string  lastError_;

    std::string buildRequestJson(const std::string& systemPrompt,
                                  const Conversation& conv) const;
    std::string httpPost(const std::string& url,
                         const std::string& body,
                         const std::string& authHeader) const;
    std::string extractContent(const std::string& jsonResponse) const;
};

// ============================================================
//  ChatAssistant  – high-level facade
// ============================================================

class ChatAssistant {
public:
    explicit ChatAssistant(ModelConfig cfg = {});

    // Configuration
    void setModelConfig(const ModelConfig& cfg);
    const ModelConfig& modelConfig() const { return client_.config(); }

    // Prompt configuration
    void setSystemRole(const std::string& desc);
    void setRules(const std::string& rules);
    void setAvailableFunctions(const std::string& funcs);
    void addExample(const std::string& ex);

    // Conversation management
    void startNewConversation();
    void appendTccResult(const std::string& result);

    const Conversation& conversation() const { return conv_; }

    /**
     * Send a user message and get an AI reply (synchronous).
     * The reply is expected to contain a TOON block.
     * The method appends both the user message and AI reply to the conversation.
     */
    std::string chat(const std::string& userMessage);

    /**
     * Async variant – calls onReply when the AI responds.
     */
    void chatAsync(const std::string& userMessage,
                   std::function<void(const std::string&)> onReply,
                   std::function<void(const std::string&)> onError = {});

    const std::string& lastError() const { return client_.lastError(); }

private:
    ModelConfig    config_;
    ChatClient     client_;
    PromptBuilder  prompt_;
    Conversation   conv_;

    static std::string buildDefaultSystemPrompt();
    static std::string buildDefaultRules();
    static std::string buildDefaultFunctions();
};

// ============================================================
//  Helpers: extract TOON block from AI reply
// ============================================================

/**
 * Extract the first ```toon ... ``` block from an AI message.
 * Returns nullopt if no block found.
 */
std::optional<std::string> extractToonBlock(const std::string& aiReply);

} // namespace chat
