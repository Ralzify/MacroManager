#pragma once

#include "version.h"

#include <string>
#include <vector>

struct ChangelogVersion
{
    std::string Version;
    std::vector<std::string> Changes;
};

inline const std::vector<ChangelogVersion>& GetChangelog()
{
    static const std::vector<ChangelogVersion> kChangelog =
    {
        {
            APP_VERSION_STRING,
            {
                "Fixed importing deleting all existing macros (was still using old pre-release import logic).",
                "Fixed key presses being capped to a background application's frame rate.",
                "Fixed the \"Toggle Macro\" hotkey occasionally resetting itself.",
                "Added \"Lock Input to Tab\" - Only allows a macro to run if the specified application is focused.",
                "Added a \"Start All\" button to enable every macro at once.",
            }
        },
    };

    return kChangelog;
}
