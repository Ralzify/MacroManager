#include "gui.h"
#include "macro_manager.h"
#include "persistence.h"
#include "hook_manager.h"
#include "macro.h"
#include "recorder.h"
#include "sound_player.h"
#include "updater.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_internal.h"

#include <dwmapi.h>
#include <ShlObj.h>
#include <wincodec.h>
#include <d3d11.h>

#undef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include <string>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static float AnimTo(ImGuiID id, float target, float speed)
{
    static std::unordered_map<ImGuiID, float> Store;
    auto It = Store.find(id);
    float Current = (It == Store.end()) ? target : It->second;
    float Dt = ImGui::GetIO().DeltaTime;
    Current += (target - Current) * (1.0f - expf(-speed * Dt));
    Store[id] = Current;
    return Current;
}

bool Gui::Init(HINSTANCE hInstance)
{
    if (!CreateAppWindow(hInstance)) 
        return false;

    if (!CreateDeviceD3D())          
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "macro_app_ui.ini";

    ImFontConfig FontConfig;
    FontConfig.FontDataOwnedByAtlas = false;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)Font, sizeof(Font), 17.f, &FontConfig);

    ImGui::StyleColorsDark();
    ImGuiStyle& Style = ImGui::GetStyle();
    Style.WindowRounding = 8.0f;
    Style.ChildRounding = 8.0f;
    Style.FrameRounding = 6.0f;
    Style.GrabRounding = 6.0f;
    Style.PopupRounding = 6.0f;
    Style.ScrollbarRounding = 6.0f;
    Style.TabRounding = 6.0f;
    Style.FrameBorderSize = 0.0f;
    Style.FramePadding = { 8, 5 };
    Style.ItemSpacing = { 8, 6 };
    Style.WindowPadding = { 12, 12 };

    ImVec4* Color = Style.Colors;
    Color[ImGuiCol_WindowBg] = { 0.10f, 0.10f, 0.13f, 1.00f };
    Color[ImGuiCol_ChildBg] = { 0.13f, 0.13f, 0.17f, 1.00f };
    Color[ImGuiCol_PopupBg] = { 0.10f, 0.10f, 0.13f, 0.97f };
    Color[ImGuiCol_Button] = { 0.20f, 0.20f, 0.28f, 1.00f };
    Color[ImGuiCol_ButtonHovered] = { 0.30f, 0.45f, 0.90f, 1.00f };
    Color[ImGuiCol_ButtonActive] = { 0.20f, 0.35f, 0.80f, 1.00f };
    Color[ImGuiCol_FrameBg] = { 0.16f, 0.16f, 0.22f, 1.00f };
    Color[ImGuiCol_FrameBgHovered] = { 0.22f, 0.22f, 0.30f, 1.00f };
    Color[ImGuiCol_FrameBgActive] = { 0.26f, 0.26f, 0.38f, 1.00f };
    Color[ImGuiCol_Header] = { 0.20f, 0.20f, 0.28f, 1.00f };
    Color[ImGuiCol_HeaderHovered] = { 0.26f, 0.26f, 0.38f, 1.00f };
    Color[ImGuiCol_TitleBg] = { 0.08f, 0.08f, 0.10f, 1.00f };
    Color[ImGuiCol_TitleBgActive] = { 0.12f, 0.12f, 0.18f, 1.00f };
    Color[ImGuiCol_CheckMark] = { 0.40f, 0.65f, 1.00f, 1.00f };
    Color[ImGuiCol_Tab] = { 0.15f, 0.15f, 0.20f, 1.00f };
    Color[ImGuiCol_TabHovered] = { 0.30f, 0.45f, 0.90f, 1.00f };
    Color[ImGuiCol_TabActive] = { 0.20f, 0.35f, 0.75f, 1.00f };

    ImGui_ImplWin32_Init(Window);
    ImGui_ImplDX11_Init(Device, Context);

    LoadIconTexture();

    Persistence::Load(MacroManager::Get().GetMacros(), Persistence::DefaultFilePath());
    MacroManager::Get().RebindAll();
    strcpy_s(ExportPathBuf, Persistence::DefaultFilePath().c_str());

    Updater::Get().CheckForUpdatesAsync();

    return true;
}

void Gui::Run()
{
    ShowWindow(Window, SW_SHOWDEFAULT);
    UpdateWindow(Window);
    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); DispatchMessageW(&msg); 
            continue;
        }

        if (CapturingKey)
        {
            for (int vk = 8; vk < 256; ++vk)
            {
                if (GetAsyncKeyState(vk) & 0x8000)
                {
                    if (auto* Macro = MacroManager::Get().FindMacro(CapturingMacroId))
                        Macro->TriggerKey = vk; MacroManager::Get().RebindAll();

                    CapturingKey = false; break;
                }
            }
        }

        if (!CapturingKey && !CapturingRecordKey && !RecordOptionsOpen && RecordOptions.ToggleKey != 0 && !SelectedMacroId.empty())
        {
            static bool WasDown = false;
            bool IsDown = (GetAsyncKeyState(RecordOptions.ToggleKey) & 0x8000) != 0;

            if (IsDown && !WasDown)
            {
                if (!Recorder::Get().IsRecording())
                    Recorder::Get().Start(RecordOptions);
                else
                {
                    auto Rec = Recorder::Get().Stop();

                    if (!Rec.empty())
                    {
                        if (auto* Macro = MacroManager::Get().FindMacro(SelectedMacroId))
                        {
                            Macro->Actions.insert(Macro->Actions.end(), Rec.begin(), Rec.end());
                            StatusMessage = "Recorded " + std::to_string(Rec.size()) + " action(s) added.";
                            StatusTimer = 4.0f;
                        }
                    }
                    else 
                    {
                        StatusMessage = "Recording stopped — no actions captured."; StatusTimer = 3.0f; 
                    }
                }
            }

            WasDown = IsDown;
        }
        else
        {
            static bool WasDown = false;
            WasDown = (RecordOptions.ToggleKey != 0) ? (GetAsyncKeyState(RecordOptions.ToggleKey) & 0x8000) != 0 : false;
        }

        if (!CapturingKey && !RecordOptionsOpen && RecordOptions.MacroToggleKey != 0 && !SelectedMacroId.empty())
        {
            static bool MacroToggleWasDown = false;
            bool IsDown = (GetAsyncKeyState(RecordOptions.MacroToggleKey) & 0x8000) != 0;

            if (IsDown && !MacroToggleWasDown)
            {
                if (auto* Macro = MacroManager::Get().FindMacro(SelectedMacroId))
                {
                    bool nowEnabled = !Macro->Enabled;
                    SetMacroEnabled(SelectedMacroId, nowEnabled);
                    StatusMessage = std::string("Macro \"") + Macro->Name + (nowEnabled ? "\" enabled." : "\" disabled.");
                    StatusTimer = 3.0f;
                }
            }

            MacroToggleWasDown = IsDown;
        }
        else if (RecordOptions.MacroToggleKey != 0)
        {
            static bool MacroToggleWasDown = false;
            MacroToggleWasDown = (GetAsyncKeyState(RecordOptions.MacroToggleKey) & 0x8000) != 0;
        }

        if (StatusTimer > 0.0f) 
            StatusTimer -= 1.0f / 60.0f;

        RenderFrame();
    }
}

