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
                "Added support for importing Macro Recorder Files (.mrf).   Clicking the \"Import\" button now allows Macro Recorder Files (.mrf) to be imported.",
                "\"Remind Me Later\" will now properly reprompt you to update again once the application is closed & re-opened.",
                "Fixed \"Check For Update\" button - Now correctly checks if version is latest update.",
				"Names >= 20 characters will now be truncated in the macro list to prevent UI overflow.",
				"When importing macros/profiles, duplicate macros will now be skipped instead of imported again.",
                "When deleting a group of selected actions, it will now prompt you to \"Confirm Delete\".",
            }
        },
    };

    return kChangelog;
}
