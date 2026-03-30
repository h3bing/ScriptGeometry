/**
 * @file mainwindow.cpp
 * @brief 主应用窗口实现
 */

#include "mainwindow.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>

#include <cstring>
#include <iostream>
#include <string>
#include <variant>

namespace app {

// ============================================================
//  辅助函数
// ============================================================

static std::string attrToString(const geo::AttrValue& v) {
    return std::visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>)       return std::to_string(val);
        if constexpr (std::is_same_v<T, int>)         return std::to_string(val);
        if constexpr (std::is_same_v<T, bool>)        return val ? "true" : "false";
        if constexpr (std::is_same_v<T, std::string>) return val;
        if constexpr (std::is_same_v<T, geo::Color>)
            return std::to_string(val.r)+","+std::to_string(val.g)+","+std::to_string(val.b);
        return "?";
    }, v);
}

// ============================================================
//  MainWindow
// ============================================================

MainWindow::MainWindow(GLFWwindow* window) : window_(window) {
    std::memset(modelNameBuf_, 0, sizeof(modelNameBuf_));
    std::memset(modelUrlBuf_,  0, sizeof(modelUrlBuf_));
    std::memset(modelKeyBuf_,  0, sizeof(modelKeyBuf_));
    std::memset(userMsgBuf_,   0, sizeof(userMsgBuf_));

    const auto& cfg = state_.chatAssistant.modelConfig();
    std::strncpy(modelNameBuf_, cfg.name.c_str(), sizeof(modelNameBuf_)-1);
    std::strncpy(modelUrlBuf_,  cfg.baseUrl.c_str(), sizeof(modelUrlBuf_)-1);
}

MainWindow::~MainWindow() { shutdown(); }

bool MainWindow::initialize() {
    if (!state_.glView.initialize()) {
        std::cerr << "[MainWindow] GlView初始化失败\n";
        return false;
    }

    // 加载脚本目录
    state_.scriptLib.loadDirectory("scripts");

    // 创建示例实体
    createExampleEntity();

    return true;
}

void MainWindow::shutdown() {
    resizeFbo(0, 0);
    state_.glView.shutdown();
}