void Gui::RenderFrame()
{
    UpdateState PrevUpdateState = Updater::Get().State();
    Updater::Get().Poll();
    UpdateState NewUpdateState = Updater::Get().State();

    if (PrevUpdateState == UpdateState::Checking && NewUpdateState != UpdateState::Checking)
    {
        bool IsStartupCheck = !HasCheckedForUpdatesThisSession;
        HasCheckedForUpdatesThisSession = true;

        if (!IsStartupCheck || NewUpdateState == UpdateState::UpdateAvailable)
            ShowUpdateDialog = true;
    }

    if (PrevUpdateState != UpdateState::Launching && NewUpdateState == UpdateState::Launching)
        QuitAfterLaunchingInstallerTimer = 1.5f;

    if (QuitAfterLaunchingInstallerTimer >= 0.0f)
    {
        QuitAfterLaunchingInstallerTimer -= 1.0f / 60.0f;

        if (QuitAfterLaunchingInstallerTimer <= 0.0f)
            PostMessageW(Window, WM_CLOSE, 0, 0);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0,0 });
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("##Main", nullptr, wf);

    DrawToolbar();
    ImGui::Separator();

    float lw = 260.0f;
    float ew = io.DisplaySize.x - lw - 32.0f;
    float ph = io.DisplaySize.y - 104.0f;

    ImGui::BeginChild("##List", { lw, ph }, true);
    DrawMacroList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##Editor", { ew, ph }, true);

    if (!SelectedMacroId.empty()) 
        DrawMacroEditor(SelectedMacroId);
    else 
    {
        ImGui::SetCursorPosY(ph * 0.45f);
        ImGui::SetCursorPosX((ew - 280) * 0.5f);
        ImGui::TextDisabled("Select or create a macro to edit it.");
    }

    ImGui::EndChild();
    ImGui::Separator();
    DrawStatusBar();
    ImGui::End();

    if (ShowImportDialog) 
    { 
        ImGui::OpenPopup("Import Macros"); 
        ShowImportDialog = false; 
    }

    if (ImGui::BeginPopupModal("Import Macros", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("JSON file path:");
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("##ip", ImportPathBuf, sizeof(ImportPathBuf));
        ImGui::SameLine();

        if (ImGui::Button("Browse...##imp"))
        {
            IFileOpenDialog* pDlg = nullptr;

            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))))
            {
                COMDLG_FILTERSPEC filter[] = {
                    { L"JSON Files", L"*.json" },
                    { L"All Files",  L"*.*" }
                };

                pDlg->SetFileTypes(2, filter);
                pDlg->SetDefaultExtension(L"json");
                pDlg->SetTitle(L"Import Macros");

                if (ImportPathBuf[0] != '\0')
                {
                    char tmp[512]; strncpy_s(tmp, ImportPathBuf, sizeof(tmp) - 1);
                    char* LastSlash = strrchr(tmp, '\\');

                    if (!LastSlash) 
                        LastSlash = strrchr(tmp, '/');

                    if (LastSlash)
                    {
                        *LastSlash = '\0';
                        wchar_t wdir[512] = {};
                        MultiByteToWideChar(CP_ACP, 0, tmp, -1, wdir, 512);
                        IShellItem* pFolder = nullptr;

                        if (SUCCEEDED(SHCreateItemFromParsingName(wdir, nullptr, IID_PPV_ARGS(&pFolder))))
                            pDlg->SetFolder(pFolder); pFolder->Release();
                    }
                }

                if (SUCCEEDED(pDlg->Show(Window)))
                {
                    IShellItem* pItem = nullptr;

                    if (SUCCEEDED(pDlg->GetResult(&pItem)))
                    {
                        PWSTR pPath = nullptr;

                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
                        {
                            WideCharToMultiByte(CP_ACP, 0, pPath, -1, ImportPathBuf, sizeof(ImportPathBuf), nullptr, nullptr);
                            CoTaskMemFree(pPath);
                        }

                        pItem->Release();
                    }
                }

                pDlg->Release();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Import", { 120,0 })) 
        {
            bool CanImport = Persistence::Load(MacroManager::Get().GetMacros(), ImportPathBuf);

            if (CanImport)
                MacroManager::Get().RebindAll();

            StatusMessage = CanImport ? "Macros imported." : "Import failed.";
            StatusTimer = 4.0f; ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 120,0 })) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (ShowImportMGP)
    { 
        ImGui::OpenPopup("Import MacroGamer Profile"); ShowImportMGP = false; 
    }

    if (ImGui::BeginPopupModal("Import MacroGamer Profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Import macros from a MacroGamer .mgp profile file.");
        ImGui::TextDisabled("Usually found in: C:\\Users\\<you>\\Documents\\MacroGamer");
        ImGui::Spacing();
        ImGui::Text(".mgp file path:");
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("##mgp", MGPPathBuf, sizeof(MGPPathBuf));

        ImGui::SameLine();
        if (ImGui::Button("Browse...##mgp"))
        {
            IFileOpenDialog* pDlg = nullptr;

            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))))
            {
                COMDLG_FILTERSPEC filter[] = {
                    { L"MacroGamer Profile", L"*.mgp" },
                    { L"All Files", L"*.*" }
                };

                pDlg->SetFileTypes(2, filter);
                pDlg->SetDefaultExtension(L"mgp");
                pDlg->SetTitle(L"Import MacroGamer Profile");

                if (MGPPathBuf[0] == '\0')
                {
                    IShellItem* pDocs = nullptr;

                    if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&pDocs))))
                    {
                        PWSTR pDocsPath = nullptr;

                        if (SUCCEEDED(pDocs->GetDisplayName(SIGDN_FILESYSPATH, &pDocsPath)))
                        {
                            std::wstring mgDir = std::wstring(pDocsPath) + L"\\MacroGamer";
                            CoTaskMemFree(pDocsPath);
                            IShellItem* pMgFolder = nullptr;

                            if (SUCCEEDED(SHCreateItemFromParsingName(mgDir.c_str(), nullptr, IID_PPV_ARGS(&pMgFolder))))
                            {
                                pDlg->SetFolder(pMgFolder); 
                                pMgFolder->Release();
                            }
                            else
                            {
                                pDlg->SetFolder(pDocs);
                            }
                        }

                        pDocs->Release();
                    }
                }
                else
                {
                    char tmp[512]; strncpy_s(tmp, MGPPathBuf, sizeof(tmp) - 1);
                    char* lastSlash = strrchr(tmp, '\\');

                    if (!lastSlash) 
                        lastSlash = strrchr(tmp, '/');

                    if (lastSlash)
                    {
                        *lastSlash = '\0';
                        wchar_t wdir[512] = {};
                        MultiByteToWideChar(CP_ACP, 0, tmp, -1, wdir, 512);
                        IShellItem* pFolder = nullptr;

                        if (SUCCEEDED(SHCreateItemFromParsingName(wdir, nullptr, IID_PPV_ARGS(&pFolder))))
                        {
                            pDlg->SetFolder(pFolder); 
                            pFolder->Release();
                        }
                    }
                }

                if (SUCCEEDED(pDlg->Show(Window)))
                {
                    IShellItem* pItem = nullptr;

                    if (SUCCEEDED(pDlg->GetResult(&pItem)))
                    {
                        PWSTR pPath = nullptr;

                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
                        {
                            WideCharToMultiByte(CP_ACP, 0, pPath, -1, MGPPathBuf, sizeof(MGPPathBuf), nullptr, nullptr);
                            CoTaskMemFree(pPath);
                        }

                        pItem->Release();
                    }
                }

                pDlg->Release();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Import##mgpgo", { 120,0 }))
        {
            int n = Persistence::ImportMGP(MacroManager::Get().GetMacros(), MGPPathBuf);

            if (n < 0)
                StatusMessage = "MGP import failed — check the file path.";
            else
            {
                MacroManager::Get().RebindAll();
                StatusMessage = "Imported " + std::to_string(n) + " macro(s) from MacroGamer profile.";
            }

            StatusTimer = 5.0f;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel##mgp", { 120,0 })) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    if (ShowExportDialog) 
    { 
        ImGui::OpenPopup("Export Macros"); 
        ShowExportDialog = false; 
    }

    if (ImGui::BeginPopupModal("Export Macros", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Save to path:");
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("##ep", ExportPathBuf, sizeof(ExportPathBuf));

        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            IFileSaveDialog* pDlg = nullptr;

            if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))))
            {
                COMDLG_FILTERSPEC filter[] = {
                    { L"JSON Files", L"*.json" },
                    { L"All Files",  L"*.*"    }
                };

                pDlg->SetFileTypes(2, filter);
                pDlg->SetDefaultExtension(L"json");
                pDlg->SetTitle(L"Export Macros");

                if (ExportPathBuf[0] != '\0')
                {
                    char tmp[512]; strncpy_s(tmp, ExportPathBuf, sizeof(tmp) - 1);
                    char* lastSlash = strrchr(tmp, '\\');

                    if (!lastSlash) 
                        lastSlash = strrchr(tmp, '/');

                    if (lastSlash)
                    {
                        *lastSlash = '\0';
                        wchar_t wdir[512] = {};
                        MultiByteToWideChar(CP_ACP, 0, tmp, -1, wdir, 512);
                        IShellItem* pFolder = nullptr;

                        if (SUCCEEDED(SHCreateItemFromParsingName(wdir, nullptr, IID_PPV_ARGS(&pFolder))))
                        {
                            pDlg->SetFolder(pFolder);
                            pFolder->Release();
                        }

                        wchar_t wfile[512] = {};
                        MultiByteToWideChar(CP_ACP, 0, lastSlash + 1, -1, wfile, 512);
                        pDlg->SetFileName(wfile);
                    }
                }

                if (SUCCEEDED(pDlg->Show(Window)))
                {
                    IShellItem* pItem = nullptr;

                    if (SUCCEEDED(pDlg->GetResult(&pItem)))
                    {
                        PWSTR pPath = nullptr;

                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
                        {
                            WideCharToMultiByte(CP_ACP, 0, pPath, -1, ExportPathBuf, sizeof(ExportPathBuf), nullptr, nullptr);
                            CoTaskMemFree(pPath);
                        }

                        pItem->Release();
                    }
                }

                pDlg->Release();
            }
        }

        ImGui::Spacing();

        if (ImGui::Button("Export", { 120,0 })) 
        {
            bool ok = Persistence::Save(MacroManager::Get().GetMacros(), ExportPathBuf);
            StatusMessage = ok ? "Macros exported." : "Export failed — check the path.";
            StatusTimer = 4.0f; ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 120,0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ShowAbout) 
    { 
        ImGui::OpenPopup("About");
        ShowAbout = false;
    }

    if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Ralzify's Macro Manager | " APP_VERSION_STRING);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Keyboard & mouse macro recorder/player for Windows.");
        ImGui::Spacing();
        ImGui::Text("NOTE: If you paid for this application, you got scammed.");
        ImGui::Text("The full source code to the project can be found on");
		ImGui::SameLine();
		ImGui::TextColored({ 0.4f,0.7f,1.0f,1.0f }, "github.com/Ralzify/MacroManager");
        ImGui::Spacing();
        if (ImGui::Button("Check for Updates", { 160,0 }))
        {
            if (Updater::Get().State() != UpdateState::Checking && Updater::Get().State() != UpdateState::Downloading)
                Updater::Get().CheckForUpdatesAsync();
        }
        ImGui::Spacing();
        if (ImGui::Button("Close", { 120,0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    DrawUpdateDialog();

    if (ShowRecordOptions) 
    { 
        ImGui::OpenPopup("Record Options"); 
        ShowRecordOptions = false; 
    }

    if (ImGui::BeginPopupModal("Record Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Record Key:");
        ImGui::SameLine(0, 12);

        if (CapturingRecordKey)
        {
            ImGui::TextColored({ 1,0.8f,0.2f,1 }, "Press any key...");

            for (int vk = 8; vk < 256; ++vk)
            {
                if (GetAsyncKeyState(vk) & 0x8000)
                {
                    RecordOptions.ToggleKey = (vk == VK_ESCAPE) ? 0 : vk;
                    CapturingRecordKey = false;
                    break;
                }
            }
        }
        else
        {
            if (ImGui::Button(VKCodeToName(RecordOptions.ToggleKey).c_str(), { 120,0 }))
                CapturingRecordKey = true;

            ImGui::SameLine();
            if (ImGui::Button("Clear##tk")) RecordOptions.ToggleKey = 0;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RecordOptionsOpen = true;
        ImGui::Text("What to capture:");
        ImGui::Spacing();
        ImGui::Checkbox("Key presses", &RecordOptions.RecordKeyPress);
        ImGui::Checkbox("Mouse clicks", &RecordOptions.RecordMouseClick);
        ImGui::Checkbox("Mouse movement", &RecordOptions.RecordMouseMove);
        ImGui::Checkbox("Delays", &RecordOptions.RecordDelays);

        if (RecordOptions.RecordDelays || RecordOptions.RecordMouseMove)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (RecordOptions.RecordDelays)
            {
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("Minimum Delay (ms)", &RecordOptions.MinMsDelay);

                if (RecordOptions.MinMsDelay < 0)
                    RecordOptions.MinMsDelay = 0;
            }

            if (RecordOptions.RecordMouseMove)
            {
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("Mouse Sample Interval (ms)", &RecordOptions.MouseMoveInterval);

                if (RecordOptions.MouseMoveInterval < 1)
                    RecordOptions.MouseMoveInterval = 1;
            }
        }

        ImGui::Spacing(); 
        ImGui::Separator(); 
        ImGui::Spacing();

        ImGui::Text("Toggle Selected Macro:");
        ImGui::SameLine(0, 12);

        static bool CapturingMacroToggleKey = false;
        if (CapturingMacroToggleKey)
        {
            ImGui::TextColored({ 1, 0.8f, 0.2f, 1 }, "Press any key...");
            for (int vk = 8; vk < 256; ++vk)
            {
                if (GetAsyncKeyState(vk) & 0x8000)
                {
                    RecordOptions.MacroToggleKey = (vk == VK_ESCAPE) ? 0 : vk;
                    CapturingMacroToggleKey = false;
                    break;
                }
            }
        }
        else
        {
            std::string mtkLabel = VKCodeToName(RecordOptions.MacroToggleKey) + "##mtk";

            if (ImGui::Button(mtkLabel.c_str(), { 120, 0 }))
                CapturingMacroToggleKey = true;

            ImGui::SameLine();
            if (ImGui::Button("Clear##mtk2")) RecordOptions.MacroToggleKey = 0;
        }

        ImGui::Checkbox("Play chime on toggle", &RecordOptions.MacroToggleChime);
        // ImGui::TextDisabled("  Place enabled.mp3 / disabled.mp3 next to the .exe");

        ImGui::Spacing();

        if (ImGui::Button("OK", { 120, 0 }))
        {
            CapturingRecordKey = false;
            CapturingMacroToggleKey = false;
            RecordOptionsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        if (RecordOptionsOpen)
        {
            RecordOptionsOpen = false;
            CapturingRecordKey = false;
        }
    }

    if (ShowActionEditor) 
    { 
        ImGui::OpenPopup("Edit Action"); 
        ShowActionEditor = false; 
    }

    if (ImGui::BeginPopupModal("Edit Action", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        DrawActionEditor(SelectedMacroId, EditActionIdx);
        ImGui::Spacing();

        if (ImGui::Button("Done", { 120,0 })) 
        { 
            CapturingActionKey = false; 
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

    if (ConfirmClearActions) 
    { 
        ImGui::OpenPopup("Confirm Clear"); 
        ConfirmClearActions = false; 
    }

    if (ImGui::BeginPopupModal("Confirm Clear", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (auto* Macro = MacroManager::Get().FindMacro(SelectedMacroId))
            ImGui::Text("Delete all %zu action(s) from \"%s\"?", Macro->Actions.size(), Macro->Name.c_str());

        ImGui::Text("This cannot be undone.");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f,0.15f,0.15f,1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f,0.20f,0.20f,1 });

        if (ImGui::Button("Delete All", { 110,0 }))
        {
            if (auto* Macro = MacroManager::Get().FindMacro(SelectedMacroId))
                Macro->Actions.clear();

            EditActionIdx = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 110,0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Render();
    const float clr[4] = { 0.08f,0.08f,0.10f,1.0f };
    Context->OMSetRenderTargets(1, &MainRenderTargetView, nullptr);
    Context->ClearRenderTargetView(MainRenderTargetView, clr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    SwapChain->Present(1, 0);
}

void Gui::DrawToolbar()
{
    ImGui::Spacing();
    ImGui::Text("  Ralzify's Macro Manager"); ImGui::SameLine(0, 20);

    if (ImGui::Button("+ New Macro"))
    {
        auto& Macro = MacroManager::Get().AddMacro("New Macro"); 
        SelectedMacroId = Macro.ID;
    }

    ImGui::SameLine();

    if (ImGui::Button("Import MGP"))
        ShowImportMGP = true;

    ImGui::SameLine();
    if (ImGui::Button("Stop All"))
    {
        MacroManager::Get().StopAll();
        StatusMessage = "All macros disabled.";
        StatusTimer = 3.0f;
    }

    ImGui::SameLine();

    if (ImGui::Button("About"))
        ShowAbout = true;

    if (!SelectedMacroId.empty())
    {
        ImGui::SameLine(0, 24);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 24);

        /*if (ImGui::Button("Save"))
        {
            bool CanSave = Persistence::Save(MacroManager::Get().GetMacros(), Persistence::DefaultFilePath());
            StatusMessage = CanSave ? "Macros saved." : "Save failed.";
            StatusTimer = 3.0f;
        }

        ImGui::SameLine();*/

        if (ImGui::Button("Import"))  
            ShowImportDialog = true;

        ImGui::SameLine(); 
        if (ImGui::Button("Export"))  
            ShowExportDialog = true;

        ImGui::SameLine(0, 24);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 24);

        bool IsRecording = Recorder::Get().IsRecording();

        if (!IsRecording)
        {
            if (ImGui::Button("Record Options"))
                ShowRecordOptions = true;

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, { 0.15f,0.50f,0.15f,1 });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.20f,0.70f,0.20f,1 });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.10f,0.40f,0.10f,1 });

            if (ImGui::Button("Start Recording")) 
                Recorder::Get().Start(RecordOptions);

            ImGui::PopStyleColor(3);
        }
        else
        {
            float p = (sinf((float)ImGui::GetTime() * 4.0f) + 1) * 0.5f;
            ImGui::PushStyleColor(ImGuiCol_Text, { 1, p * 0.4f + 0.2f,0.2f,1 });
            ImGui::Text("RECORDING...");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f,0.15f,0.15f,1 });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f,0.20f,0.20f,1 });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.45f,0.10f,0.10f,1 });

            if (ImGui::Button("Stop Recording"))
            {
                auto Recorder = Recorder::Get().Stop();
                if (!Recorder.empty())
                {
                    if (auto* Macro = MacroManager::Get().FindMacro(SelectedMacroId))
                    {
                        Macro->Actions.insert(Macro->Actions.end(), Recorder.begin(), Recorder.end());
                        StatusMessage = "Recorded " + std::to_string(Recorder.size()) + " action(s) added.";
                        StatusTimer = 4.0f;
                    }
                }
                else 
                {
                    StatusMessage = "Recording stopped — no actions captured."; 
                    StatusTimer = 3.0f; 
                }
            }

            ImGui::PopStyleColor(3);
        }

        if (IconTexture)
        {
            const float IconSize = 46.0f;
            const float SlotSize = ImGui::GetFrameHeight();

            ImGui::SameLine(0, 64);
            ImVec2 SlotMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy({ SlotSize, SlotSize });

            ImVec2 Center = { SlotMin.x + SlotSize * 0.5f, SlotMin.y + SlotSize * 0.5f };
            ImVec2 ImgMin = { Center.x - IconSize * 0.5f, Center.y - IconSize * 0.5f };
            ImVec2 ImgMax = { Center.x + IconSize * 0.5f, Center.y + IconSize * 0.5f };
            ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)IconTexture, ImgMin, ImgMax);
        }
    }

    ImGui::Spacing();
}

