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
                "Fixed an issue where recording would cause mouse movement to lag.",
                "If \"Lock Input to App\" is enabled on the selected macro, recording will be blocked unless capturing input from that app.",
                "You can now organize your macros by dragging them in any order.",
                "Added foundational keyboard shortcut support to the macros bar on the left.",
            }
        },
    };

    return kChangelog;
}
