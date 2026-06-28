#pragma once

#include <windows.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "macro.h"

struct RecorderOptions
{
    bool RecordKeyPress = true;
    bool RecordMouseMove = false;
    bool RecordMouseClick = true;
    bool RecordDelays = true;
    int  MinMsDelay = 5;
    int  MouseMoveInterval = 50;
    int  ToggleKey = 0;

    int  MacroToggleKey = VK_F3;
    bool MacroToggleChime = true;
};

class Recorder
{
public:
    static Recorder& Get();

    void Start(const RecorderOptions& Options);

    std::vector<MacroAction> Stop();

    bool IsRecording() const { return Recording.load(); }

    bool OnKey(int vk, bool IsDown);
    bool OnMouse(UINT Message, int x, int y, int WheelDelta);

private:
    Recorder() = default;
    ~Recorder() { StopHookThread(); }

    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    static DWORD WINAPI HookThreadProc(LPVOID lpParam);
    void StopHookThread();

    void PushDelay();

    HHOOK KeyboardHook = nullptr;
    HHOOK MouseHook = nullptr;

    HANDLE HookThread = nullptr;
    DWORD  HookThreadId = 0;

    std::atomic<bool> Recording{ false };
    RecorderOptions Options;
    std::mutex Mutex;
    std::vector<MacroAction> Actions;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint LastEventTime;
    TimePoint LastMouseSample;

    bool KeyDown[256] = {};
};