void Gui::PlayToggleChime(bool enabled)
{
    if (!RecordOptions.MacroToggleChime)
        return;

    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string Directory = exePath;
    auto Slash = Directory.find_last_of("\\/");

    if (Slash != std::string::npos)
        Directory = Directory.substr(0, Slash);

    std::string Sound = Directory + (enabled ? "\\enabled.mp3" : "\\disabled.mp3");
    SoundPlayer::Play(Sound);
}

void Gui::SetMacroEnabled(const std::string& macroId, bool enabled)
{
    MacroManager::Get().SetEnabled(macroId, enabled);
    PlayToggleChime(enabled);
}

void Gui::DrawStatusBar()
{
    if (StatusTimer > 0.0f)
        ImGui::TextColored({ 0.4f,0.9f,0.5f,1 }, "  %s", StatusMessage.c_str());
    else
    {
        size_t tot = MacroManager::Get().GetMacros().size(), en = 0;

        for (auto& Macro : MacroManager::Get().GetMacros()) 
        {
            if (Macro.Enabled)
                ++en;
        }

        ImGui::TextDisabled(Gui::Version.c_str());
        ImGui::SameLine(0, 2);
		ImGui::TextDisabled("  -  ");
		ImGui::SameLine(0, 2);
        ImGui::TextDisabled("  %zu macro(s)  |  %zu enabled", tot, en);
    }
}