void MainWindow::resizeFbo(int w, int h) {
    if (fbWidth_ == w && fbHeight_ == h) return;
    if (fbo_) {
        glDeleteFramebuffers(1,  &fbo_);
        glDeleteTextures(1,      &fbColorTex_);
        glDeleteRenderbuffers(1, &fbDepthRbo_);
        fbo_ = fbColorTex_ = fbDepthRbo_ = 0;
    }
    if (w <= 0 || h <= 0) { fbWidth_ = fbHeight_ = 0; return; }

    glGenFramebuffers(1,  &fbo_);
    glGenTextures(1,      &fbColorTex_);
    glGenRenderbuffers(1, &fbDepthRbo_);

    glBindTexture(GL_TEXTURE_2D, fbColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindRenderbuffer(GL_RENDERBUFFER, fbDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbColorTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbDepthRbo_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[MainWindow] FBO不完整\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbWidth_ = w; fbHeight_ = h;
}

// ============================================================
//  更新循环
// ============================================================

void MainWindow::update() {
    // 重建脏实体
    for (auto& ep : state_.doc.entities()) {
        if (ep->isDirty()) {
            rebuildEntityGeometry(*ep);
            ep->clearDirty();
        }
    }

    // 处理AI回复
    if (state_.aiPending && !state_.aiPendingText.empty()) {
        state_.aiPending = false;

        // 提取TOON块
        auto toonBlock = chat::extractToonBlock(state_.aiPendingText);
        if (toonBlock) {
            handleAiToonResponse(*toonBlock);
        }
        state_.aiPendingText.clear();
    }
}

// ============================================================
//  核心逻辑：重建实体几何
// ============================================================

void MainWindow::rebuildEntityGeometry(geo::Entity& entity) {
    const std::string& scriptId = entity.scriptId();
    if (scriptId.empty()) return;

    // 获取脚本元数据
    const auto* meta = state_.scriptLib.findScript(scriptId);
    if (!meta) {
        std::cerr << "[MainWindow] 脚本未找到: " << scriptId << "\n";
        return;
    }

    // 收集属性名
    std::vector<std::string> attrNames;
    for (const auto& def : meta->attrDefs) {
        attrNames.push_back(def.name);
    }

    // 编译脚本（如果未编译，或源码变化）
    const auto* script = state_.tccEngine.compile(scriptId, meta->code, attrNames);
    if (!script) {
        std::cerr << "[MainWindow] 编译失败: " << state_.tccEngine.lastError() << "\n";
        return;
    }

    // 执行脚本（Entity指针作为参数，脚本从中读取属性）
    state_.tccEngine.execute(script, &entity, entity.geoData());

    // 上传GPU
    state_.glView.uploadEntity(entity.id(), entity.geoData());
}

// ============================================================
//  AI TOON响应处理
// ============================================================

void MainWindow::handleAiToonResponse(const std::string& toonSource) {
    // 注册到脚本库
    std::string scriptId = state_.scriptLib.registerFromSource(toonSource, "ai_generated");
    if (scriptId.empty()) {
        state_.chatAssistant.appendTccResult("AI脚本注册失败");
        return;
    }

    // 获取脚本元数据
    const auto* meta = state_.scriptLib.findScript(scriptId);
    if (!meta) return;

    // 编译脚本
    std::vector<std::string> attrNames;
    for (const auto& def : meta->attrDefs) attrNames.push_back(def.name);
    state_.tccEngine.compile(scriptId, meta->code, attrNames);

    // 创建实体
    geo::Entity& entity = state_.doc.createEntity(meta->name.empty() ? scriptId : meta->name);
    entity.setScriptId(scriptId);

    // 应用默认属性
    for (const auto& [k, v] : meta->defaultAttrs()) {
        entity.setAttr(k, v);
    }

    entity.markDirty();
    state_.selectedEntityId = entity.id();
    state_.glView.setSelectedEntity(entity.id());

    state_.chatAssistant.appendTccResult("脚本已注册: " + scriptId);
}

// ============================================================
//  渲染
// ============================================================

void MainWindow::render() {
    int winW, winH;
    glfwGetFramebufferSize(window_, &winW, &winH);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    float leftW   = state_.docPanelOpen ? docPanelWidth_ : 0.f;
    float rightW  = state_.aiPanelOpen  ? aiPanelWidth_  : 0.f;
    float centerW = std::max(200.f, (float)winW - leftW - rightW);
    float h       = (float)winH;

    if (state_.docPanelOpen) drawDocumentPanel(0.f, 0.f, leftW, h);
    drawMainViewport(leftW, 0.f, centerW, h);
    if (state_.aiPanelOpen) drawAiPanel(leftW + centerW, 0.f, rightW, h);

    float toolbarW = 480.f, toolbarH = 36.f;
    drawToolbar(leftW + centerW * 0.5f, h - toolbarH - 8.f);

    ImGui::End();

    if (state_.settingsOpen) drawSettingsWindow();
}

// ============================================================
//  文档面板
// ============================================================

void MainWindow::drawDocumentPanel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("Document##panel", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    if (ImGui::Button("<<")) state_.docPanelOpen = false;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f,0.9f,1.f,1.f), "Document");

    // 文档管理按钮
    ImGui::Separator();
    {
        // 新建按钮
        if (ImGui::Button("New")) onNewDocument();
        ImGui::SameLine();

        // 打开按钮
        if (ImGui::Button("Open")) onOpenDocument();
        ImGui::SameLine();

        // 保存按钮
        if (ImGui::Button("Save")) onSaveDocument();
        ImGui::SameLine();

        // 另存为按钮
        if (ImGui::Button("SaveAs")) onSaveDocumentAs();
    }

    // 导入导出按钮
    {
        if (ImGui::Button("Import STL")) state_.showImportDialog = true;
        ImGui::SameLine();
        if (ImGui::Button("Export STL")) state_.showExportDialog = true;
    }

    // 显示当前文件路径
    if (!state_.doc.filePath().empty()) {
        ImGui::TextDisabled("%s", state_.doc.filePath().c_str());
    }
    if (state_.doc.isModified()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,0.5f,0,1), "*");
    }

    ImGui::Separator();
    drawEntityList();

    ImGui::Separator();
    auto* ent = state_.doc.findEntity(state_.selectedEntityId);
    if (ent) {
        drawScriptSelector(ent);
        drawEntityProperties(ent);
    }

    ImGui::End();

    // 绘制对话框
    if (state_.showExportDialog) drawExportDialog();
    if (state_.showImportDialog) drawImportDialog();
}

