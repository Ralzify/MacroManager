#pragma once
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "macro.h"

class MacroManager
{
public:
    static MacroManager& Get();

    Macro& AddMacro(const std::string& Name = "New Macro");
    bool RemoveMacro(const std::string& id);
    Macro* FindMacro(const std::string& id);
    std::vector<Macro>& GetMacros() { return Macros; }

    void SetEnabled(const std::string& id, bool Enabled);
    void ToggleEnabled(const std::string& id);
    void RebindAll();
    void Execute(const std::string& id);
    bool IsRunning(const std::string& id) const;
    void StopAll();

    static bool ShouldStop();
    static void ClearStop();

private:
    MacroManager() = default;
    static std::string GenerateId();

    std::vector<Macro> Macros;
    mutable std::mutex RunMutex;
    std::unordered_map<std::string, bool> Running;
};