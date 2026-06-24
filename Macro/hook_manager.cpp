#include "hook_manager.h"

#include <stdexcept>

HookManager& HookManager::Get()
{
    static HookManager Instance;
    return Instance;
}

bool HookManager::Install()
{
    if (Hook) 
        return true;

    Hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    return Hook != nullptr;
}

void HookManager::Uninstall()
{
    if (Hook)
    {
        UnhookWindowsHookEx(Hook);
        Hook = nullptr;
    }
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
