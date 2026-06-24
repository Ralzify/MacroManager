#pragma once
#include <string>

class SoundPlayer
{
public:
    static void Play(const std::string& filepath);

    static void Stop();
};