void Gui::DrawUpdateDialog()
{
    UpdateState State = Updater::Get().State();

    if (ShowUpdateDialog)
    {
        ImGui::OpenPopup("Update Notifier");
        ShowUpdateDialog = false;
    }

    if (ImGui::BeginPopupModal("Update Notifier", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const UpdateInfo& Info = Updater::Get().PendingUpdate();

        switch (State)
        {
        case UpdateState::UpdateAvailable:
        {
            ImGui::Text("A new version of Macro Manager is available!");
            ImGui::Spacing();
            ImGui::Text("Installed version:");
            ImGui::SameLine();
            ImGui::TextColored({ 0.7f,0.7f,0.7f,1 }, APP_VERSION_STRING);
            ImGui::Text("Latest version:    ");
            ImGui::SameLine();
            ImGui::TextColored({ 0.4f,0.9f,0.5f,1 }, "%s", Info.LatestVersion.c_str());

            if (!Info.ReleaseNotes.empty())
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextDisabled("What's new:");
                ImGui::BeginChild("##relnotes", { 420, 120 }, true);
                ImGui::TextWrapped("%s", Info.ReleaseNotes.c_str());
                ImGui::EndChild();
            }

            ImGui::Spacing();

            if (ImGui::Button("Update Now", { 140,0 }))
                Updater::Get().BeginDownloadAndInstallAsync();

            ImGui::SameLine();

            if (ImGui::Button("Remind Me Later", { 140,0 }))
            {
                Updater::Get().RemindLater();
                ImGui::CloseCurrentPopup();
            }

            break;
        }

        case UpdateState::Downloading:
        {
            ImGui::Text("Downloading the update...");
            ImGui::Spacing();
            float Progress = Updater::Get().DownloadProgress();

            if (Progress >= 0.0f)
                ImGui::ProgressBar(Progress, { 320, 0 });
            else
                ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), { 320, 0 }, "Downloading...");

            ImGui::Spacing();
            ImGui::TextDisabled("This won't take long.");
            break;
        }

        case UpdateState::ReadyToInstall:
        case UpdateState::Launching:
        {
            ImGui::Text("Starting the installer...");
            ImGui::Spacing();
            ImGui::TextDisabled("Macro Manager will close in a moment so setup can finish.");
            break;
        }

        case UpdateState::Failed:
        {
            ImGui::TextColored({ 1.0f,0.4f,0.4f,1 }, "The update could not be completed.");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", Updater::Get().LastError().c_str());
            ImGui::Spacing();

            if (ImGui::Button("Try Again", { 120,0 }))
                Updater::Get().CheckForUpdatesAsync();

            ImGui::SameLine();

            if (ImGui::Button("Close", { 120,0 }))
                ImGui::CloseCurrentPopup();

            break;
        }

        default:
        {
            ImGui::Text("You're already on the latest version.");
            ImGui::Spacing();

            if (ImGui::Button("Close", { 120,0 }))
                ImGui::CloseCurrentPopup();

            break;
        }
        }

        ImGui::EndPopup();
    }
}

