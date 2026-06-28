#pragma once

#include "recorder.h"
#include "macro.h"

#include <string>
#include <vector>

class Persistence
{
public:
    static bool Save(const std::vector<Macro>& macros, const std::string& filepath);
    static bool Load(std::vector<Macro>& macros, const std::string& filepath);
    static bool SaveOne(const Macro& macro, const std::string& filepath);
    static int Append(std::vector<Macro>& macros, const std::string& filepath);
    static int ImportMGP(std::vector<Macro>& macros, const std::string& filepath);
    static bool SaveSettings(const RecorderOptions& options, const std::string& filepath);
    static bool LoadSettings(RecorderOptions& options, const std::string& filepath);
    static std::string LoadLastSeenVersion();
    static bool SaveLastSeenVersion(const std::string& version);
    static std::string LastSeenVersionFilePath();
    static std::string SettingsFilePath();

    static std::string DefaultFilePath();
};