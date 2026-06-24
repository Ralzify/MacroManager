#include "input_sim.h"
#include "macro_manager.h"
#include <thread>
#include <chrono>
#include <windows.h>
#include <timeapi.h>

void InputSim::KeyDown(int vk)
{
    INPUT Input = {};
    Input.type = INPUT_KEYBOARD;
    Input.ki.wVk = static_cast<WORD>(vk);
    Input.ki.dwFlags = 0;
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::KeyUp(int vk)
{
    INPUT Input = {};
    Input.type = INPUT_KEYBOARD;
    Input.ki.wVk = static_cast<WORD>(vk);
    Input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::KeyPress(int vk)
{
    KeyDown(vk);
    KeyUp(vk);
}

DWORD InputSim::MouseButtonDownFlag(MouseButton Button)
{
    switch (Button)
    {
    case MouseButton::Left:
        return MOUSEEVENTF_LEFTDOWN;
    case MouseButton::Right:
        return MOUSEEVENTF_RIGHTDOWN;
    case MouseButton::Middle:
        return MOUSEEVENTF_MIDDLEDOWN;
    default:
        return MOUSEEVENTF_LEFTDOWN;
    }
}

DWORD InputSim::MouseButtonUpFlag(MouseButton Button)
{
    switch (Button)
    {
    case MouseButton::Left:
        return MOUSEEVENTF_LEFTUP;
    case MouseButton::Right:
        return MOUSEEVENTF_RIGHTUP;
    case MouseButton::Middle:
        return MOUSEEVENTF_MIDDLEUP;
    default:
        return MOUSEEVENTF_LEFTUP;
    }
}

DWORD InputSim::MouseButtonDataValue(MouseButton Button)
{
    return (Button == MouseButton::Middle) ? XBUTTON1 : 0;
}

void InputSim::MouseMoveTo(int x, int y)
{
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    INPUT Input = {};
    Input.type = INPUT_MOUSE;
    Input.mi.dx = static_cast<LONG>((x * 65535) / screenW);
    Input.mi.dy = static_cast<LONG>((y * 65535) / screenH);
    Input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::MouseMoveBy(int dx, int dy)
{
    INPUT Input = {};
    Input.type = INPUT_MOUSE;
    Input.mi.dx = static_cast<LONG>(dx);
    Input.mi.dy = static_cast<LONG>(dy);
    Input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::MouseButtonDown(MouseButton Button)
{
    INPUT Input = {};
    Input.type = INPUT_MOUSE;
    Input.mi.dwFlags = MouseButtonDownFlag(Button);
    Input.mi.mouseData = MouseButtonDataValue(Button);
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::MouseButtonUp(MouseButton Button)
{
    INPUT Input = {};
    Input.type = INPUT_MOUSE;
    Input.mi.dwFlags = MouseButtonUpFlag(Button);
    Input.mi.mouseData = MouseButtonDataValue(Button);
    SendInput(1, &Input, sizeof(INPUT));
}

void InputSim::MouseClick(MouseButton Button)
{
    MouseButtonDown(Button);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    MouseButtonUp(Button);
}

void InputSim::PlayMacro(const std::vector<MacroAction>& Actions)
{
    for (const auto& Action : Actions)
    {
        if (MacroManager::ShouldStop()) 
            return;

        switch (Action.Type)
        {
        case ActionType::KeyPress:
        {
            KeyPress(Action.KeyCode);
            break;
        }
        case ActionType::KeyDown:
        {
            KeyDown(Action.KeyCode);
            break;
        }
        case ActionType::KeyUp:
        {
            KeyUp(Action.KeyCode);
            break;
        }
        case ActionType::MouseMove:
        {
            MouseMoveTo(Action.MouseX, Action.MouseY);
            break;
        }
        case ActionType::MouseMoveRel:
        {
            MouseMoveBy(Action.MouseX, Action.MouseY);
            break;
        }
        case ActionType::MouseClick:
        {
            MouseClick(Action.MouseButton);
            break;
        }
        case ActionType::MouseDown:
        {
            MouseButtonDown(Action.MouseButton);
            break;
        }
        case ActionType::MouseUp:
        {
            MouseButtonUp(Action.MouseButton);
            break;
        }
        case ActionType::MouseScroll:
        {
            INPUT Input = {};
            Input.type = INPUT_MOUSE;
            Input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            Input.mi.mouseData = static_cast<DWORD>(Action.ScrollDelta);
            SendInput(1, &Input, sizeof(INPUT));
            break;
        }

        case ActionType::Delay:
        {
            timeBeginPeriod(1);

            auto Deadline = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(Action.MsDelay);
            int Remaining = Action.MsDelay;

            while (Remaining > 2 && !MacroManager::ShouldStop())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                Remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Deadline - std::chrono::high_resolution_clock::now()).count());
            }

            while (!MacroManager::ShouldStop())
            {
                if (std::chrono::high_resolution_clock::now() >= Deadline) 
                    break;
            }

            timeEndPeriod(1);
            break;
        }
        }
    }
}