void Gui::DrawMacroList()
{
    ImGui::Text("Macros"); ImGui::Separator(); ImGui::Spacing();

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float RowH = ImGui::GetFrameHeight();

    for (auto& Macro : MacroManager::Get().GetMacros())
    {
        ImGui::PushID(Macro.ID.c_str());

        const ImVec2 RowMin = ImGui::GetCursorScreenPos();
        const float  RowW = ImGui::GetContentRegionAvail().x;
        const ImVec2 RowMax = { RowMin.x + RowW, RowMin.y + RowH };

        const bool IsSelected = (SelectedMacroId == Macro.ID);
        const bool Running = MacroManager::Get().IsRunning(Macro.ID);

        bool Enabled = Macro.Enabled;
        if (ImGui::Checkbox("##en", &Enabled))
            SetMacroEnabled(Macro.ID, Enabled);

        ImGui::SameLine(0, 6);

        const ImVec2 RestPos = ImGui::GetCursorScreenPos();
        const float  RestW = RowMax.x - RestPos.x;

        if (ImGui::InvisibleButton("##sel", { RestW, RowH }))
        {
            SelectedMacroId = Macro.ID; EditActionIdx = -1;
        }

        const bool Hovered = ImGui::IsItemHovered();

        const float SelAnim = AnimTo(ImGui::GetID("sel"), IsSelected ? 1.0f : 0.0f, 14.0f);
        const float HovAnim = AnimTo(ImGui::GetID("hov"), Hovered ? 1.0f : 0.0f, 16.0f);

        const ImVec2 HlMin = { RestPos.x, RowMin.y };

        if (SelAnim > 0.001f)
            DrawList->AddRectFilled(HlMin, RowMax, ImGui::GetColorU32(ImVec4(0.28f, 0.42f, 0.85f, SelAnim * 0.38f)), 5.0f);

        if (HovAnim > 0.001f)
            DrawList->AddRectFilled(HlMin, RowMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, HovAnim * 0.05f)), 5.0f);

        if (SelAnim > 0.001f)
            DrawList->AddRectFilled(HlMin, { HlMin.x + 3.0f, RowMax.y }, ImGui::GetColorU32(ImVec4(0.45f, 0.65f, 1.0f, SelAnim)), 2.0f);

        if (Running)
            DrawList->AddCircleFilled({ RestPos.x + 4.0f, RowMin.y + RowH * 0.5f }, 4.0f, IM_COL32(80, 230, 120, 255));

        const float TextY = RowMin.y + (RowH - ImGui::GetTextLineHeight()) * 0.5f;
        DrawList->AddText({ RestPos.x + 14.0f, TextY }, ImGui::GetColorU32(ImGuiCol_Text), Macro.Name.c_str());

        if (Macro.TriggerKey != 0)
        {
            std::string KeyText = "[" + VKCodeToName(Macro.TriggerKey) + "]";
            float KeyW = ImGui::CalcTextSize(KeyText.c_str()).x;
            DrawList->AddText({ RowMax.x - KeyW - 4.0f, TextY }, ImGui::GetColorU32(ImGuiCol_TextDisabled), KeyText.c_str());
        }

        ImGui::PopID();
    }
}

