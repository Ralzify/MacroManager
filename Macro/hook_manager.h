#pragma once
#include <windows.h>
#include <functional>
#include <unordered_map>
#include <mutex>

using KeyCallback = std::function<bool(int vkCode, bool isKeyDown)>;

class HookManager
{
public:
    static HookManager& Get();

    bool Install();
    void Uninstall();

    void RegisterCallback(const std::string& id, int vkCode, KeyCallback cb);
    void UnregisterCallback(const std::string& id);

    bool DispatchKeyEvent(int vkCode, bool isKeyDown);

private:
    HookManager() = default;
    ~HookManager() { Uninstall(); }

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static DWORD WINAPI HookThreadProc(LPVOID lpParam);

    HANDLE HookThread = nullptr;
    DWORD  HookThreadId = 0;
    HHOOK  Hook = nullptr;

    struct Entry
    {
        int VKCode = 0;
        KeyCallback Callback;
    };

    std::mutex Mutex;
    std::unordered_map<std::string, Entry> Callbacks;
};