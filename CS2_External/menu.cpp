#include "menu.h"
#include "imgui/imgui.h"
#include <fstream>
#include <string>
#include "memory.h"      // brings in extern bool g_teamCheck;
#include "aimbot.h"

bool g_showESP = false;
bool g_showHealthBar = false;
bool g_showTeamText = false;
bool g_menuShown = false;
bool g_showSkeletons = false;
bool g_showNames = false;
bool g_teamCheck = false;
bool g_showBoxes = false;
bool g_showStats = false; 
bool g_shutdown = false;

// Simple nav state
static int g_currentPage = 1; // 0=Home,1=ESP,2=Settings

// Path to config file
static const char* CONFIG_PATH = "config.json";
static bool configLoaded = false;

// Synthetic-like style
static void ApplySyntheticStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.12f, 0.94f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.23f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.36f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.50f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.40f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.60f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.36f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.36f, 0.36f, 0.41f, 1.00f);
}

// Load config from disk (simple parsing)
static void LoadConfig() {
    std::ifstream ifs(CONFIG_PATH);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("showESP") != std::string::npos)
            g_showESP = (line.find("true") != std::string::npos);
        else if (line.find("showHealthBar") != std::string::npos)
            g_showHealthBar = (line.find("true") != std::string::npos);
        else if (line.find("showTeamText") != std::string::npos)
            g_showTeamText = (line.find("true") != std::string::npos);
    }
    ifs.close();
}

// Save config to disk as JSON
static void SaveConfig() {
    std::ofstream ofs(CONFIG_PATH);
    if (!ofs.is_open()) return;
    ofs << "{\n";
    ofs << "  \"showESP\": " << (g_showESP ? "true" : "false") << ",\n";
    ofs << "  \"showHealthBar\": " << (g_showHealthBar ? "true" : "false") << ",\n";
    ofs << "  \"showTeamText\": " << (g_showTeamText ? "true" : "false") << "\n";
    ofs << "}\n";
    ofs.close();
}

void RenderMenu() {
    if (!configLoaded) {
        LoadConfig();
        configLoaded = true;
    }

    ApplySyntheticStyle();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - 600) / 2, (io.DisplaySize.y - 400) / 2), ImGuiCond_FirstUseEver);

    ImGui::Begin("Synthetic Config", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    // --- Header ---
    ImGui::TextWrapped("Configure your overlay settings below. Click Panic in Home tab to exit immediately.");
    ImGui::Separator();

    // --- Tab Bar ---
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_Reorderable)) {
        // Home Tab
        if (ImGui::BeginTabItem("Home")) {
            ImGui::Spacing();
            ImGui::TextWrapped("Welcome to Synthetic!\nUse the tabs above to tweak your ESP, rendering and other options.");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Panic", ImVec2(-1, 0))) {
                g_shutdown = true;
            }
            ImGui::PopStyleColor(2);
            ImGui::EndTabItem();
        }

        // ESP Tab
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Spacing();
            ImGui::Text("General:");
            ImGui::Indent(10);
            ImGui::Checkbox("Enable ESP", &g_showESP);
            ImGui::Checkbox("Show Boxes Around Players", &g_showBoxes);
            ImGui::Checkbox("Show Health Bars", &g_showHealthBar);
            ImGui::Checkbox("Show Team Text", &g_showTeamText);
            ImGui::Unindent(10);

            ImGui::Separator();
            ImGui::Text("Extras:");
            ImGui::Indent(10);
            ImGui::Checkbox("Only show enemies (Team Check)", &g_teamCheck);
            ImGui::Checkbox("Draw Skeletons", &g_showSkeletons);
            ImGui::Checkbox("Draw Names", &g_showNames);
            ImGui::Unindent(10);

            ImGui::Separator();
            ImGui::Checkbox("Show Statistics Overlay", &g_showStats);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Aimbot")) {
            RenderAimbotSettings();
            ImGui::EndTabItem();
        }

        // Settings Tab
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Spacing();
            ImGui::Text("Configuration:");
            ImGui::Indent(10);
            if (ImGui::Button("Save to Config", ImVec2(120, 0))) {
                SaveConfig();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load from Config", ImVec2(120, 0))) {
                LoadConfig();
            }
            ImGui::Unindent(10);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
