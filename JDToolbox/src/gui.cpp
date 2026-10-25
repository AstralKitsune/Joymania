#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <dinput.h>
#include <psapi.h>
#include <vector>
#include <algorithm>
#include <cwctype>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "imgui_internal.h"
#include "logger.h"
#include "font.cpp"
#include "utils.cpp"
#include "game_info.h"

// Global variables for ImGui state
bool menu_Show = true;
bool menu_ShowDemoWindow = false;
bool menu_ShowModulesWindow = false;
float menu_Scale = 1.f;
int menu_Width = 600;
int menu_Height = 400;
int menu_InitialX = 100;
int menu_InitialY = 100;
POINTS menu_Position = { 100, 100 };

GameInfo* currentGame = nullptr;

bool isInGame = false;
bool isGameLoading = false;
int health = 0;
int jumpState = 0;
bool infiniteJump = false;
bool freezePosition = false;
float posX = 1.0f;
float posY = 2.0f;
float posZ = 3.0f;
int level = 0;

// Globals for window and device
HWND g_hWnd = nullptr;
WNDCLASSEX wc = {};

// DirectX
LPDIRECT3DDEVICE9       g_pd3dDevice = nullptr;
LPDIRECT3D9             g_pD3D = nullptr;
D3DPRESENT_PARAMETERS   g_d3dpp = {};

// Used whenever the window is resized
void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Create the Window process handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 window procedure
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Send all Window messages through ImGui's handler so it can receive them and act accordingly
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATE;
        /*case WM_SIZE:
        {
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED)
            {
                g_d3dpp.BackBufferWidth = LOWORD(lParam);
                g_d3dpp.BackBufferHeight = HIWORD(lParam);
                ResetDevice();
                Logger::Log("WM_SIZE received, device reset. New size: " + std::to_string(g_d3dpp.BackBufferWidth) + "x" + std::to_string(g_d3dpp.BackBufferHeight));
            }
        }
        return 0;*/

        case WM_SYSCOMMAND:
        {
            if (wParam & 0xfff0 == SC_KEYMENU) // Disable ALT application menu
                return 0;
        }
        break;

        case WM_DESTROY:
        {
            Logger::Log("WM_DESTROY received, quitting.");
            PostQuitMessage(0);
        }
        return 0;

        /*case WM_LBUTTONDOWN:
        {
            menu_Position = MAKEPOINTS(lParam);
        }
        return 0;

        case WM_MOUSEMOVE:
        {
            if (wParam == MK_LBUTTON)
            {
                const auto points = MAKEPOINTS(lParam);
                auto rect = RECT{ };

                GetWindowRect(g_hWnd, &rect);

                rect.left += points.x - menu_Position.x;
                rect.top += points.y - menu_Position.y;

                if (menu_Position.x >= 0 &&
                    menu_Position.x <= menu_Width &&
                    menu_Position.y >= 0 &&
                    menu_Position.y <= 19)
                    SetWindowPos(
                        hWnd,
                        HWND_TOPMOST,
                        rect.left,
                        rect.top,
                        0, 0,
                        SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
                    );
            }
        }
        return 0;*/
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void InitImGuiWindow(const wchar_t* windowName, const wchar_t* className) {
    // Register window class
    wc =
    {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_CLASSDC,
        .lpfnWndProc = WndProc,
        .cbClsExtra = 0L,
        .cbWndExtra = 0L,
        .hInstance = GetModuleHandle(0),
        .hIcon = 0,
        //.hCursor = 0,
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = 0,
        .lpszMenuName = 0,
        .lpszClassName = className,
        .hIconSm = 0
    };

    RegisterClassEx(&wc);
    Logger::Log("Window class registered.");

    g_hWnd = CreateWindowExW( // CreateWindowW
        WS_EX_TOPMOST | WS_EX_LAYERED, //  | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
        wc.lpszClassName,
        windowName,
        // WS_OVERLAPPEDWINDOW,
        WS_POPUP, // | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE, // WS_BORDER
        menu_InitialX,
        menu_InitialY,
        menu_Width,
        menu_Height,
        0,
        0,
        wc.hInstance,
        0
    );
    SetLayeredWindowAttributes(g_hWnd, RGB(0,0,0), 255, LWA_COLORKEY); // ULW_COLORKEY

    if (!g_hWnd) {
        Logger::Log("CreateWindowW failed with error: " + std::to_string(GetLastError()));
        MessageBoxA(NULL, "Failed to create window!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Create D3D9 device
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) {
        Logger::Log("Direct3DCreate9 failed with error: " + std::to_string(GetLastError()));
        MessageBoxA(NULL, "Failed to create Direct3D9 object!", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    Logger::Log("Direct3D9 object created.");

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));

    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;             // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate

    g_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        g_hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &g_d3dpp,
        &g_pd3dDevice
    );
    Logger::Log("Direct3D9 device created.");

    // Show window
    ShowWindow(g_hWnd, SW_SHOW); // SW_SHOWDEFAULT
    //ShowWindow(g_hWnd, SW_HIDE);
    UpdateWindow(g_hWnd);
    SetWindowPos(g_hWnd, HWND_TOPMOST, menu_InitialX, menu_InitialY, menu_Width, menu_Height, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW); // SWP_NOMOVE | SWP_NOSIZE SWP_NOACTIVATE | SWP_SHOWWINDOW

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ::ImGui::GetIO();
    io.IniFilename = "imgui.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../Roboto-Medium.ttf");
    //ImFontConfig font_cfg;
    //font_cfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryCompressedTTF(RobotoMedium_compressed_data, RobotoMedium_compressed_size);
    // IM_ASSERT(font != NULL);

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 3.0f;
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.FramePadding = ImVec2(5, 5);
    style.CellPadding = ImVec2(4.f, 8.f);
    style.FontSizeBase = 18.0f;
    // style.ImGuiCol_CheckboxSelectedBg

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    Logger::Log("Window created successfully.");
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
    Logger::Log("Direct3D9 device destroyed.");
}

