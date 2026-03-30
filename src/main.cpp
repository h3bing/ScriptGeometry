/**
 * @file main.cpp
 * @brief ScriptGeometry – application entry point
 *
 * Initializes GLFW, GLAD, ImGui, and launches the main window.
 */

#include "mainwindow.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>

// ============================================================
//  GLFW error callback
// ============================================================

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "[GLFW Error " << error << "] " << description << "\n";
}

// ============================================================
//  Mouse callbacks – forwarded to MainWindow / GlView
// ============================================================

static app::MainWindow* g_mainWindow = nullptr;

static void mouseButtonCallback(GLFWwindow* /*w*/, int button, int action, int /*mods*/) {
    if (!g_mainWindow) return;
    double mx, my;
    glfwGetCursorPos(g_mainWindow->state().glView.camera().projection ==
                     glview::ProjectionMode::Perspective
                     ? nullptr : nullptr, &mx, &my); // unused here
    // Let ImGui handle UI, pass to glView only if viewport is hovered
    if (!ImGui::GetIO().WantCaptureMouse) {
        double cx, cy;
        glfwGetCursorPos(glfwGetCurrentContext(), &cx, &cy);
        g_mainWindow->state().glView.onMouseButton(button, action,
                                                    (float)cx, (float)cy);
    }
}

static void cursorPosCallback(GLFWwindow* /*w*/, double xpos, double ypos) {
    if (!g_mainWindow) return;
    if (!ImGui::GetIO().WantCaptureMouse)
        g_mainWindow->state().glView.onMouseMove((float)xpos, (float)ypos);
}

static void scrollCallback(GLFWwindow* /*w*/, double /*xoff*/, double yoff) {
    if (!g_mainWindow) return;
    if (!ImGui::GetIO().WantCaptureMouse)
        g_mainWindow->state().glView.onMouseScroll((float)yoff);
}

static void framebufferSizeCallback(GLFWwindow* /*w*/, int /*width*/, int /*height*/) {
    // Nothing to do; render() reads the current size each frame
}

// ============================================================
//  main
// ============================================================

int main(int /*argc*/, char** /*argv*/) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "[main] GLFW init failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1400, 860, "ScriptGeometry", nullptr, nullptr);
    if (!window) {
        std::cerr << "[main] Window creation failed\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[main] GLAD load failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "[main] OpenGL " << glGetString(GL_VERSION)
              << " | Renderer: " << glGetString(GL_RENDERER) << "\n";

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.f;
    style.FrameRounding  = 3.f;
    style.ItemSpacing    = ImVec2(6, 4);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Application
    auto mainWindow = std::make_unique<app::MainWindow>(window);
    g_mainWindow = mainWindow.get();

    if (!mainWindow->initialize()) {
        std::cerr << "[main] MainWindow init failed\n";
        return 1;
    }

    // Input callbacks
    glfwSetMouseButtonCallback(window,    mouseButtonCallback);
    glfwSetCursorPosCallback(window,      cursorPosCallback);
    glfwSetScrollCallback(window,         scrollCallback);
    glfwSetFramebufferSizeCallback(window,framebufferSizeCallback);

    // ---- Main loop ----
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Update logic
        mainWindow->update();

        // Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw UI (includes 3D viewport via FBO)
        mainWindow->render();

        // Final OpenGL clear + ImGui draw
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    mainWindow->shutdown();
    g_mainWindow = nullptr;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
