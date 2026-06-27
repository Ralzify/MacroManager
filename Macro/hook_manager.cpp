#include "hook_manager.h"

#include <stdexcept>

HookManager& HookManager::Get()
{
    static HookManager Instance;
    return Instance;
}

DWORD WINAPI HookManager::HookThreadProc(LPVOID lpParam)
{
    HookManager* Self = static_cast<HookManager*>(lpParam);

    Self->Hook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );

    if (!Self->Hook)
        return 1;

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (Self->Hook)
    {
        UnhookWindowsHookEx(Self->Hook);
        Self->Hook = nullptr;
    }

    return 0;
}

bool HookManager::Install()
{
    if (HookThread)
        return true;

    HookThread = CreateThread(nullptr, 0, HookThreadProc, this, 0, &HookThreadId);

    if (!HookThread)
        return false;

    for (int i = 0; i < 200; ++i)
    {
        if (Hook)
            return true;

        Sleep(10);
    }

    Uninstall();
    return false;
}

void HookManager::Uninstall()
{
    if (HookThread)
    {
        PostThreadMessageW(HookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(HookThread, 3000);
        CloseHandle(HookThread);
        HookThread = nullptr;
        HookThreadId = 0;
    }

    Hook = nullptr;
}

void HookManager::RegisterCallback(const std::string& id, int vkCode, KeyCallback cb)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Callbacks[id] = { vkCode, std::move(cb) };
}

void HookManager::UnregisterCallback(const std::string& id)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Callbacks.erase(id);
}

bool HookManager::DispatchKeyEvent(int VKCode, bool IsKeyDown)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    bool Consumed = false;

    for (auto it = Callbacks.begin(); it != Callbacks.end(); ++it)
    {
        Entry& Entry = it->second;

        if (Entry.VKCode == 0 || Entry.VKCode == VKCode)
        {
            if (Entry.Callback(VKCode, IsKeyDown))
                Consumed = true;
        }
    }

    return Consumed;
}

LRESULT CALLBACK HookManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* KBStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool IsKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool Consumed = HookManager::Get().DispatchKeyEvent(static_cast<int>(KBStruct->vkCode), IsKeyDown);

        if (Consumed)
            return 1;
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}