#include "sound_player.h"
#include <windows.h>
#include <mmsystem.h>
#include <string>

void SoundPlayer::Stop()
{
    mciSendStringA("stop MacroManagerSnd", nullptr, 0, nullptr);
    mciSendStringA("close MacroManagerSnd", nullptr, 0, nullptr);
}

void SoundPlayer::Play(const std::string& filepath)
{
    Stop();

    std::string OpenCmd  = "open \"" + filepath + "\" type mpegvideo alias MacroManagerSnd";
    std::string PlayCmd  = "play MacroManagerSnd";

    if (mciSendStringA(OpenCmd.c_str(), nullptr, 0, nullptr) == 0)
        mciSendStringA(PlayCmd.c_str(), nullptr, 0, nullptr);
}
