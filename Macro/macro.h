#pragma once
#include <string>
#include <vector>
#include <windows.h>

enum class ActionType
{
    KeyPress,
    KeyDown,
    KeyUp,
    MouseMove,
    MouseMoveRel,
    MouseClick,
    MouseDown,
    MouseUp,
    MouseScroll,
    Delay,
};

enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
};

struct MacroAction
{
    ActionType Type = ActionType::KeyPress;
    int KeyCode = 0;
    int MouseX = 0;
    int MouseY = 0;
    MouseButton MouseButton = MouseButton::Left;
    int ScrollDelta = 120;
    int MsDelay = 100;
};


struct Macro
{
    std::string ID;
    std::string Name;
    int TriggerKey = 0;
    bool Enabled = true;
    bool Repeat = false;
    int RepeatCount = 1;
    bool LockInputToApp = false;
    std::string LockedAppName;
    std::vector<MacroAction> Actions;
};

inline bool operator==(const MacroAction& A, const MacroAction& B)
{
    return A.Type == B.Type && A.KeyCode == B.KeyCode && A.MouseX == B.MouseX && A.MouseY == B.MouseY 
        && A.MouseButton == B.MouseButton && A.ScrollDelta == B.ScrollDelta && A.MsDelay == B.MsDelay;
}

inline bool operator!=(const MacroAction& A, const MacroAction& B)
{
    return !(A == B);
}

inline bool IsDuplicateMacro(const Macro& A, const Macro& B)
{
    if (A.Name != B.Name)
        return false;

    if (A.Actions.size() != B.Actions.size())
        return false;

    for (size_t i = 0; i < A.Actions.size(); ++i)
    {
        if (A.Actions[i] != B.Actions[i])
            return false;
    }

    return true;
}

inline const char* ActionTypeName(ActionType Type)
{
    switch (Type)
    {
    case ActionType::KeyPress:
        return "Key Press";
    case ActionType::KeyDown:
        return "Key Down";
    case ActionType::KeyUp:
        return "Key Up";
    case ActionType::MouseMove:
        return "Mouse Move (Abs)";
    case ActionType::MouseMoveRel:
        return "Mouse Move (Rel)";
    case ActionType::MouseClick:
        return "Mouse Click";
    case ActionType::MouseDown:
        return "Mouse Down";
    case ActionType::MouseUp:
        return "Mouse Up";
    case ActionType::MouseScroll:
        return "Mouse Scroll";
    case ActionType::Delay:
        return "Delay";
    default:
        return "Unknown";
    }
}

inline const char* MouseButtonName(MouseButton Button)
{
    switch (Button)
    {
    case MouseButton::Left:
        return "Left";
    case MouseButton::Right:
        return "Right";
    case MouseButton::Middle:
        return "Middle";
    default:
        return "Unknown";
    }
}

inline std::string VKCodeToName(int VK)
{
    if (VK == 0) 
        return "(unbound)";

    char buf[64] = {};
    UINT scanCode = MapVirtualKeyA(static_cast<UINT>(VK), MAPVK_VK_TO_VSC);

    switch (VK)
    {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
    {
        scanCode |= 0x100;
        break;
    }
    default: 
    {
        break;
    }
    }

    GetKeyNameTextA(static_cast<LONG>(scanCode << 16), buf, sizeof(buf));

    if (buf[0] != '\0') 
        return buf;

    static char Fallback[16];
    snprintf(Fallback, sizeof(Fallback), "VK_%02X", VK);
    return Fallback;
}