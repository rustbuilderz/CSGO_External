// main.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "offsets.hpp"
#include "memory.h"
#include "menu.h"
#include "aimbot.h"
// Forward to our ESP runner
int RunESP();

int main() {



    // One-time offset dump
    std::cout << "dwEntityList = 0x"
        << std::hex << cs2_dumper::offsets::client_dll::dwEntityList
        << std::dec << "\n";

    // Start background updater
    StartMemoryThread();

    // Give it a moment to fetch
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Print entity data
    std::cout << "Entity Data (Pos | Life | Health | Team) (" << entityCount << "):\n";
    for (int i = 0; i < entityCount; ++i) {
        const auto& v = entityPositions[i];
        std::cout << "[" << i << "] X=" << v.x
            << " Y=" << v.y
            << " Z=" << v.z
            << " LS=" << entityLifeState[i]
            << " HP=" << entityHealth[i]
                << " T=" << entityTeam[i]
                    << "\n";
    }

    Aimbot_Init();
    std::thread([]() {
        // set highest priority so it keeps up
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (!g_shutdown) {
            Aimbot_Update();
            // small sleep to cap CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        }).detach();

    std::thread espThread(RunESP);
    espThread.detach();\

    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
	}

}
