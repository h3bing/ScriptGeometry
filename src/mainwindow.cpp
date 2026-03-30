/**
 * @file mainwindow.cpp
 * @brief Main application window implementation
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
//  AttrValue visitor helpers
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

MainWindow::MainWindow(GLFWwindow* window)
    : window_(window)
{
    std::memset(modelNameBuf_, 0, sizeof(modelNameBuf_));
    std::memset(modelUrlBuf_,  0, sizeof(modelUrlBuf_));
    std::memset(modelKeyBuf_,  0, sizeof(modelKeyBuf_));
    std::memset(userMsgBuf_,   0, sizeof(userMsgBuf_));

    // Default model config
    const auto& cfg = state_.chatAssistant.modelConfig();
    std::strncpy(modelNameBuf_, cfg.name.c_str(),    sizeof(modelNameBuf_)-1);
    std::strncpy(modelUrlBuf_,  cfg.baseUrl.c_str(), sizeof(modelUrlBuf_)-1);
}

MainWindow::~MainWindow() { shutdown(); }

bool MainWindow::initialize() {
    if (!state_.glView.initialize()) {
        std::cerr << "[MainWindow] GlView init failed\n";
        return false;
    }

    // Load scripts from default directory
    state_.scriptLib.loadDirectory("scripts");

    // Create a demo entity
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
        std::cerr << "[MainWindow] FBO incomplete\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbWidth_ = w; fbHeight_ = h;
}

// ============================================================
//  Update
// ============================================================

void MainWindow::update() {
    // Rebuild dirty entities
    for (auto& ep : state_.doc.entities()) {
        if (ep->isDirty()) {
            rebuildEntityGeometry(*ep);
            ep->clearDirty();
        }
    }

    // Handle pending AI reply
    if (state_.aiPending && !state_.aiPendingText.empty()) {
        state_.aiPending = false;
        // Try to extract and load TOON block
        auto toonBlock = chat::extractToonBlock(state_.aiPendingText);
        if (toonBlock) {
            auto doc = toon::ToonParser::parse(*toonBlock);
            if (doc) {
                state_.scriptLib.loadFile(""); // placeholder – would load from string
                state_.tccEngine.compile(doc->id, doc->code);
                // Create or update entity
                geo::Entity* ent = state_.doc.findEntityByName(doc->name);
                if (!ent) ent = &state_.doc.createEntity(doc->name);
                ent->setScriptId(doc->id);
                ent->markDirty();
                state_.chatAssistant.appendTccResult("Script loaded: " + doc->id);
            }
        }
        state_.aiPendingText.clear();
    }
}

// ============================================================
//  Render (full ImGui layout)
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

    // --- Document panel ---
    if (state_.docPanelOpen)
        drawDocumentPanel(0.f, 0.f, leftW, h);

    // --- GL Viewport ---
    drawMainViewport(leftW, 0.f, centerW, h);

    // --- AI panel ---
    if (state_.aiPanelOpen)
        drawAiPanel(leftW + centerW, 0.f, rightW, h);

    // --- Floating toolbar ---
    float toolbarW = 480.f;
    float toolbarH = 36.f;
    drawToolbar(leftW + centerW * 0.5f, h - toolbarH - 8.f);

    ImGui::End();

    // Settings popup
    if (state_.settingsOpen) drawSettingsWindow();
}

// ============================================================
//  Document panel
// ============================================================

void MainWindow::drawDocumentPanel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("Document##panel", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    // Toggle button
    if (ImGui::Button("<<")) state_.docPanelOpen = false;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f,0.9f,1.f,1.f), "Document");

    ImGui::Separator();
    drawEntityList();

    ImGui::Separator();
    auto* ent = state_.doc.findEntity(state_.selectedEntityId);
    if (ent) drawEntityProperties(ent);

    ImGui::End();
}

void MainWindow::drawEntityList() {
    ImGui::Text("Entities (%zu)", state_.doc.size());
    if (ImGui::Button("+ New")) {
        auto& e = state_.doc.createEntity("Entity_" +
            std::to_string(state_.doc.size()));
        state_.selectedEntityId = e.id();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (state_.selectedEntityId) {
            state_.glView.removeEntity(state_.selectedEntityId);
            state_.doc.removeEntity(state_.selectedEntityId);
            state_.selectedEntityId = 0;
        }
    }

    ImGui::BeginChild("##entityList", ImVec2(0, 150), true);
    for (const auto& ep : state_.doc.entities()) {
        bool selected = (ep->id() == state_.selectedEntityId);
        std::string label = ep->name() + "##" + std::to_string(ep->id());
        if (ImGui::Selectable(label.c_str(), selected))
            onEntitySelected(ep->id());
    }
    ImGui::EndChild();
}

void MainWindow::drawEntityProperties(geo::Entity* entity) {
    ImGui::TextColored(ImVec4(1,0.8f,0.4f,1), "Properties: %s", entity->name().c_str());

    // Script selector
    const char* currentScript = entity->scriptId().empty()
        ? "(none)" : entity->scriptId().c_str();
    ImGui::Text("Script:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##script", currentScript)) {
        if (ImGui::Selectable("(none)", entity->scriptId().empty()))
            entity->setScriptId("");
        for (const auto& sm : state_.scriptLib.scripts()) {
            bool sel = (sm.id == entity->scriptId());
            if (ImGui::Selectable((sm.name + " [" + sm.id + "]").c_str(), sel)) {
                entity->setScriptId(sm.id);
                // Apply default attrs
                auto* meta = state_.scriptLib.findScript(sm.id);
                if (meta) {
                    for (const auto& [k, v] : meta->defaultAttrs())
                        entity->setAttr(k, v);
                }
            }
        }
        ImGui::EndCombo();
    }

    // Attributes
    ImGui::Separator();
    ImGui::Text("Attributes:");
    ImGui::BeginChild("##attrs", ImVec2(0, 0), false);
    for (auto& [key, val] : entity->attrs()) {
        ImGui::PushID(key.c_str());
        ImGui::Text("%s", key.c_str());
        ImGui::SameLine(100.f);

        std::visit([&](auto& v) {
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

// ============================================================
//  AI Panel
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
    float inputAreaH = 120.f;
    float convH = ImGui::GetContentRegionAvail().y - inputAreaH - 50.f;
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
    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();

    if (state_.aiPending) {
        ImGui::TextColored(ImVec4(0.5f,1,0.5f,1), "AI is thinking...");
    }
    if (!state_.aiError.empty()) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Error: %s", state_.aiError.c_str());
    }
}

void MainWindow::drawAiControls() {
    ImGui::Text("Model:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-80.f);
    ImGui::InputText("##model", modelNameBuf_, sizeof(modelNameBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        chat::ModelConfig cfg = state_.chatAssistant.modelConfig();
        cfg.name    = modelNameBuf_;
        cfg.baseUrl = modelUrlBuf_;
        cfg.apiKey  = modelKeyBuf_;
        state_.chatAssistant.setModelConfig(cfg);
    }

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextMultiline("##userMsg", userMsgBuf_, sizeof(userMsgBuf_),
                              ImVec2(0, 60));

    if (ImGui::Button("New")) {
        onNewConversation();
    }
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
//  Toolbar
// ============================================================

void MainWindow::drawToolbar(float centerX, float bottomY) {
    float toolW = 560.f, toolH = 36.f;
    float tx = centerX - toolW * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(tx, bottomY));
    ImGui::SetNextWindowSize(ImVec2(toolW, toolH));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    auto& gv = state_.glView;

    // Axes
    ImGui::Checkbox("Axes",  &gv.showAxes);   ImGui::SameLine();
    ImGui::Checkbox("Grid",  &gv.showGrid);   ImGui::SameLine();

    // Projection
    bool isPerspective = (gv.camera().projection == glview::ProjectionMode::Perspective);
    if (ImGui::Button(isPerspective ? "Persp" : "Ortho")) {
        gv.camera().projection = isPerspective
            ? glview::ProjectionMode::Orthographic
            : glview::ProjectionMode::Perspective;
    }
    ImGui::SameLine();

    // Render mode
    const char* rmLabels[] = {"Solid","Wire","SW","Points"};
    int rm = (int)gv.renderMode;
    ImGui::SetNextItemWidth(80.f);
    if (ImGui::Combo("##rm", &rm, rmLabels, 4))
        gv.renderMode = (glview::RenderMode)rm;
    ImGui::SameLine();

    // Standard views
    for (const char* v : {"Front","Top","Right","Iso"}) {
        if (ImGui::SmallButton(v)) {
            std::string lower(v);
            for (char& c : lower) c = (char)std::tolower(c);
            gv.camera().setStandardView(lower);
        }
        ImGui::SameLine();
    }

    // Toggle panels
    if (!state_.docPanelOpen && ImGui::SmallButton("Doc")) state_.docPanelOpen = true;
    ImGui::SameLine();
    if (!state_.aiPanelOpen  && ImGui::SmallButton("AI"))  state_.aiPanelOpen  = true;

    ImGui::End();
}

// ============================================================
//  Main viewport (renders into FBO, displays as ImGui image)
// ============================================================

void MainWindow::drawMainViewport(float x, float y, float w, float h) {
    int iw = std::max(1, (int)w);
    int ih = std::max(1, (int)h - 44);  // leave room for toolbar

    resizeFbo(iw, ih);

    // Render 3D scene into FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    state_.glView.render(iw, ih);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display as ImGui child window
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##viewport", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    ImGui::Image((ImTextureID)(intptr_t)fbColorTex_, ImVec2((float)iw, (float)ih),
                 ImVec2(0,1), ImVec2(1,0));

    viewportHovered_ = ImGui::IsItemHovered();
    viewportX_ = x; viewportY_ = y;
    viewportW_ = (float)iw; viewportH_ = (float)ih;

    ImGui::End();
}

// ============================================================
//  Settings window
// ============================================================

void MainWindow::drawSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(450, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &state_.settingsOpen)) {
        ImGui::Text("LLM Configuration");
        ImGui::Separator();
        ImGui::InputText("Model Name", modelNameBuf_, sizeof(modelNameBuf_));
        ImGui::InputText("Base URL",   modelUrlBuf_,  sizeof(modelUrlBuf_));
        ImGui::InputText("API Key",    modelKeyBuf_,  sizeof(modelKeyBuf_),
                         ImGuiInputTextFlags_Password);
        if (ImGui::Button("Save")) {
            chat::ModelConfig cfg = state_.chatAssistant.modelConfig();
            cfg.name    = modelNameBuf_;
            cfg.baseUrl = modelUrlBuf_;
            cfg.apiKey  = modelKeyBuf_;
            state_.chatAssistant.setModelConfig(cfg);
            state_.settingsOpen = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) state_.settingsOpen = false;
    }
    ImGui::End();
}

// ============================================================
//  Actions
// ============================================================

void MainWindow::onEntitySelected(uint64_t id) {
    state_.selectedEntityId = id;
    state_.glView.setSelectedEntity(id);
}

void MainWindow::onAttrChanged(geo::Entity& entity, const std::string&) {
    entity.markDirty();
}

void MainWindow::rebuildEntityGeometry(geo::Entity& entity) {
    const std::string& sid = entity.scriptId();
    if (sid.empty()) return;

    // Compile if needed
    const auto* meta = state_.scriptLib.findScript(sid);
    if (!meta) return;

    state_.tccEngine.compile(sid, meta->code);

    // Build the code string with attribute values injected
    // For simplicity, we pass attrs as global vars via a wrapper
    std::string wrappedCode = meta->code;
    state_.tccEngine.execute(sid, entity.geoData());

    state_.glView.uploadEntity(entity.id(), entity.geoData());
}

void MainWindow::onSendAiMessage(const std::string& msg) {
    state_.aiPending = true;
    state_.aiError.clear();

    state_.chatAssistant.chatAsync(
        msg,
        [this](const std::string& reply) {
            state_.aiPendingText = reply;
            state_.aiPending     = false;
        },
        [this](const std::string& err) {
            state_.aiError   = err;
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
    auto& e = state_.doc.createEntity("Demo_Sphere");
    e.setAttr("radius",  1.f);
    e.setAttr("rings",   16);
    e.setAttr("sectors", 32);

    // Inline geometry (sphere) without a TCC script for the demo
    geo::GeoData gd;
    GeoDataHandle h = reinterpret_cast<GeoDataHandle>(&gd);
    solid_sphere(h, 0, 0, 0, 1.f, 16, 32);
    e.setGeoData(gd);
    e.clearDirty();
    state_.glView.uploadEntity(e.id(), gd);
    state_.selectedEntityId = e.id();
    state_.glView.setSelectedEntity(e.id());
}

} // namespace app
