#ifndef JDTOOLBOX_GAME_INFO_H
#define JDTOOLBOX_GAME_INFO_H

#include <cstdint>
#include <string>
#include <vector>

struct GameInfo
{
    std::string gameName; // name
    std::string processName; // moduleName or exeName?
    uintptr_t baseAddress; // pointerBase
    std::vector<uintptr_t> healthOffsets; // playerHealth
    std::vector<uintptr_t> jumpOffsets;
    std::vector<uintptr_t> xPositionOffsets;
    std::vector<uintptr_t> yPositionOffsets;
    std::vector<uintptr_t> zPositionOffsets;
    std::vector<uintptr_t> pastTimeInLevel;
    std::vector<uintptr_t> normalPresents;
    std::vector<uintptr_t> rarePresents;
    std::vector<uintptr_t> epicPresents;
};

inline std::vector<GameInfo> games = {
    { "Santa Claus in Trouble", "SantaClausInTrouble.exe",
        0x0009FE1C, { 0x14, 0x50 }, { 0x14, 0x58 }, { 0x14, 0x14 }, { 0x14, 0x18 }, { 0x14, 0x1C } },

    { "Rosso Rabbit in Trouble", "RossoRabbitInTrouble.exe",
        0x000C00A8, { 0x14, 0x54 }, { 0x14, 0x60 } },

    { "Santa Claus in Trouble... again!", "SantaClaus2.exe",
        0x000B26C8, { 0x14, 0xA0 } },

    { "Santa Claus in Trouble (HD)", "SantaClausInTrouble.exe",
        0x0039BCE8, { 0x50 }}
};

#endif //JDTOOLBOX_GAME_INFO_H
