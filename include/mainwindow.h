/**
 * @file mainwindow.h
 * @brief Main application window – ImGui layout manager
 *
 * Layout:
 *   ┌─────────────────────────────────────────┐
 *   │  [Document panel]   [3D View]  [AI panel] │
 *   │  (left collapsible) (center)  (right)     │
 *   │                    [Toolbar]              │
 *   └─────────────────────────────────────────┘
 *
 * Document panel (left, collapsible):
 *   - Entity list
 *   - Selected entity: attributes + script assignment
 *
 * AI panel (right, collapsible):
 *   - Conversation list
 *   - User message editor
 *   - Model selector, New conversation, Send buttons
 *
 * Toolbar (floating, center-bottom):
 *   - Show axes toggle
 *   - Projection mode (Perspective / Orthographic)
 *   - Render mode (Solid / Wireframe / SolidWire / Points)
 *   - Standard views (Front / Top / Right / Iso)
 */

#pragma once

#include "geolib.h"
#include "scriptlib.h"
#include "tccengine.h"
#include "chat.h"
#include "glview.h"

#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <string>

namespace app {

struct AppState {
    geo::Document           doc;
    script::ScriptLib       scriptLib;
    tcc_engine::TccEngine   tccEngine;
    chat::ChatAssistant     chatAssistant;
    glview::GlView          glView;

    uint64_t  selectedEntityId {0};
    bool      docPanelOpen     {true};
    bool      aiPanelOpen      {true};
    bool      settingsOpen     {false};

    // Pending async AI reply
    bool        aiPending       {false};
    std::string aiPendingText;
    std::string aiError;
};

class MainWindow {
public:
    explicit MainWindow(GLFWwindow* window);
    ~MainWindow();

    bool initialize();
    void shutdown();

    /// Called every frame
    void update();
    void render();

    AppState& state() { return state_; }

private:
    GLFWwindow* window_;
    AppState    state_;

    // Panel dimensions
    float docPanelWidth_   {260.f};
    float aiPanelWidth_    {320.f};

    // UI helpers
    void drawDocumentPanel(float x, float y, float w, float h);
    void drawEntityList();
    void drawEntityProperties(geo::Entity* entity);

    void drawAiPanel(float x, float y, float w, float h);
    void drawConversation();
    void drawAiControls();

    void drawToolbar(float centerX, float bottomY);
    void drawMainViewport(float x, float y, float w, float h);
    void drawSettingsWindow();

    // Actions
    void onEntitySelected(uint64_t id);
    void onAttrChanged(geo::Entity& entity, const std::string& key);
    void rebuildEntityGeometry(geo::Entity& entity);

    void onSendAiMessage(const std::string& msg);
    void onNewConversation();

    void createExampleEntity();

    // Input forwarding (mouse captured by viewport area)
    bool viewportHovered_ {false};
    float viewportX_      {0.f};
    float viewportY_      {0.f};
    float viewportW_      {0.f};
    float viewportH_      {0.f};

    // Framebuffer for viewport rendering
    GLuint fbo_           {0};
    GLuint fbColorTex_    {0};
    GLuint fbDepthRbo_    {0};
    int    fbWidth_       {0};
    int    fbHeight_      {0};
    void   resizeFbo(int w, int h);

    // Model config UI state
    char   modelNameBuf_[128]  {};
    char   modelUrlBuf_[512]   {};
    char   modelKeyBuf_[512]   {};
    char   userMsgBuf_[2048]   {};
};

} // namespace app