void MainWindow::drawEntityList() {
    ImGui::Text("实体 (%zu)", state_.doc.size());
    if (ImGui::Button("+ New")) {
        auto& e = state_.doc.createEntity("Entity_" + std::to_string(state_.doc.size()));
        state_.selectedEntityId = e.id();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && state_.selectedEntityId) {
        state_.glView.removeEntity(state_.selectedEntityId);
        state_.doc.removeEntity(state_.selectedEntityId);
        state_.selectedEntityId = 0;
    }

    ImGui::BeginChild("##entityList", ImVec2(0, 120), true);
    for (const auto& ep : state_.doc.entities()) {
        bool selected = (ep->id() == state_.selectedEntityId);
        std::string label = ep->name() + "##" + std::to_string(ep->id());
        if (ImGui::Selectable(label.c_str(), selected))
            onEntitySelected(ep->id());
    }
    ImGui::EndChild();
}

void MainWindow::drawScriptSelector(geo::Entity* entity) {
    const char* currentScript = entity->scriptId().empty() ? "(none)" : entity->scriptId().c_str();
    ImGui::Text("脚本:");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##script", currentScript)) {
        if (ImGui::Selectable("(none)", entity->scriptId().empty()))
            entity->setScriptId("");

        for (const auto& sm : state_.scriptLib.scripts()) {
            bool sel = (sm.id == entity->scriptId());
            std::string label = sm.name + " [" + sm.id + "]";
            if (ImGui::Selectable(label.c_str(), sel)) {
                entity->setScriptId(sm.id);
                // 应用默认属性
                auto* meta = state_.scriptLib.findScript(sm.id);
                if (meta) {
                    for (const auto& [k, v] : meta->defaultAttrs())
                        entity->setAttr(k, v);
                }
            }
        }
        ImGui::EndCombo();
    }
}

void MainWindow::drawEntityProperties(geo::Entity* entity) {
    ImGui::TextColored(ImVec4(1,0.8f,0.4f,1), "属性: %s", entity->name().c_str());

    // --------------------------------------------------------
    //  位姿调整（位置、旋转、缩放）
    // --------------------------------------------------------
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 位置
        geo::Point3 pos = entity->position();
        float p[3] = {pos.x, pos.y, pos.z};
        if (ImGui::DragFloat3("Position", p, 0.01f)) {
            entity->setPosition(p[0], p[1], p[2]);
            onTransformChanged(*entity);
        }

        // 旋转（度数显示，内部转换）
        geo::Vector3 rot = entity->rotation();
        float r[3] = {
            rot.x * 180.f / geo::PI,
            rot.y * 180.f / geo::PI,
            rot.z * 180.f / geo::PI
        };
        if (ImGui::DragFloat3("Rotation (deg)", r, 0.5f)) {
            entity->setRotation(geo::Vector3(
                r[0] * geo::PI / 180.f,
                r[1] * geo::PI / 180.f,
                r[2] * geo::PI / 180.f
            ));
            onTransformChanged(*entity);
        }

        // 缩放
        geo::Vector3 scl = entity->scale();
        float s[3] = {scl.x, scl.y, scl.z};
        if (ImGui::DragFloat3("Scale", s, 0.01f, 0.001f, 100.f)) {
            entity->setScale(geo::Vector3(s[0], s[1], s[2]));
            onTransformChanged(*entity);
        }

        // 快捷按钮
        if (ImGui::Button("Reset Transform")) {
            entity->resetTransform();
            onTransformChanged(*entity);
        }
        ImGui::SameLine();
        if (ImGui::Button("Scale 1.0")) {
            entity->setScale(geo::Vector3(1.f));
            onTransformChanged(*entity);
        }
    }

    ImGui::Separator();

    // --------------------------------------------------------
    //  属性列表
    // --------------------------------------------------------
    if (ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("##attrs", ImVec2(0, 0), false);

        for (auto& [key, val] : entity->attrs()) {
            ImGui::PushID(key.c_str());
            ImGui::Text("%s", key.c_str());
            ImGui::SameLine(100.f);

            std::visit([&, this](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, float>) {
                    if (ImGui::DragFloat("##v", &v, 0.01f))
                        onAttrChanged(*entity, key);
                } else if constexpr (std::is_same_v<T, int>) {
                    if (ImGui::DragInt("##v", &v, 1))
                        onAttrChanged(*entity, key);
                } else if constexpr (std::is_same_v<T, bool>) {
                    if (ImGui::Checkbox("##v", &v))
                        onAttrChanged(*entity, key);
                } else if constexpr (std::is_same_v<T, geo::Color>) {
                    float col[4] = {v.r, v.g, v.b, v.a};
                    if (ImGui::ColorEdit4("##v", col)) {
                        v = geo::Color{col[0], col[1], col[2], col[3]};
                        onAttrChanged(*entity, key);
                    }
                } else {
                    ImGui::TextUnformatted(attrToString(val).c_str());
                }
            }, val);

            ImGui::PopID();
        }
        ImGui::EndChild();
    }
}

