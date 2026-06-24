#pragma once
#include <windows.h>
#include <vector>
#include "macro.h"

class InputSim
{
public:
    static void KeyPress(int vk);
    static void KeyDown(int vk);
    static void KeyUp(int vk);

    static void MouseMoveTo(int x, int y);
    static void MouseMoveBy(int dx, int dy);
    static void MouseClick(MouseButton Button);
    static void MouseButtonDown(MouseButton Button);
    static void MouseButtonUp(MouseButton Button);

    static void PlayMacro(const std::vector<MacroAction>& actions);

private:
    static DWORD MouseButtonDownFlag(MouseButton btn);
    static DWORD MouseButtonUpFlag(MouseButton btn);
    static DWORD MouseButtonDataValue(MouseButton btn);
};