void Gui::DrawMacroEditor(const std::string& MacroId)
{
    Macro* Macro = MacroManager::Get().FindMacro(MacroId);

    if (!Macro) 
    { 
        SelectedMacroId.clear(); 
        return; 
    }

    ImGui::Text("Edit Macro"); 
    ImGui::Separator(); 
    ImGui::Spacing();

    char nb[128]; 
    strncpy_s(nb, Macro->Name.c_str(), sizeof(nb) - 1);

    ImGui::SetNextItemWidth(280);
    if (ImGui::InputText("Name", nb, sizeof(nb))) Macro->Name = nb;

    ImGui::Spacing(); 
    ImGui::Text("Hotkey:"); 
    ImGui::SameLine();

    if (CapturingKey && CapturingMacroId == MacroId)
        ImGui::TextColored({ 1,0.8f,0.2f,1 }, "Press any key...");
    else
    {
        if (ImGui::Button(VKCodeToName(Macro->TriggerKey).c_str(), { Macro->TriggerKey != 0 ? 40.0f : 120.0f, 0 }))
        {
            CapturingKey = true; 
            CapturingMacroId = MacroId;
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear##key")) 
        { 
            Macro->TriggerKey = 0;
            MacroManager::Get().RebindAll(); 
        }
    }

    ImGui::Spacing();
    bool Enabled = Macro->Enabled;

    if (ImGui::Checkbox("Enabled", &Enabled))
        SetMacroEnabled(MacroId, Enabled);

    ImGui::SameLine(0, 24); 
    ImGui::Checkbox("Repeat", &Macro->Repeat);

    if (Macro->Repeat)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130);
        ImGui::InputInt("Times", &Macro->RepeatCount);

        if (Macro->RepeatCount < 0) 
            Macro->RepeatCount = 0;

        ImGui::SameLine(); 
        ImGui::TextDisabled("(0 = infinite)");
    }

    ImGui::Spacing();

    if (ImGui::Button("Run Now", { 110,0 }))
    {
        MacroManager::Get().ClearStop();
        MacroManager::Get().Execute(MacroId);
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f,0.15f,0.15f,1 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f,0.20f,0.20f,1 });

    if (ImGui::Button("Delete Macro", { 110,0 }))
    {
        MacroManager::Get().RemoveMacro(MacroId);
        SelectedMacroId.clear();
        ImGui::PopStyleColor(2); return;
    }

    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float PanelW = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("##ActionsPanel", { PanelW, 0.0f }, true);

    ImGui::Text("Actions (%zu)", Macro->Actions.size());
    ImGui::Spacing();

    if (ImGui::Button("+ Add Action"))
    {
        Macro->Actions.push_back(MacroAction{});
        EditActionIdx = static_cast<int>(Macro->Actions.size()) - 1;
        ShowActionEditor = true;
    }

    if (!Macro->Actions.empty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f,0.15f,0.15f,1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f,0.20f,0.20f,1 });

        if (ImGui::Button("Clear All Actions"))
            ConfirmClearActions = true;

        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float ROW_HEIGHT = ImGui::GetTextLineHeight() + 2.0f;
    const float ROW_STRIDE = ImGui::GetTextLineHeightWithSpacing();

    const float Fp2 = ImGui::GetStyle().FramePadding.x * 2.0f;
    const float BtnCluster = ImGui::CalcTextSize(" ^ ").x + Fp2 + ImGui::CalcTextSize(" v ").x + Fp2 + ImGui::CalcTextSize("Edit").x + Fp2 + ImGui::CalcTextSize(" X ").x + Fp2 + 3.0f * 3.0f;

    bool DragFloatActive = false;
    std::string DragFloatLabel;
    float DragFloatX = 0.0f, DragFloatW = 0.0f;
    float ListTopY = 0.0f, ListBottomY = 0.0f;

    for (int i = 0; i < (int)Macro->Actions.size(); ++i)
    {
        auto& Actions = Macro->Actions[i];
        ImGui::PushID(i);

        std::string Label = std::to_string(i + 1) + ".  ";

        switch (Actions.Type)
        {
        case ActionType::KeyPress:
        {
            Label += "Key Press  [" + VKCodeToName(Actions.KeyCode) + "]";
            break;
        }
        case ActionType::KeyDown:
        {
            Label += "Key Down   [" + VKCodeToName(Actions.KeyCode) + "]";
            break;
        }
        case ActionType::KeyUp:
        {
            Label += "Key Up     [" + VKCodeToName(Actions.KeyCode) + "]";
            break;
        }
        case ActionType::MouseMove:   
        {
            Label += "Mouse Move  (" + std::to_string(Actions.MouseX) + ", " + std::to_string(Actions.MouseY) + ")";
            break;
        }
        case ActionType::MouseMoveRel:
        {
            Label += "Mouse Move Rel  (+" + std::to_string(Actions.MouseX) + ", +" + std::to_string(Actions.MouseY) + ")";
            break;
        }
        case ActionType::MouseClick:
        {
            Label += "Mouse Click  [" + std::string(MouseButtonName(Actions.MouseButton)) + "]";
            break;
        }
        case ActionType::MouseDown:
        {
            Label += "Mouse Down   [" + std::string(MouseButtonName(Actions.MouseButton)) + "]";
            break;
        }
        case ActionType::MouseUp:
        {
            Label += "Mouse Up     [" + std::string(MouseButtonName(Actions.MouseButton)) + "]";
            break;
        }
        case ActionType::MouseScroll: 
        {
            Label += std::string("Mouse Scroll ") + (Actions.ScrollDelta >= 0 ? "Up" : "Down") + "  (" + std::to_string(std::abs(Actions.ScrollDelta)) + ")";
            break;
        }
        case ActionType::Delay:       
        {
            Label += "Delay  " + std::to_string(Actions.MsDelay) + " ms";
            break;
        }
        default:
        {
            Label += ActionTypeName(Actions.Type);
            break;
        }
        }

        const float BtnOffset = ImGui::GetContentRegionAvail().x - BtnCluster;

        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = { rowMin.x + BtnOffset - 8.0f, rowMin.y + ROW_HEIGHT };

        if (i == 0)
            ListTopY = rowMin.y;

        ListBottomY = rowMin.y;

        bool Hovered = ImGui::IsMouseHoveringRect(rowMin, rowMax) && ImGui::IsWindowHovered();
        bool IsDragging = (DragIdx == i);

        float RowHovAnim = AnimTo(ImGui::GetID("rowhov"), (Hovered && !IsDragging) ? 1.0f : 0.0f, 16.0f);

        ImDrawList* RowDraw = ImGui::GetWindowDrawList();

        if (IsDragging)
            RowDraw->AddRect(rowMin, rowMax, IM_COL32(90, 150, 255, 130), 4.0f, 0, 1.5f);

        else if (RowHovAnim > 0.001f)
            RowDraw->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, RowHovAnim * 0.08f)), 4.0f);

        if (IsDragging)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 0.30f));
            ImGui::TextUnformatted(Label.c_str());
            ImGui::PopStyleColor();

            DragFloatActive = true;
            DragFloatLabel = Label;
            DragFloatX = rowMin.x;
            DragFloatW = BtnOffset - 8.0f;
        }
        else
        {
            ImGui::TextUnformatted(Label.c_str());
        }

        if (Hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            EditActionIdx = i;
            ShowActionEditor = true;
        }

        if (Hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            DragIdx = i;
            DragStartY = ImGui::GetIO().MousePos.y;
            DragOffsetY = ImGui::GetIO().MousePos.y - rowMin.y;
        }

        ImGui::SameLine(BtnOffset);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 3.0f, 2.0f });

        bool DoUp = false, DoDown = false, DoEdit = false, DoDel = false;
        DoUp = ImGui::SmallButton(" ^ ") && i > 0;
        ImGui::SameLine();
        DoDown = ImGui::SmallButton(" v ") && i < (int)Macro->Actions.size() - 1;
        ImGui::SameLine();

        if (ImGui::SmallButton("Edit")) 
            DoEdit = true;

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, { 0.55f,0.15f,0.15f,1 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.75f,0.20f,0.20f,1 });
        DoDel = ImGui::SmallButton(" X ");
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        if (DoUp) 
        {
            std::swap(Macro->Actions[i], Macro->Actions[i - 1]);

            if (EditActionIdx == i)
                EditActionIdx = i - 1;

            else if (EditActionIdx == i - 1)
                EditActionIdx = i;
        }

        if (DoDown)
        {
            std::swap(Macro->Actions[i], Macro->Actions[i + 1]);

            if (EditActionIdx == i) EditActionIdx = i + 1;

            else if (EditActionIdx == i + 1)
                EditActionIdx = i;
        }

        if (DoEdit) 
        {
            EditActionIdx = i; 
            ShowActionEditor = true; 
        }

        if (DoDel)
        {
            Macro->Actions.erase(Macro->Actions.begin() + i);

            if (EditActionIdx == i)
                EditActionIdx = -1;

            else if (EditActionIdx > i)
                EditActionIdx--;

            if (DragIdx == i)
                DragIdx = -1;

            else if (DragIdx > i)
                DragIdx--;
            
            ImGui::PopID(); break;
        }

        ImGui::PopID();
    }

    if (DragIdx >= 0 && DragIdx < (int)Macro->Actions.size())
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float MouseY = ImGui::GetIO().MousePos.y;
            float deltaY = MouseY - DragStartY;
            int Steps = static_cast<int>(deltaY / ROW_STRIDE);

            if (Steps != 0)
            {
                int target = DragIdx + Steps;
                target = (target < 0) ? 0 : (target >= (int)Macro->Actions.size() ? (int)Macro->Actions.size() - 1 : target);

                while (DragIdx < target)
                {
                    std::swap(Macro->Actions[DragIdx], Macro->Actions[DragIdx + 1]);

                    if (EditActionIdx == DragIdx)     
                        EditActionIdx = DragIdx + 1;

                    else if (EditActionIdx == DragIdx + 1) 
                        EditActionIdx = DragIdx;

                    DragIdx++;
                }

                while (DragIdx > target)
                {
                    std::swap(Macro->Actions[DragIdx], Macro->Actions[DragIdx - 1]);

                    if (EditActionIdx == DragIdx)
                        EditActionIdx = DragIdx - 1;

                    else if (EditActionIdx == DragIdx - 1) 
                        EditActionIdx = DragIdx;

                    DragIdx--;
                }

                DragStartY = MouseY;
            }
        }
        else
        {
            DragIdx = -1;
        }
    }

    if (DragIdx >= 0 && DragFloatActive)
    {
        float FloatY = ImGui::GetIO().MousePos.y - DragOffsetY;
        FloatY = (FloatY < ListTopY) ? ListTopY : (FloatY > ListBottomY ? ListBottomY : FloatY);

        ImVec2 fMin = { DragFloatX - 4.0f, FloatY - 2.0f };
        ImVec2 fMax = { DragFloatX + DragFloatW + 4.0f, FloatY + ROW_HEIGHT + 2.0f };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(fMin, fMax, IM_COL32(38, 66, 130, 235), 5.0f);
        dl->AddRect(fMin, fMax, IM_COL32(90, 150, 255, 255), 5.0f, 0, 1.5f);
        dl->AddText({ DragFloatX, FloatY }, IM_COL32(225, 238, 255, 255), DragFloatLabel.c_str());
    }

    ImGui::EndChild();
}