// ============================================================
//  AI面板
// ============================================================

void MainWindow::drawAiPanel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("AI##panel", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    if (ImGui::Button(">>")) state_.aiPanelOpen = false;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "AI Assistant");
    ImGui::SameLine();
    if (ImGui::SmallButton("Settings")) state_.settingsOpen = true;

    ImGui::Separator();
    drawConversation();
    ImGui::Separator();
    drawAiControls();

    ImGui::End();
}

void MainWindow::drawConversation() {
    float convH = ImGui::GetContentRegionAvail().y - 150.f;
    if (convH < 80.f) convH = 80.f;

    ImGui::BeginChild("##conv", ImVec2(0, convH), true);
    for (const auto& msg : state_.chatAssistant.conversation().messages()) {
        ImVec4 col;
        switch (msg.role) {
            case chat::Role::System: col = {0.5f,0.5f,0.5f,1}; break;
            case chat::Role::TCC:    col = {1.f,0.7f,0.2f,1};  break;
            case chat::Role::AI:     col = {0.4f,0.9f,0.6f,1}; break;
            case chat::Role::User:   col = {0.9f,0.9f,1.f,1};  break;
        }
        ImGui::TextColored(col, "%s", chat::roleLabel(msg.role));
        ImGui::SameLine();
        ImGui::TextWrapped("%s", msg.content.c_str());
        ImGui::Spacing();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();

    if (state_.aiPending)
        ImGui::TextColored(ImVec4(0.5f,1,0.5f,1), "AI正在思考...");
    if (!state_.aiError.empty())
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "错误: %s", state_.aiError.c_str());
}

