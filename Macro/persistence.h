#pragma once
#include <string>
#include <vector>
#include "macro.h"

class Persistence
{
public:
    static bool Save(const std::vector<Macro>& macros, const std::string& filepath);
    static bool Load(std::vector<Macro>& macros, const std::string& filepath);
    static bool SaveOne(const Macro& macro, const std::string& filepath);
    static int Append(std::vector<Macro>& macros, const std::string& filepath);
    static int ImportMGP(std::vector<Macro>& macros, const std::string& filepath);

    static std::string DefaultFilePath();
};