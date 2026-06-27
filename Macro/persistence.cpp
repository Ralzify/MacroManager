#include "persistence.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <string>
#include <windows.h>
#include <shlobj.h>
using json = nlohmann::json;

static json ActionToJson(const MacroAction& Action)
{
    json JSON;
    JSON["type"] = static_cast<int>(Action.Type);
    JSON["KeyCode"] = Action.KeyCode;
    JSON["MouseX"] = Action.MouseX;
    JSON["MouseY"] = Action.MouseY;
    JSON["MouseButton"] = static_cast<int>(Action.MouseButton);
    JSON["ScrollDelta"] = Action.ScrollDelta;
    JSON["MsDelay"] = Action.MsDelay;
    return JSON;
}

static MacroAction ActionFromJson(const json& JSON)
{
    MacroAction Action;
    Action.Type = static_cast<ActionType>(JSON.value("type", 0));
    Action.KeyCode = JSON.value("KeyCode", 0);
    Action.MouseX = JSON.value("MouseX", 0);
    Action.MouseY = JSON.value("MouseY", 0);
    Action.MouseButton = static_cast<MouseButton>(JSON.value("MouseButton", 0));
    Action.ScrollDelta = JSON.value("ScrollDelta", 120);
    Action.MsDelay = JSON.value("MsDelay", 100);
    return Action;
}

static json MacroToJson(const Macro& Macro)
{
    json JSON;
    JSON["id"] = Macro.ID;
    JSON["name"] = Macro.Name;
    JSON["triggerKey"] = Macro.TriggerKey;
    JSON["enabled"] = Macro.Enabled;
    JSON["repeat"] = Macro.Repeat;
    JSON["repeatCount"] = Macro.RepeatCount;
    json Actions = json::array();

    for (const auto& a : Macro.Actions)
        Actions.push_back(ActionToJson(a));

    JSON["actions"] = Actions;
    return JSON;
}

static Macro MacroFromJson(const json& JSON)
{
    Macro Macro;
    Macro.ID = JSON.value("id", "");
    Macro.Name = JSON.value("name", "Unnamed");
    Macro.TriggerKey = JSON.value("triggerKey", 0);
    Macro.Enabled = JSON.value("enabled", true);
    Macro.Repeat = JSON.value("repeat", false);
    Macro.RepeatCount = JSON.value("repeatCount", 1);

    if (JSON.contains("actions") && JSON["actions"].is_array())
    {
        for (const auto& a : JSON["actions"])
            Macro.Actions.push_back(ActionFromJson(a));
    }

    return Macro;
}

bool Persistence::Save(const std::vector<Macro>& Macros, const std::string& FilePath)
{
    try
    {
        std::string Directory = FilePath;
        auto Slash = Directory.find_last_of("\\/");

        if (Slash != std::string::npos)
        {
            Directory = Directory.substr(0, Slash);
            CreateDirectoryA(Directory.c_str(), nullptr);
        }

        std::ofstream File(FilePath);

        if (!File.is_open())
            return false;

        json Root = json::array();

        for (const auto& Macro : Macros)
            Root.push_back(MacroToJson(Macro));

        File << Root.dump(4);
        return true;
    }

    catch (...) { return false; }
}

bool Persistence::SaveOne(const Macro& Macro, const std::string& FilePath)
{
    try
    {
        std::string Directory = FilePath;
        auto Slash = Directory.find_last_of("\\/");

        if (Slash != std::string::npos)
        {
            Directory = Directory.substr(0, Slash);
            CreateDirectoryA(Directory.c_str(), nullptr);
        }

        std::ofstream File(FilePath);

        if (!File.is_open())
            return false;

        json Root = json::array();
        Root.push_back(MacroToJson(Macro));

        File << Root.dump(4);
        return true;
    }

    catch (...) { return false; }
}

bool Persistence::Load(std::vector<Macro>& Macros, const std::string& FilePath)
{
    try
    {
        std::ifstream File(FilePath);
        if (!File.is_open()) return false;

        json Root;
        File >> Root;

        if (!Root.is_array())
            return false;

        Macros.clear();

        for (const auto& j : Root)
            Macros.push_back(MacroFromJson(j));

        return true;
    }

    catch (...) { return false; }
}

int Persistence::Append(std::vector<Macro>& Macros, const std::string& FilePath)
{
    try
    {
        std::ifstream File(FilePath);
        if (!File.is_open()) return -1;

        json Root;
        File >> Root;

        if (!Root.is_array())
            return -1;

        int Imported = 0;

        for (const auto& j : Root)
        {
            Macros.push_back(MacroFromJson(j));
            ++Imported;
        }

        return Imported;
    }

    catch (...) { return -1; }
}