void Gui::DrawActionEditor(const std::string& macroId, int idx)
{
    Macro* Macro = MacroManager::Get().FindMacro(macroId);

    if (!Macro || idx < 0 || idx >= (int)Macro->Actions.size()) 
        return;

    MacroAction& Action = Macro->Actions[idx];

    ImGui::Text("Action %d of %zu", idx + 1, Macro->Actions.size());
    ImGui::Separator();
    ImGui::Spacing();

    const char* tn[] = 
    { 
        "Key Press","Key Down","Key Up",
        "Mouse Move (Abs)","Mouse Move (Rel)",
        "Mouse Click","Mouse Down","Mouse Up",
        "Mouse Scroll","Delay" 
    };

    int ti = static_cast<int>(Action.Type);

    ImGui::SetNextItemWidth(200);

    if (ImGui::Combo("Type", &ti, tn, IM_ARRAYSIZE(tn)))
    {
        Action.Type = static_cast<ActionType>(ti); CapturingActionKey = false;
    }

    ImGui::Spacing();

    bool IsKey = (Action.Type == ActionType::KeyPress || Action.Type == ActionType::KeyDown || Action.Type == ActionType::KeyUp);

    if (IsKey)
    {
        ImGui::Text("Key:"); ImGui::SameLine();

        if (CapturingActionKey)
        {
            ImGui::TextColored({ 1,0.8f,0.2f,1 }, "Press any key...");
            for (int vk = 8; vk < 256; ++vk)
            {
                if (GetAsyncKeyState(vk) & 0x8000)
                {
                    Action.KeyCode = vk; CapturingActionKey = false;
                    break;
                }
            }
        }
        else if (ImGui::Button(VKCodeToName(Action.KeyCode).c_str(), { 140,0 }))
            CapturingActionKey = true;
    }

    bool HasPos = (Action.Type == ActionType::MouseMove || Action.Type == ActionType::MouseMoveRel);

    if (HasPos)
    {
        ImGui::SetNextItemWidth(100); ImGui::InputInt("X", &Action.MouseX);
        ImGui::SetNextItemWidth(100); ImGui::InputInt("Y", &Action.MouseY);

        if (Action.Type == ActionType::MouseMoveRel)
            ImGui::TextDisabled("Relative offset from current position.");
    }

    bool HasButton = (Action.Type == ActionType::MouseClick || Action.Type == ActionType::MouseDown || Action.Type == ActionType::MouseUp);

    if (HasButton)
    {
        const char* bn[] = { "Left","Right","Middle" };
        int ButtonInput = static_cast<int>(Action.MouseButton);
        ImGui::SetNextItemWidth(140);

        if (ImGui::Combo("Button", &ButtonInput, bn, 3))
            Action.MouseButton = static_cast<MouseButton>(ButtonInput);
    }

    if (Action.Type == ActionType::MouseScroll)
    {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Scroll Delta", &Action.ScrollDelta);
        ImGui::TextDisabled("+ = up    - = down    120 = one notch");
    }

    if (Action.Type == ActionType::Delay)
    {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Delay (ms)", &Action.MsDelay);

        if (Action.MsDelay < 0)
            Action.MsDelay = 0;
    }
}

