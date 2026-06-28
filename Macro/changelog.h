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
                "Added foundational keyboard shortcuts support (CTRL + C, CTRL + V, Backspace).",
                "Click one action, hold \"LEFT SHIFT\" and click another to select a list of actions.",
                "Added a \"Confirm Clear\" popup when deleting an action.",
            }
        },
    };

    return kChangelog;
}