std::string Persistence::DefaultFilePath()
{
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
    {
        std::string Directory = std::string(path) + "\\Macro Manager";
        CreateDirectoryA(Directory.c_str(), nullptr);
        return Directory + "\\macros.json";
    }
    return "macros.json";
}

static int MgpKeyNameToVK(const std::string& Name)
{
    if (Name.size() == 1)
    {
        char Character = Name[0];

        if (Character >= 'a' && Character <= 'z')
            return VkKeyScanA(Character) & 0xFF;
        if (Character >= 'A' && Character <= 'Z')
            return (int)Character;
        if (Character >= '0' && Character <= '9')
            return (int)Character;

        SHORT vks = VkKeyScanA(Character);

        if (vks != -1) 
            return vks & 0xFF;
    }

    if (Name == "ESC")
        return VK_ESCAPE;
    if (Name == "SPACE")
        return VK_SPACE;
    if (Name == "ENTER")
        return VK_RETURN;
    if (Name == "TAB")
        return VK_TAB;
    if (Name == "BACK")
        return VK_BACK;
    if (Name == "DELETE")
        return VK_DELETE;
    if (Name == "INSERT")
        return VK_INSERT;
    if (Name == "HOME")
        return VK_HOME;
    if (Name == "END")
        return VK_END;
    if (Name == "PGUP")
        return VK_PRIOR;
    if (Name == "PGDN")
        return VK_NEXT;
    if (Name == "UP")
        return VK_UP;
    if (Name == "DOWN")
        return VK_DOWN;
    if (Name == "LEFT")
        return VK_LEFT;
    if (Name == "RIGHT")
        return VK_RIGHT;
    if (Name == "F1")
        return VK_F1;
    if (Name == "F2")
        return VK_F2;
    if (Name == "F3")
        return VK_F3;
    if (Name == "F4")
        return VK_F4;
    if (Name == "F5")
        return VK_F5;
    if (Name == "F6")
        return VK_F6;
    if (Name == "F7")
        return VK_F7;
    if (Name == "F8")
        return VK_F8;
    if (Name == "F9")
        return VK_F9;
    if (Name == "F10")
        return VK_F10;
    if (Name == "F11")
        return VK_F11;
    if (Name == "F12")
        return VK_F12;
    if (Name == "SHIFT" || Name == "SHIFTDOWN" || Name == "SHIFTUP")
        return VK_SHIFT;
    if (Name == "CTRL")
        return VK_CONTROL;
    if (Name == "ALT")
        return VK_MENU;
    if (Name == "LSHIFT")
        return VK_LSHIFT;
    if (Name == "RSHIFT")
        return VK_RSHIFT;
    if (Name == "LCTRL")
        return VK_LCONTROL;
    if (Name == "RCTRL")
        return VK_RCONTROL;

    return 0;
}

static std::string Trim(const std::string& String)
{
    size_t a = String.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = String.find_last_not_of(" \t\r\n");
    return String.substr(a, b - a + 1);
}

static void ParseMgpEvent(const std::string& Value, std::vector<MacroAction>& Out)
{
    if (Value.size() >= 2 && Value.back() == 'D' && Value[Value.size() - 2] == ' ')
    {
        std::string NumStr = Trim(Value.substr(0, Value.size() - 2));
        try {
            int Ms = std::stoi(NumStr);

            if (Ms > 0)
            {
                MacroAction Action;
                Action.Type = ActionType::Delay;
                Action.MsDelay = Ms;
                Out.push_back(Action);
            }
        }
        catch (...) {}
        return;
    }

    if (Value.size() > 2 && Value[0] == 'M' && Value[1] == ' ')
    {
        size_t lp = Value.find('(');
        size_t cm = Value.find(',', lp);
        size_t rp = Value.find(')', cm);
        if (lp != std::string::npos && cm != std::string::npos && rp != std::string::npos)
        {
            try {
                int x = std::stoi(Value.substr(lp + 1, cm - lp - 1));
                int y = std::stoi(Value.substr(cm + 1, rp - cm - 1));
                MacroAction Action;
                Action.Type = ActionType::MouseMove;
                Action.MouseX = x;
                Action.MouseY = y;
                Out.push_back(Action);
            }
            catch (...) {}
        }
        return;
    }

    if (!Value.empty() && Value[0] == '{')
    {
        size_t Close = Value.find('}');
        if (Close == std::string::npos) return;

        std::string Inner = Trim(Value.substr(1, Close - 1));

        bool IsDown = false;
        std::string KeyName;

        if (Inner == "SHIFTDOWN") { KeyName = "SHIFT"; IsDown = true; }
        else if (Inner == "SHIFTUP") { KeyName = "SHIFT"; IsDown = false; }
        else
        {
            size_t Space = Inner.rfind(' ');

            if (Space == std::string::npos)
                return;

            KeyName = Trim(Inner.substr(0, Space));
            std::string Directory = Trim(Inner.substr(Space + 1));
            IsDown = (Directory == "down");
        }

        MouseButton MouseButton = MouseButton::Left;
        bool IsMouse = false;
        if (KeyName == "LMouse") { MouseButton = MouseButton::Left;   IsMouse = true; }
        if (KeyName == "RMouse") { MouseButton = MouseButton::Right;  IsMouse = true; }
        if (KeyName == "MMouse") { MouseButton = MouseButton::Middle; IsMouse = true; }

        if (IsMouse)
        {
            int mx = 0, my = 0;
            size_t lp = Value.find('(', Close);
            if (lp != std::string::npos)
            {
                size_t cm = Value.find(',', lp);
                size_t rp = Value.find(')', cm != std::string::npos ? cm : lp);

                if (cm != std::string::npos && rp != std::string::npos)
                {
                    try
                    {
                        mx = std::stoi(Value.substr(lp + 1, cm - lp - 1));
                        my = std::stoi(Value.substr(cm + 1, rp - cm - 1));
                    }
                    catch (...) {}
                }
            }

            if (mx != 0 || my != 0)
            {
                MacroAction Macro;
                Macro.Type = ActionType::MouseMove;
                Macro.MouseX = mx;
                Macro.MouseY = my;
                Out.push_back(Macro);
            }

            MacroAction Action;
            Action.Type = IsDown ? ActionType::MouseDown : ActionType::MouseUp;
            Action.MouseButton = MouseButton;
            Action.MouseX = mx;
            Action.MouseY = my;
            Out.push_back(Action);
            return;
        }

        int vk = MgpKeyNameToVK(KeyName);
        if (vk == 0)
            return;

        MacroAction Action;
        Action.Type = IsDown ? ActionType::KeyDown : ActionType::KeyUp;
        Action.KeyCode = vk;
        Out.push_back(Action);
        return;
    }
}

