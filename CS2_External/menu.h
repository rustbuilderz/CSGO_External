// menu.h
#pragma once

// toggles used by both esp.cpp and menu.cpp:
extern bool g_showESP;
extern bool g_showHealthBar;
extern bool g_showTeamText;
extern bool g_menuShown;

// Renders the configuration menu
void RenderMenu();
