#include <windows.h>
#include <objbase.h>
#include "gui.h"
#include "hook_manager.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HANDLE AppMutexHandle = CreateMutexW(nullptr, FALSE, L"MacroManagerSingleInstanceMutex");

    if (!HookManager::Get().Install())
    {
        MessageBoxW(nullptr, L"Failed to install keyboard hook.\n" L"The app may not have sufficient permissions.", L"Macro Manager - Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    Gui GUI;

    if (!GUI.Init(hInstance))
    {
        MessageBoxW(nullptr, L"Failed to initialise the application window.", L"Macro Manager - Error", MB_ICONERROR | MB_OK);
        HookManager::Get().Uninstall();
        CoUninitialize();
        return 1;
    }

    GUI.Run();
    GUI.Shutdown();

    HookManager::Get().Uninstall();

    if (AppMutexHandle)
        CloseHandle(AppMutexHandle);

    CoUninitialize();
    return 0;
}