void MainWindow::drawAiControls() {
    ImGui::Text("Model:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-80.f);
    ImGui::InputText("##model", modelNameBuf_, sizeof(modelNameBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        chat::ModelConfig cfg = state_.chatAssistant.modelConfig();
        cfg.name = modelNameBuf_;
        cfg.baseUrl = modelUrlBuf_;
        cfg.apiKey = modelKeyBuf_;
        state_.chatAssistant.setModelConfig(cfg);
    }

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextMultiline("##userMsg", userMsgBuf_, sizeof(userMsgBuf_), ImVec2(0, 60));

    if (ImGui::Button("New")) onNewConversation();
    ImGui::SameLine();
    bool sendDisabled = state_.aiPending || (userMsgBuf_[0] == '\0');
    if (sendDisabled) ImGui::BeginDisabled();
    if (ImGui::Button("Send")) {
        onSendAiMessage(userMsgBuf_);
        std::memset(userMsgBuf_, 0, sizeof(userMsgBuf_));
    }
    if (sendDisabled) ImGui::EndDisabled();
}

// ============================================================
//  工具栏
// ============================================================

void MainWindow::drawToolbar(float centerX, float bottomY) {
    float toolW = 560.f, toolH = 36.f;
    ImGui::SetNextWindowPos(ImVec2(centerX - toolW * 0.5f, bottomY));
    ImGui::SetNextWindowSize(ImVec2(toolW, toolH));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    auto& gv = state_.glView;

    ImGui::Checkbox("Axes", &gv.showAxes); ImGui::SameLine();
    ImGui::Checkbox("Grid", &gv.showGrid); ImGui::SameLine();

    bool isPerspective = (gv.camera().projection == glview::ProjectionMode::Perspective);
    if (ImGui::Button(isPerspective ? "Persp" : "Ortho")) {
        gv.camera().projection = isPerspective
            ? glview::ProjectionMode::Orthographic
            : glview::ProjectionMode::Perspective;
    }
    ImGui::SameLine();

    const char* rmLabels[] = {"Solid","Wire","SW","Points"};
    int rm = (int)gv.renderMode;
    ImGui::SetNextItemWidth(80.f);
    if (ImGui::Combo("##rm", &rm, rmLabels, 4))
        gv.renderMode = (glview::RenderMode)rm;
    ImGui::SameLine();

    for (const char* v : {"Front","Top","Right","Iso"}) {
        if (ImGui::SmallButton(v)) {
            std::string lower(v);
            for (char& c : lower) c = (char)std::tolower(c);
            gv.camera().setStandardView(lower);
        }
        ImGui::SameLine();
    }

    if (!state_.docPanelOpen && ImGui::SmallButton("Doc")) state_.docPanelOpen = true;
    ImGui::SameLine();
    if (!state_.aiPanelOpen && ImGui::SmallButton("AI"))  state_.aiPanelOpen = true;

    ImGui::End();
}

// ============================================================
//  主视口
// ============================================================

void MainWindow::drawMainViewport(float x, float y, float w, float h) {
    int iw = std::max(1, (int)w);
    int ih = std::max(1, (int)h - 44);

    resizeFbo(iw, ih);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    state_.glView.render(iw, ih);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##viewport", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    ImGui::Image((ImTextureID)(intptr_t)fbColorTex_, ImVec2((float)iw, (float)ih), ImVec2(0,1), ImVec2(1,0));

    viewportHovered_ = ImGui::IsItemHovered();
    viewportX_ = x; viewportY_ = y; viewportW_ = (float)iw; viewportH_ = (float)ih;

    ImGui::End();
}

// ============================================================
//  设置窗口
// ============================================================

void MainWindow::drawSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(450, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &state_.settingsOpen)) {
        ImGui::Text("LLM配置");
        ImGui::Separator();
        ImGui::InputText("Model Name", modelNameBuf_, sizeof(modelNameBuf_));
        ImGui::InputText("Base URL",   modelUrlBuf_,  sizeof(modelUrlBuf_));
        ImGui::InputText("API Key",    modelKeyBuf_,  sizeof(modelKeyBuf_), ImGuiInputTextFlags_Password);
        if (ImGui::Button("Save")) {
            chat::ModelConfig cfg = state_.chatAssistant.modelConfig();
            cfg.name = modelNameBuf_;
            cfg.baseUrl = modelUrlBuf_;
            cfg.apiKey = modelKeyBuf_;
            state_.chatAssistant.setModelConfig(cfg);
            state_.settingsOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) state_.settingsOpen = false;
    }
    ImGui::End();
}

// ============================================================
//  事件处理
// ============================================================

void MainWindow::onEntitySelected(uint64_t id) {
    state_.selectedEntityId = id;
    state_.glView.setSelectedEntity(id);
}

void MainWindow::onAttrChanged(geo::Entity& entity, const std::string&) {
    entity.markDirty();
}

void MainWindow::onTransformChanged(geo::Entity& entity) {
    // 变换变化后需要更新GPU缓冲区
    // 对于纯变换，不需要重新生成几何，只需要更新渲染时的模型矩阵
    // 但这里我们标记dirty以确保一致性
    state_.doc.markModified();
}

void MainWindow::onSendAiMessage(const std::string& msg) {
    state_.aiPending = true;
    state_.aiError.clear();

    state_.chatAssistant.chatAsync(msg,
        [this](const std::string& reply) {
            state_.aiPendingText = reply;
            state_.aiPending = false;
        },
        [this](const std::string& err) {
            state_.aiError = err;
            state_.aiPending = false;
        }
    );
}

void MainWindow::onNewConversation() {
    state_.chatAssistant.startNewConversation();
    state_.aiError.clear();
    state_.aiPending = false;
}

