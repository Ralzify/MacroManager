#include "macro_manager.h"
#include "hook_manager.h"
#include "input_sim.h"
#include <algorithm>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <unordered_map>

static std::atomic<bool> bStopAll{ false };

static bool ForegroundMatchesLockedApp(const std::string& LockedApp)
{
    HWND FgWnd = GetForegroundWindow();

    if (!FgWnd)
        return false;

    char WndTitle[512] = {};
    GetWindowTextA(FgWnd, WndTitle, sizeof(WndTitle));
    std::string FgTitle(WndTitle);

    if (FgTitle.empty())
        return false;

    if (FgTitle == LockedApp)
        return true;

    const std::string Suffix = " - " + LockedApp;

    if (FgTitle.size() < Suffix.size())
        return false;

    return FgTitle.compare(FgTitle.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
}


bool MacroManager::ShouldStop()
{
    return bStopAll.load();
}

void MacroManager::ClearStop()
{
    bStopAll = false;
}

MacroManager& MacroManager::Get()
{
    static MacroManager Instance;
    return Instance;
}

std::string MacroManager::GenerateId()
{
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream OSS;
    OSS << std::hex << std::setw(16) << std::setfill('0') << dist(rng) << std::setw(16) << std::setfill('0') << dist(rng);
    return OSS.str();
}

Macro& MacroManager::AddMacro(const std::string& Name)
{
    Macro Macro;
    Macro.ID = GenerateId();
    Macro.Name = Name;
    Macro.Enabled = true;
    Macros.push_back(std::move(Macro));
    RebindAll();
    return Macros.back();
}

bool MacroManager::RemoveMacro(const std::string& id)
{
    auto it = std::find_if(Macros.begin(), Macros.end(), [&](const Macro& m) { return m.ID == id; });

    if (it == Macros.end())
        return false;

    HookManager::Get().UnregisterCallback("macro_" + id);
    Macros.erase(it);
    return true;
}

Macro* MacroManager::FindMacro(const std::string& id)
{
    auto it = std::find_if(Macros.begin(), Macros.end(), [&](const Macro& m) { return m.ID == id; });

    return (it != Macros.end()) ? &(*it) : nullptr;
}

void MacroManager::SetEnabled(const std::string& id, bool Enabled)
{
    if (auto* Macro = FindMacro(id)) { Macro->Enabled = Enabled; RebindAll(); }
}

void MacroManager::ToggleEnabled(const std::string& id)
{
    if (auto* Macro = FindMacro(id)) SetEnabled(id, !Macro->Enabled);
}

void MacroManager::RebindAll()
{
    for (auto& Macro : Macros)
        HookManager::Get().UnregisterCallback("macro_" + Macro.ID);

    for (auto& Macro : Macros)
    {
        if (!Macro.Enabled || Macro.TriggerKey == 0)
            continue;

        const std::string id = Macro.ID;
        const bool lockToApp = Macro.LockInputToApp;
        const std::string lockedApp = Macro.LockedAppName;
        HookManager::Get().RegisterCallback("macro_" + id, Macro.TriggerKey, [id, lockToApp, lockedApp](int, bool isKeyDown) -> bool {
            if (!isKeyDown)
                return false;

            if (lockToApp)
            {
                if (lockedApp.empty() || !ForegroundMatchesLockedApp(lockedApp))
                    return false;
            }

            MacroManager::Get().ClearStop();
            MacroManager::Get().Execute(id);
            return true;
            });
    }
}


void MacroManager::Execute(const std::string& id)
{
    Macro* Macro = FindMacro(id);

    if (!Macro || !Macro->Enabled)
        return;

    const auto& Actions = Macro->Actions;
    const bool Repeat = Macro->Repeat;
    const int RepeatCount = Macro->RepeatCount;

    {
        std::lock_guard<std::mutex> Lock(RunMutex);

        if (Running[id])
            return;

        Running[id] = true;
    }

    std::thread([this, id, Actions, Repeat, RepeatCount]()
        {
            int RunsLeft = (Repeat && RepeatCount > 0) ? RepeatCount : -1;

            do
            {
                InputSim::PlayMacro(Actions);

                if (bStopAll)
                    break;

                if (RunsLeft > 0)
                {
                    --RunsLeft;

                    if (RunsLeft == 0)
                        break;
                }

                bool ShouldStop = false;
                { std::lock_guard<std::mutex> Lock(RunMutex); ShouldStop = !Running[id]; }

                if (ShouldStop)
                    break;

            } while (Repeat);

            { std::lock_guard<std::mutex> Lock(RunMutex); Running[id] = false; }
        }).detach();
}

bool MacroManager::IsRunning(const std::string& id) const
{
    std::lock_guard<std::mutex> Lock(RunMutex);
    auto it = Running.find(id);
    return (it != Running.end()) && it->second;
}

void MacroManager::StopAll()
{
    for (auto& Macro : Macros)
        Macro.Enabled = false;

    RebindAll();
}

void MacroManager::StartAll()
{
    for (auto& Macro : Macros)
        Macro.Enabled = true;

    RebindAll();
}