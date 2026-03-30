/**
 * @file mainwindow.h
 * @brief 主应用窗口 - ImGui布局管理器
 *
 * 布局:
 *   ┌─────────────────────────────────────────────────────┐
 *   │ [文档面板]     [3D视图]        [AI面板]              │
 *   │ (左可折叠)     (中心)          (右可折叠)            │
 *   │                [工具栏]                             │
 *   └─────────────────────────────────────────────────────┘
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

// ============================================================
//  应用状态
// ============================================================

struct AppState {
    geo::Document           doc;          // 场景文档
    script::ScriptLib       scriptLib;    // 脚本库（注册制）
    tcc_engine::TccEngine   tccEngine;    // TCC引擎（脚本缓存）
    chat::ChatAssistant     chatAssistant;// AI聊天助手
    glview::GlView          glView;       // OpenGL视图

    uint64_t  selectedEntityId{0};        // 当前选中的实体
    bool      docPanelOpen{true};         // 文档面板开关
    bool      aiPanelOpen{true};          // AI面板开关
    bool      settingsOpen{false};        // 设置窗口开关

    // AI请求状态
    bool        aiPending{false};
    std::string aiPendingText;
    std::string aiError;

    // 文档状态
    bool        showExportDialog{false};  // 导出对话框
    bool        showImportDialog{false};  // 导入对话框
    char        exportPathBuf_[512]{};    // 导出路径
    int         exportFormat{0};          // 0=ASCII, 1=Binary
};

// ============================================================
//  MainWindow
// ============================================================

class MainWindow {
public:
    explicit MainWindow(GLFWwindow* window);
    ~MainWindow();

    bool initialize();
    void shutdown();

    void update();
    void render();

    AppState& state() { return state_; }

private:
    GLFWwindow* window_;
    AppState    state_;

    // 面板尺寸
    float docPanelWidth_{260.f};
    float aiPanelWidth_{320.f};

    // --------------------------------------------------------
    //  UI绘制
    // --------------------------------------------------------

    void drawDocumentPanel(float x, float y, float w, float h);
    void drawEntityList();
    void drawEntityProperties(geo::Entity* entity);
    void drawScriptSelector(geo::Entity* entity);

    void drawAiPanel(float x, float y, float w, float h);
    void drawConversation();
    void drawAiControls();

    void drawToolbar(float centerX, float bottomY);
    void drawMainViewport(float x, float y, float w, float h);
    void drawSettingsWindow();

    // --------------------------------------------------------
    //  核心逻辑
    // --------------------------------------------------------

    /// 选中实体
    void onEntitySelected(uint64_t id);

    /// 属性变更
    void onAttrChanged(geo::Entity& entity, const std::string& key);

    /// 变换变更
    void onTransformChanged(geo::Entity& entity);

    /// 重建实体几何（核心：脚本编译一次，多实例执行）
    void rebuildEntityGeometry(geo::Entity& entity);

    /// AI消息发送
    void onSendAiMessage(const std::string& msg);

    /// 新对话
    void onNewConversation();

    /// 处理AI回复中的TOON脚本
    void handleAiToonResponse(const std::string& toonSource);

    /// 创建示例实体
    void createExampleEntity();

    // --------------------------------------------------------
    //  文档管理
    // --------------------------------------------------------

    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
    void onExportStl();
    void onImportStl();
    void drawExportDialog();
    void drawImportDialog();

    // --------------------------------------------------------
    //  帧缓冲
    // --------------------------------------------------------

    GLuint fbo_{0};
    GLuint fbColorTex_{0};
    GLuint fbDepthRbo_{0};
    int    fbWidth_{0};
    int    fbHeight_{0};
    void   resizeFbo(int w, int h);

    bool   viewportHovered_{false};
    float  viewportX_{0.f}, viewportY_{0.f}, viewportW_{0.f}, viewportH_{0.f};

    // --------------------------------------------------------
    //  UI状态缓冲
    // --------------------------------------------------------

    char   modelNameBuf_[128]{};
    char   modelUrlBuf_[512]{};
    char   modelKeyBuf_[512]{};
    char   userMsgBuf_[2048]{};
};

} // namespace app
