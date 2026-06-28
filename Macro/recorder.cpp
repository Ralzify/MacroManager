#include "recorder.h"

#include <algorithm>

Recorder& Recorder::Get()
{
    static Recorder instance;
    return instance;
}

void Recorder::Start(const RecorderOptions& Option)
{
    StopHookThread();

    {
        std::lock_guard<std::mutex> Lock(Mutex);
        Actions.clear();
        Options = Option;
        LastEventTime = Clock::now();
        LastMouseSample = Clock::now();
        std::fill(std::begin(KeyDown), std::end(KeyDown), false);
    }

    Recording = true;

    HookThread = CreateThread(nullptr, 0, HookThreadProc, this, 0, &HookThreadId);

    for (int i = 0; i < 200 && HookThread; ++i)
    {
        if (KeyboardHook)
            break;

        Sleep(5);
    }
}

std::vector<MacroAction> Recorder::Stop()
{
    Recording = false;

    StopHookThread();

    std::lock_guard<std::mutex> Lock(Mutex);
    return std::move(Actions);
}

void Recorder::StopHookThread()
{
    if (HookThread)
    {
        PostThreadMessageW(HookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(HookThread, 3000);
        CloseHandle(HookThread);
        HookThread = nullptr;
        HookThreadId = 0;
    }

    KeyboardHook = nullptr;
    MouseHook = nullptr;
}

DWORD WINAPI Recorder::HookThreadProc(LPVOID lpParam)
{
    Recorder* Self = static_cast<Recorder*>(lpParam);

    Self->KeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(nullptr), 0);

    bool WantMouse = false;
    {
        std::lock_guard<std::mutex> Lock(Self->Mutex);
        WantMouse = Self->Options.RecordMouseMove || Self->Options.RecordMouseClick;
    }

    if (WantMouse)
        Self->MouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (Self->KeyboardHook) { UnhookWindowsHookEx(Self->KeyboardHook); Self->KeyboardHook = nullptr; }
    if (Self->MouseHook) { UnhookWindowsHookEx(Self->MouseHook);    Self->MouseHook = nullptr; }

    return 0;
}

void Recorder::PushDelay()
{
    auto Now = Clock::now();
    int Elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastEventTime).count());
    LastEventTime = Now;

    if (!Options.RecordDelays || Elapsed < Options.MinMsDelay)
        return;

    if (Actions.empty())
        return;

    if (Actions.back().Type == ActionType::Delay)
    {
        Actions.back().MsDelay += Elapsed;
        return;
    }

    MacroAction Delay;
    Delay.Type = ActionType::Delay;
    Delay.MsDelay = Elapsed;
    Actions.push_back(Delay);
}

bool Recorder::OnKey(int vk, bool IsDown)
{
    if (!Recording)
        return false;

    if (Options.ToggleKey != 0 && vk == Options.ToggleKey) return false;

    if (!Options.RecordKeyPress)
        return false;

    if (vk <= 0 || vk >= 256)
        return false;

    std::lock_guard<std::mutex> lock(Mutex);
    PushDelay();

    if (IsDown && !KeyDown[vk])
    {
        KeyDown[vk] = true;
        MacroAction Action;
        Action.Type = ActionType::KeyDown;
        Action.KeyCode = vk;
        Actions.push_back(Action);
    }
    else if (!IsDown && KeyDown[vk])
    {
        KeyDown[vk] = false;

        if (!Actions.empty() && Actions.back().Type == ActionType::KeyDown && Actions.back().KeyCode == vk)
        {
            Actions.back().Type = ActionType::KeyPress;
        }
        else
        {
            MacroAction Action;
            Action.Type = ActionType::KeyUp;
            Action.KeyCode = vk;
            Actions.push_back(Action);
        }
    }

    return false;
}

bool Recorder::OnMouse(UINT Message, int x, int y, int WheelDelta)
{
    if (!Recording)
        return false;

    std::lock_guard<std::mutex> Lock(Mutex);

    if (Message == WM_MOUSEMOVE && Options.RecordMouseMove)
    {
        auto Now = Clock::now();
        int Elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastMouseSample).count());

        if (Elapsed < Options.MouseMoveInterval)
            return false;

        LastMouseSample = Now;

        PushDelay();
        MacroAction Action;
        Action.Type = ActionType::MouseMove;
        Action.MouseX = x;
        Action.MouseY = y;
        Actions.push_back(Action);
        return false;
    }

    if (Message == WM_MOUSEWHEEL && Options.RecordMouseClick)
    {
        PushDelay();
        MacroAction Action;
        Action.Type = ActionType::MouseScroll;
        Action.ScrollDelta = WheelDelta;
        Actions.push_back(Action);
        return false;
    }

    if (!Options.RecordMouseClick)
        return false;

    MouseButton Button = MouseButton::Left;
    bool IsDown = false;

    switch (Message)
    {
    case WM_LBUTTONDOWN:
    {
        Button = MouseButton::Left;
        IsDown = true;
        break;
    }
    case WM_LBUTTONUP:
    {
        Button = MouseButton::Left;
        IsDown = false;
        break;
    }
    case WM_RBUTTONDOWN:
    {
        Button = MouseButton::Right;
        IsDown = true;
        break;
    }
    case WM_RBUTTONUP:
    {
        Button = MouseButton::Right;
        IsDown = false;
        break;
    }
    case WM_MBUTTONDOWN:
    {
        Button = MouseButton::Middle;
        IsDown = true;
        break;
    }
    case WM_MBUTTONUP:
    {
        Button = MouseButton::Middle;
        IsDown = false;
        break;
    }
    default:
        return false;
    }

    PushDelay();

    if (!IsDown && !Actions.empty() && Actions.back().Type == ActionType::MouseDown && Actions.back().MouseButton == Button)
    {
        Actions.back().Type = ActionType::MouseClick;
    }
    else
    {
        MacroAction Action;
        Action.Type = IsDown ? ActionType::MouseDown : ActionType::MouseUp;
        Action.MouseButton = Button;
        Action.MouseX = x;
        Action.MouseY = y;
        Actions.push_back(Action);
    }

    return false;
}

LRESULT CALLBACK Recorder::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* Keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool IsDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        Recorder::Get().OnKey(static_cast<int>(Keyboard->vkCode), IsDown);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Recorder::MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* Ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        int WheelDelta = (wParam == WM_MOUSEWHEEL) ? static_cast<int>(static_cast<short>(HIWORD(Ms->mouseData))) : 0;
        Recorder::Get().OnMouse(static_cast<UINT>(wParam), Ms->pt.x, Ms->pt.y, WheelDelta);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}