// main.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "offsets.hpp"
#include "memory.h"

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
    std::thread espThread(RunESP);
    espThread.detach();

    // block forever (or until you signal exit)
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}