void RightTableText(const char* text) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x));
    ImGui::TextDisabled("%s", text);
}

DWORD WINAPI ImGuiThread(LPVOID)
{
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    //SetConsoleOutputCP(CP_UTF8);
    //SetConsoleTitleA(std::format("Debug Console {} - {}", __DATE__, __TIME__).c_str());
    std::cout << "Console created" << std::endl;

    currentGame = DetectGame();
    if (!currentGame) {
        MessageBoxA(NULL, "Game not supported or not detected!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // If the parameter is NULL, GetModuleHandle returns a handle to the file used to create the calling process (.exe file).
    // HMODULE is a handle to a module, which is just the base address of the module in memory.
    HMODULE game_handle = GetModuleHandleA(nullptr);
    if (game_handle == nullptr) {
        Logger::Log("GetModuleHandleA failed with error: " + std::to_string(GetLastError()));
        MessageBoxA(NULL, "Failed to get game handle!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    Logger::Log("Game module base address: " + FormatHex(reinterpret_cast<uintptr_t>(game_handle)));
    std::cout << "Game module base address: " << game_handle << std::endl;

    uintptr_t base = reinterpret_cast<uintptr_t>(game_handle) + currentGame->baseAddress;
    Logger::Log("Pointer base calculated: " + FormatHex(base));

    InitImGuiWindow(L"JDToolbox Window", L"JDToolbox");

    while (menu_Show)
    {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg); // Dispatch messages to WndProc
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({ float(menu_Width), float(menu_Height) }); // , ImGuiCond_Once
        //ImGui::SetNextWindowSize({0, 0}); ImGui::SetWindowSize();
        ImGui::Begin(
                "JDToolbox",
                &menu_Show,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoSavedSettings |
                // ImGuiWindowFlags_NoTitleBar
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove
                );

        ImGui::BeginTabBar("123");
        if (ImGui::BeginTabItem("Player")) {
            if (base == 0)
            {
                Logger::Log("base not initialized, we are probably still loading up the game");
            }

            uintptr_t test = 0;
            std::memcpy(&test, reinterpret_cast<void*>(base), sizeof(test));
            if (test == 0)
            {
                isGameLoading = true;
            }
            else
            {
                isGameLoading = false;
            }

            uintptr_t healthAddr = ResolvePointer(base, currentGame->healthOffsets);
            uintptr_t jumpAddr = ResolvePointer(base, currentGame->jumpOffsets);

            // I currently don't have a reliable way of checking if we are in-game or not, so I just
            // try to read from the health address, which will only work in-game
            if (healthAddr != 0)
            {
                isInGame = true;
            }
            else
            {
                isInGame = false;
            }

            int memHealth = 0;
            if (healthAddr != 0)
            {
                memHealth = *reinterpret_cast<int*>(healthAddr);
            }

            static int uiHealth = 0;
            static bool uiHealthDirty = false;
            static bool uiHealthInitialized = false;

            if (!uiHealthInitialized) {
                uiHealth = memHealth;
                uiHealthInitialized = true;
            }

            if (uiHealthDirty && uiHealth == memHealth) {
                uiHealthDirty = false;
            }

            ImGui::Text("Health:"); // this is a little small compared to the inputint box below. also make apply button a little wider
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);

            ImGui::InputInt("##health", &uiHealth, 0, 0);

            bool inputActive = ImGui::IsItemActive();
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                // user finished editing -> mark dirty so we don't override their edit
                uiHealthDirty = true;
            }

            int uiHealthSnapshot = uiHealth;

            if (!inputActive && !uiHealthDirty) {
                uiHealth = memHealth;
            }

            ImGui::SameLine();
            if (ImGui::Button("Apply")) {
                if (healthAddr != 0)
                {
                    if (std::memcpy(reinterpret_cast<void*>(healthAddr), &uiHealthSnapshot, sizeof(uiHealthSnapshot))) {
                        Logger::Log("Health set to " + std::to_string(uiHealthSnapshot) + " at address " + FormatHex(healthAddr));
                        // apply succeeded -> clear dirty because memory now matches UI
                        uiHealthDirty = false;
                        // optional: update uiHealth to reflect memory (already equals snapshot)
                        uiHealth = uiHealthSnapshot;
                    } else {
                        Logger::Log("Failed to set health at address " + FormatHex(healthAddr));
                    }
                }
            }

            ImGui::Separator();

            ImGui::Text("2nd jump used: "); // Jump State
            if (jumpAddr != 0)
            {
                std::memcpy(&jumpState, reinterpret_cast<int*>(jumpAddr), sizeof(jumpState));
                ImGui::SameLine();
                ImGui::Text(jumpState == 0 ? "No" : "Yes");
            }

            ImGui::Checkbox("Infinite Jump", &infiniteJump);
            if (infiniteJump)
            {
                if (jumpAddr != 0)
                {
                    int n = 0;
                    std::memcpy(reinterpret_cast<int*>(jumpAddr), &n, sizeof(n));
                }
            }

            ImGui::Separator();

            ImGui::Text("Position:");
            ImGui::SameLine();
            ImGui::InputFloat3("##position", &posX, "%.6f"); // , ImGuiInputTextFlags_ReadOnly
            ImGui::Checkbox("Freeze position", &freezePosition);
            if (freezePosition)
            {

            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Levels"))
        {
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("##level", &level, 0, 10, "Level: %d");
            if (ImGui::Button("Load"))
            {
            }
            ImGui::Spacing();
            // put divider here or smth
            if (ImGui::Button("Select Custom Level File (.jml)"))
            {
            }
            ImGui::Text("Selected Level File: ");
            if (ImGui::Button("Load (idk how yet)"))
            {
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("About"))
        {
            ImGui::Text("About");

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
            if (ImGui::BeginChild("AboutChild", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
            {
                // To debug table borders put ImGuiTableFlags_Borders or ImGuiTableFlags_BordersOuter
                ImGui::BeginTable("About", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp);

                ImGui::TableNextRow(0, 20);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Creator:");
                ImGui::TableSetColumnIndex(1);
                RightTableText("Astral");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Build:");
                ImGui::TableSetColumnIndex(1);
                RightTableText(__DATE__);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Mode:");
                ImGui::TableSetColumnIndex(1);
                RightTableText("Debug");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Platform:");
                ImGui::TableSetColumnIndex(1);
                RightTableText("x86_64");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Game:");
                ImGui::TableSetColumnIndex(1);
                RightTableText(currentGame->gameName.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("ImGui Version:");
                ImGui::TableSetColumnIndex(1);
                RightTableText(IMGUI_VERSION);

                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::Spacing();

            // Links Heading
            ImGui::Text("Links");

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
            if (ImGui::BeginChild("LinksChild", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
            {
                ImGui::BeginTable("Links", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp);
                ImGui::TableNextRow(0, 20);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("GitHub:");
                ImGui::TableSetColumnIndex(1);
                const auto text = "astra1dev";
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x));
                ImGui::TextLinkOpenURL(text, "https://github.com/astra1dev");

                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            if (ImGui::Button("Show Demo Window")) menu_ShowDemoWindow = true;
            if (ImGui::Button("Show Loaded Modules")) menu_ShowModulesWindow = true;

            // for some reason this style selector is way too wide (same width as sliderFloat below for some reason)
            ImGui::ShowStyleSelector("Style Selector"); // this only shows dark, light, classic and doesnt respect new styles we might add
                                                        // look at the src for ShowStyleSelector, then use that but add more switch cases for any styles we might add ourselves
            ImGui::ShowFontSelector("Font Selector"); // this loops for (ImFont* font : io.Fonts->Fonts) and uses font->GetDebugName()
            ImGui::ShowDebugLogWindow();

            // put divider here or smth
            ImGui::Spacing();

            ImGui::Text("Debug Information:");
            ImGui::Text("Base Game Address: 0x%p", (void*)base);
            ImGui::Text("Running as: %s", IsGameRunningAsAdmin() ? "Administrator" : "Standard User");
            ImGui::Text("Menu Scale: %.2f", menu_Scale);
            ImGui::SliderFloat("Menu Scale", &menu_Scale, 0.5f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); // ts is too wide
            ImGuiIO& io = ::ImGui::GetIO();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        //ImGui::SameLine(ImGui::GetWindowWidth() - 19 * menu_Scale);
        //ImGui::BeginChild("Scrolling", ImVec2(90 * menuScale, 0), true);
        //ImGui::EndChild();

        // if (ImGui::Checkbox("Check", &val)){ // runs once when checkbox is checked / unchecked }
        // if (ImGui::Button("Load", ImVec2(100, 0))) // button with width 100, height auto

        ImGui::End();

        if (menu_ShowDemoWindow) {
            ImGui::ShowDemoWindow(&menu_ShowDemoWindow);
        }

        if (menu_ShowModulesWindow) {
            ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Loaded Modules", &menu_ShowModulesWindow, ImGuiWindowFlags_NoCollapse)) {
                auto modules = GetLoadedModules();
                for (const auto& mod : modules) {
                    wchar_t modName[MAX_PATH];
                    if (GetModuleFileNameW(mod, modName, MAX_PATH)) {
                        ImGui::Text("- %ls", modName);
                    }
                }
                /*ImGui::Text("File Version Info:");
                wchar_t processPath[MAX_PATH] = {};
                GetModuleFileNameW(NULL, processPath, MAX_PATH);
                DWORD verHandle = 0;
                DWORD verSize = GetFileVersionInfoSizeW(processPath, &verHandle);
                if (verSize > 0)
                {
                    std::vector<char> verData(verSize);
                    if (GetFileVersionInfo(processPath, verHandle, verSize, verData.data()))
                    {
                        VS_FIXEDFILEINFO* fileInfo = nullptr;
                        UINT size = 0;
                        if (VerQueryValue(verData.data(), L"\\", (LPVOID*)&fileInfo, &size))
                        {
                            if (size)
                            {
                                ImGui::Text("  File Version: %d.%d.%d.%d",
                                    (fileInfo->dwFileVersionMS >> 16) & 0xffff,
                                    (fileInfo->dwFileVersionMS >> 0) & 0xffff,
                                    (fileInfo->dwFileVersionLS >> 16) & 0xffff,
                                    (fileInfo->dwFileVersionLS >> 0) & 0xffff);
                                ImGui::Text("  Product Version: %d.%d.%d.%d",
                                    (fileInfo->dwProductVersionMS >> 16) & 0xffff,
                                    (fileInfo->dwProductVersionMS >> 0) & 0xffff,
                                    (fileInfo->dwProductVersionLS >> 16) & 0xffff,
                                    (fileInfo->dwProductVersionLS >> 0) & 0xffff);
                                ImGui::Text("  File Flags Mask: 0x%08X", fileInfo->dwFileFlagsMask);
                                ImGui::Text("  File Flags: 0x%08X", fileInfo->dwFileFlags);
                                ImGui::Text("  File OS: 0x%08X", fileInfo->dwFileOS);
                                ImGui::Text("  File Type: 0x%08X", fileInfo->dwFileType);
                                ImGui::Text("  File Subtype: 0x%08X", fileInfo->dwFileSubtype);
                            }
                        }
                    }
                }*/
            }
            ImGui::End();
        }

        ImGui::EndFrame();

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0,0,0,255), 1.0f, 0);

        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        
        const auto result = g_pd3dDevice->Present(0, 0, 0, 0);
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();

        Sleep(16);
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    fclose(stdout);
    FreeConsole();

    return 0;
}
