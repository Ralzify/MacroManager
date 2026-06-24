#pragma once
#include <string>
#include <vector>
#include "macro.h"

class Persistence
{
public:
    // Save all macros to a JSON file. Returns true on success.
    static bool Save(const std::vector<Macro>& macros, const std::string& filepath);

    // Load macros from a JSON file. Returns true on success.
    static bool Load(std::vector<Macro>& macros, const std::string& filepath);

    // Import macros from a MacroGamer .mgp profile file.
    // Appends imported macros to the existing list. Returns number imported (or -1 on error).
    static int ImportMGP(std::vector<Macro>& macros, const std::string& filepath);

    // Default file used by the app
    static std::string DefaultFilePath();
};