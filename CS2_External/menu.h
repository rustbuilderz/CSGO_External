// menu.h
#pragma once

// toggles used by both esp.cpp and menu.cpp:
extern bool g_showESP;
extern bool g_showHealthBar;
extern bool g_showTeamText;
extern bool g_menuShown;
extern bool g_showSkeletons;
extern bool g_showNames;
extern bool g_teamCheck;
extern bool g_showBoxes;
extern bool g_showStats;
extern bool g_shutdown;
extern bool g_drawCrosshair;
extern bool g_rainbowMode;
// Renders the configuration menu
void RenderMenu();