void MainWindow::createExampleEntity() {
    // 查找 sphere 脚本
    const auto* meta = state_.scriptLib.findScript("sphere_solid");

    auto& e = state_.doc.createEntity("Demo_Sphere");

    if (meta) {
        e.setScriptId("sphere_solid");
        for (const auto& [k, v] : meta->defaultAttrs()) {
            e.setAttr(k, v);
        }
        // 立即编译脚本
        std::vector<std::string> attrNames;
        for (const auto& def : meta->attrDefs) attrNames.push_back(def.name);
        state_.tccEngine.compile("sphere_solid", meta->code, attrNames);
        // 执行
        state_.tccEngine.execute("sphere_solid", &e, e.geoData());
    } else {
        // 内置几何
        GeoDataHandle h = reinterpret_cast<GeoDataHandle>(&e.geoData());
        solid_sphere(&e.geoData(), 0, 0, 0, 1.f, 16, 32);
    }

    e.clearDirty();
    state_.glView.uploadEntity(e.id(), e.geoData());
    state_.selectedEntityId = e.id();
    state_.glView.setSelectedEntity(e.id());
}

// ============================================================
//  文档管理
// ============================================================

// 文件对话框辅助函数
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
static std::string openFileDialog(const char* filter, bool save) {
    char filename[1024] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (save) {
        if (GetSaveFileNameA(&ofn)) return filename;
    } else {
        if (GetOpenFileNameA(&ofn)) return filename;
    }
    return "";
}
#else
// 简化实现：使用ImGui输入框
static std::string lastFileDialogPath = "";
#endif

void MainWindow::onNewDocument() {
    state_.doc.clear();
    state_.glView.clearAll();
    state_.selectedEntityId = 0;
    state_.doc.setFilePath("");
    state_.doc.clearModified();
}

void MainWindow::onOpenDocument() {
    // 简化实现：使用输入框
    static char pathBuf[512] = "";
    ImGui::OpenPopup("Open Document");
}

void MainWindow::onSaveDocument() {
    if (state_.doc.filePath().empty()) {
        onSaveDocumentAs();
    } else {
        if (state_.doc.saveToFile(state_.doc.filePath())) {
            state_.doc.clearModified();
        }
    }
}

void MainWindow::onSaveDocumentAs() {
    // 打开保存对话框
    ImGui::OpenPopup("Save Document As");
}

void MainWindow::onExportStl() {
    auto* entity = state_.doc.findEntity(state_.selectedEntityId);
    if (!entity) return;
    
    if (state_.exportFormat == 0) {
        geo::StlIo::exportEntityAscii(*entity, state_.exportPathBuf_);
    } else {
        geo::StlIo::exportEntityBinary(*entity, state_.exportPathBuf_);
    }
}

void MainWindow::onImportStl() {
    auto* entity = state_.doc.findEntity(state_.selectedEntityId);
    if (!entity) {
        // 创建新实体
        entity = &state_.doc.createEntity("Imported_STL");
        state_.selectedEntityId = entity->id();
    }
    
    if (geo::StlIo::importEntity(*entity, state_.exportPathBuf_)) {
        entity->clearDirty();
        state_.glView.uploadEntity(entity->id(), entity->geoData());
    }
}

void MainWindow::drawExportDialog() {
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Export STL", &state_.showExportDialog)) {
        auto* entity = state_.doc.findEntity(state_.selectedEntityId);
        if (!entity) {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "请先选择一个实体");
        } else {
            ImGui::Text("实体: %s", entity->name().c_str());
            
            ImGui::InputText("文件路径", state_.exportPathBuf_, sizeof(state_.exportPathBuf_));
            
            const char* formats[] = {"ASCII STL", "Binary STL"};
            ImGui::Combo("格式", &state_.exportFormat, formats, 2);
            
            if (ImGui::Button("导出")) {
                onExportStl();
                state_.showExportDialog = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消")) {
                state_.showExportDialog = false;
            }
        }
    }
    ImGui::End();
}

void MainWindow::drawImportDialog() {
    ImGui::SetNextWindowSize(ImVec2(400, 120), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Import STL", &state_.showImportDialog)) {
        ImGui::InputText("文件路径", state_.exportPathBuf_, sizeof(state_.exportPathBuf_));
        
        if (ImGui::Button("导入")) {
            onImportStl();
            state_.showImportDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消")) {
            state_.showImportDialog = false;
        }
    }
    ImGui::End();
}

} // namespace app