void Gui::Shutdown()
{
    Persistence::Save(MacroManager::Get().GetMacros(), Persistence::DefaultFilePath());

    if (IconTexture)
    {
        IconTexture->Release();
        IconTexture = nullptr;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();

    if (Window)
        DestroyWindow(Window);
}

bool Gui::CreateAppWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); 
    wc.style = CS_CLASSDC; 
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance; 
    wc.lpszClassName = L"Macro ManagerWnd";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    Window = CreateWindowExW(0, L"Macro ManagerWnd", L"Macro Manager", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 770, nullptr, nullptr, hInstance, this);

    if (!Window) 
        return false;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(Window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    return true;
}

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) 
        return true;

    static Gui* self = nullptr;

    if (msg == WM_CREATE) 
    { 
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam); 
        self = static_cast<Gui*>(cs->lpCreateParams);
    }

    switch (msg)
    {
    case WM_SIZE:
    {
        if (self && self->SwapChain && wParam != SIZE_MINIMIZED)
        {
            self->CleanupRenderTarget();
            self->SwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            self->CreateRenderTarget();
        }

        return 0;
    }
    case WM_SYSCOMMAND:
    {
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0; 
        
        break;
    }
    case WM_DESTROY: 
    {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool Gui::CreateDeviceD3D()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; 
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; 
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; 
    sd.OutputWindow = Window;
    sd.SampleDesc.Count = 1; 
    sd.Windowed = TRUE; 
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL lvl; 
    const D3D_FEATURE_LEVEL lvls[] = { D3D_FEATURE_LEVEL_11_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, lvls, 1, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, &lvl, &Context))) 
        return false;

    CreateRenderTarget(); return true;
}

void Gui::CleanupDeviceD3D()
{
    CleanupRenderTarget();

    if (SwapChain) 
    { 
        SwapChain->Release(); 
        SwapChain = nullptr; 
    }

    if (Context) 
    { 
        Context->Release();  
        Context = nullptr; 
    }

    if (Device) 
    { 
        Device->Release();
        Device = nullptr;
    }
}

void Gui::CreateRenderTarget()
{
    ID3D11Texture2D* buf = nullptr;
    SwapChain->GetBuffer(0, IID_PPV_ARGS(&buf));

    if (buf) 
    { 
        Device->CreateRenderTargetView(buf, nullptr, &MainRenderTargetView); 
        buf->Release(); 
    }
}

void Gui::CleanupRenderTarget()
{
    if (MainRenderTargetView)
    {
        MainRenderTargetView->Release();
        MainRenderTargetView = nullptr;
    }
}

bool Gui::LoadIconTexture()
{
    IWICImagingFactory* Factory = nullptr;

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Factory))))
        return false;

    bool Ok = false;
    IWICStream* Stream = nullptr;
    IWICBitmapDecoder* Decoder = nullptr;
    IWICBitmapFrameDecode* Frame = nullptr;
    IWICFormatConverter* Converter = nullptr;

    if (SUCCEEDED(Factory->CreateStream(&Stream)) &&
        SUCCEEDED(Stream->InitializeFromMemory(const_cast<BYTE*>(AppIconPng), AppIconPngLen)) &&
        SUCCEEDED(Factory->CreateDecoderFromStream(Stream, nullptr, WICDecodeMetadataCacheOnDemand, &Decoder)) &&
        SUCCEEDED(Decoder->GetFrame(0, &Frame)) &&
        SUCCEEDED(Factory->CreateFormatConverter(&Converter)) &&
        SUCCEEDED(Converter->Initialize(Frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        UINT W = 0, H = 0;
        Converter->GetSize(&W, &H);

        std::vector<BYTE> Pixels(static_cast<size_t>(W) * H * 4);

        if (SUCCEEDED(Converter->CopyPixels(nullptr, W * 4, static_cast<UINT>(Pixels.size()), Pixels.data())))
        {
            D3D11_TEXTURE2D_DESC Desc = {};
            Desc.Width = W;
            Desc.Height = H;
            Desc.MipLevels = 1;
            Desc.ArraySize = 1;
            Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            Desc.SampleDesc.Count = 1;
            Desc.Usage = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA Init = {};
            Init.pSysMem = Pixels.data();
            Init.SysMemPitch = W * 4;

            ID3D11Texture2D* Tex = nullptr;
            if (SUCCEEDED(Device->CreateTexture2D(&Desc, &Init, &Tex)) && Tex)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
                SrvDesc.Format = Desc.Format;
                SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SrvDesc.Texture2D.MipLevels = 1;

                if (SUCCEEDED(Device->CreateShaderResourceView(Tex, &SrvDesc, &IconTexture)))
                {
                    IconWidth = static_cast<int>(W);
                    IconHeight = static_cast<int>(H);
                    Ok = true;
                }

                Tex->Release();
            }
        }
    }

    if (Converter) 
        Converter->Release();
    if (Frame)
        Frame->Release();
    if (Decoder)
        Decoder->Release();
    if (Stream)
        Stream->Release();

    Factory->Release();
    return Ok;
}