int Persistence::ImportMGP(std::vector<Macro>& macros, const std::string& FilePath)
{
    std::ifstream File(FilePath);

    if (!File.is_open())
        return -1;

    static std::mt19937_64 rng(std::random_device{}());
    auto makeId = [&]() -> std::string {
        std::uniform_int_distribution<uint64_t> d;
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << d(rng) << std::setw(16) << std::setfill('0') << d(rng);
        return oss.str();
        };

    int Imported = 0;
    Macro Current;
    bool InMacro = false;

    auto FinishMacro = [&]()
        {
            if (InMacro && !Current.Name.empty())
            {
                std::vector<MacroAction> collapsed;
                for (size_t i = 0; i < Current.Actions.size(); ++i)
                {
                    if (Current.Actions[i].Type == ActionType::KeyDown
                        && i + 1 < Current.Actions.size()
                        && Current.Actions[i + 1].Type == ActionType::KeyUp
                        && Current.Actions[i + 1].KeyCode == Current.Actions[i].KeyCode)
                    {
                        MacroAction kp = Current.Actions[i];
                        kp.Type = ActionType::KeyPress;
                        collapsed.push_back(kp);
                        ++i;
                    }
                    else
                    {
                        collapsed.push_back(Current.Actions[i]);
                    }
                }
                Current.Actions = std::move(collapsed);
                macros.push_back(std::move(Current));
                ++Imported;
            }

            Current = Macro{};
            InMacro = false;
        };

    std::string Line;
    while (std::getline(File, Line))
    {
        Line = Trim(Line);

        if (Line.empty())
            continue;

        if (Line.front() == '[' && Line.back() == ']')
        {
            FinishMacro();
            Current.ID = makeId();
            Current.Name = Line.substr(1, Line.size() - 2);
            Current.Enabled = true;
            Current.Repeat = false;
            InMacro = true;
            continue;
        }

        if (!InMacro)
            continue;

        if (Line.substr(0, 10) == "bindedkey=")
        {
            std::string keyStr = Trim(Line.substr(10));
            Current.TriggerKey = MgpKeyNameToVK(keyStr);
            continue;
        }

        if (Line.substr(0, 7) == "repeat=")
        {
            try {
                long long RepeatAmt = std::stoll(Line.substr(7));

                if (RepeatAmt == 1)
                {
                    Current.Repeat = false;
                    Current.RepeatCount = 1;
                }
                else if (RepeatAmt >= 4294967296LL)
                {
                    Current.Repeat = true;
                    Current.RepeatCount = 0;
                }
                else
                {
                    Current.Repeat = true;
                    Current.RepeatCount = static_cast<int>(RepeatAmt);
                }
            }
            catch (...) {}
            continue;
        }

        if (Line.size() > 2 && Line[0] == 'e' && Line[1] == '=')
        {
            std::string Value = Line.substr(2);
            ParseMgpEvent(Value, Current.Actions);
            continue;
        }
    }

    FinishMacro();
    return Imported;
}