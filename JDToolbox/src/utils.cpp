#include "utils.h"
#include "logger.h"
#include <sstream>
#include <cstring>
#include <psapi.h>

// source: https://github.com/nathan-baggs/nephrite/blob/main/src/payload.cpp
auto wide_str_to_narrow_str(const std::wstring &wstr) -> std::string
{
    auto len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    auto str = std::string(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, str.data(), len, nullptr, nullptr);
    return str;
}


/// <summary>Helper to format pointer / address as hex string</summary>
/// <param name="value">Pointer or address value</param>
/// <returns>Formatted hex string (e.g., "0xDEADBEEF")</returns>
static std::string FormatHex(const uintptr_t value)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << value;
    return ss.str();
}

/// <summary>Detect the game based on process name</summary>
/// <returns>Pointer to matching GameInfo or nullptr if not found</returns>
GameInfo* DetectGame()
{
    wchar_t processPath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, processPath, MAX_PATH);

    // Extract just the filename
    wchar_t* exeName = wcsrchr(processPath, L'\\');
    exeName = exeName ? exeName + 1 : processPath;

    // Convert to std::string for comparison
    char exeNameA[MAX_PATH];
    wcstombs(exeNameA, exeName, MAX_PATH);

    for (auto& game : games) {
        if (_stricmp(game.processName.c_str(), exeNameA) == 0) {
            // File names for SC and SCHD are the same, so we check for the presence of shaderMagic.dll
            if (game.processName == "SantaClausInTrouble.exe")
            {
                if (GetModuleHandleW(L"shaderMagic.dll") != nullptr)
                {
                    Logger::Log("Detected game: " + games[3].gameName);
                    return &games[3]; // Return SCHD
                }
            }
            Logger::Log("Detected game: " + game.gameName);
            return &game;
        }
    }
    Logger::Log("No supported game detected: " + std::string(exeNameA));
    return nullptr;
}

/// <summary>Resolve a multi-level pointer chain</summary>
/// <param name="base">Base address (e.g., module base + static offset)</param>
/// <param name="offsets">Vector of offsets to traverse</param>
/// <returns>Final resolved address or 0 on failure</returns>
uintptr_t ResolvePointer(const uintptr_t base, const std::vector<uintptr_t>& offsets)
{
    if (base == 0) return 0;
    if (offsets.empty()) return base;

    uintptr_t ptr = 0;

    // Read the initial pointer stored at `base` (e.g., module + baseAddress)
    // use memcpy to avoid UB from directly dereferencing arbitrary addresses
    std::memcpy(&ptr, reinterpret_cast<void*>(base), sizeof(ptr));
    if (ptr == 0) {
        Logger::Log("ResolvePointer: Null initial pointer read from " + FormatHex(base));
        return 0;
    }

    // Walk the chain: for each offset except the last, read the pointer at (ptr + offset)
    for (size_t i = 0; i + 1 < offsets.size(); ++i) {
        const uintptr_t addr = ptr + offsets[i];
        std::memcpy(&ptr, reinterpret_cast<void*>(addr), sizeof(ptr));
        if (ptr == 0) {
            Logger::Log("ResolvePointer: Null pointer read from " + FormatHex(addr));
            return 0;
        }
    }

    const uintptr_t finalAddr = ptr + offsets.back();

    return finalAddr;
}

/// <summary>Check if the current process is running with admin privileges</summary>
/// <returns>True if running as admin, false otherwise</returns>
bool IsGameRunningAsAdmin()
{
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te;
        DWORD ret = 0;
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &ret))
            elevated = static_cast<bool>(te.TokenIsElevated);
        CloseHandle(token);
    }
    return elevated == TRUE;
}

/// <summary>Get a list of all loaded modules in the current process</summary>
/// <returns>Vector of HMODULE handles for loaded modules</returns>
std::vector<HMODULE> GetLoadedModules()
{
    std::vector<HMODULE> modules(1024);
    DWORD needed = 0;
    if (EnumProcessModules(GetCurrentProcess(), modules.data(), modules.size() * sizeof(HMODULE), &needed))
    {
        modules.resize(needed / sizeof(HMODULE));
        return modules;
    }
    return {};
}
