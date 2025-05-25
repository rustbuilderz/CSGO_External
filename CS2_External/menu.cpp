#include "menu.h"
#include "imgui/imgui.h"
#include <fstream>
#include <string>

// Simple nav state
static int g_currentPage = 1; // 0=Home,1=ESP,2=Settings

// Path to config file
static const char* CONFIG_PATH = "config.json";
static bool configLoaded = true;

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
    // One-time load
    if (!configLoaded) {
        LoadConfig();
        configLoaded = true;
    }

    ApplySyntheticStyle();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - 600) / 2, (io.DisplaySize.y - 400) / 2), ImGuiCond_FirstUseEver);

    ImGui::Begin("Synthetic Config", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Columns(2, nullptr, true);

    // Navigation
    ImGui::BeginChild("##nav", ImVec2(150, 0), true);
    if (ImGui::Selectable("Home", g_currentPage == 0, ImGuiSelectableFlags_SpanAllColumns)) g_currentPage = 0;
    if (ImGui::Selectable("ESP", g_currentPage == 1, ImGuiSelectableFlags_SpanAllColumns)) g_currentPage = 1;
    if (ImGui::Selectable("Settings", g_currentPage == 2, ImGuiSelectableFlags_SpanAllColumns)) g_currentPage = 2;
    ImGui::EndChild();
    ImGui::NextColumn();

    // Content
    ImGui::BeginChild("##body", ImVec2(0, 0), false);
    if (g_currentPage == 0) {
        ImGui::Text("Welcome to Synthetic!");
    }
    else if (g_currentPage == 1) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "ESP Options");
        ImGui::Separator();
        ImGui::Checkbox("Enable ESP Boxes", &g_showESP);
        ImGui::Checkbox("Show Health Bars", &g_showHealthBar);
        ImGui::Checkbox("Show Team Text", &g_showTeamText);
    }
    else if (g_currentPage == 2) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1), "Settings");
        ImGui::Separator();
        if (ImGui::Button("Save to Config")) {
            SaveConfig();
        }
		if (ImGui::Button("Load from Config")) {
			LoadConfig();
		}
    }
    ImGui::EndChild();
    ImGui::Columns(1);
    ImGui::End();
}
