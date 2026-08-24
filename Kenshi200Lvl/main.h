#pragma once

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <thread>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <fstream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <span>
#include <mutex>

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <MinHook.h>
#include <SimpleIni.h>
#include <pattern.h>

bool IsGameBreakingSkill(Skill skill);

struct SkillMaxLevelConfig
{
    const char* iniName;
    Skill skill;
    float value;
};

struct ModConfig
{
    std::mutex MutexLock;

    LPVOID LevelingFunctionAbsoluteAddr = 0x0;

    //

    bool ModEnabled = true;
    float MaxLevel = 201.0f;
    float FadeLevel = 65.0f;
    bool PlayerCharactersOnly = true;
    bool ShowConsole = true;

    static const int TotalCappedSkills = 14;
    SkillMaxLevelConfig SkillsMaxLevels[TotalCappedSkills] =
    {
        { "Science Max Level", Skill::Science, 101.0f },
        { "Engineer Max Level", Skill::Engineer, 101.0f },
        { "WeaponSmith Max Level", Skill::WeaponSmith, 101.0f },
        { "ArmourSmith Max Level", Skill::ArmourSmith, 101.0f },
        { "BowSmith Max Level", Skill::BowSmith, 101.0f },
        { "Robotics Max Level", Skill::Robotics, 101.0f },
        { "Labouring Max Level", Skill::Labouring, 101.0f },
        { "Farming Max Level", Skill::Farming, 101.0f },
        { "Cooking Max Level", Skill::Cooking, 101.0f },
        { "Medic Max Level", Skill::Medic, 101.0f },
        { "Stealth Max Level", Skill::Stealth, 101.0f },
        { "Thieving Max Level", Skill::Thieving, 101.0f },
        { "Assassin Max Level", Skill::Assassin, 101.0f },
        { "Lockpicking Max Level", Skill::Lockpicking, 101.0f },
    };

    std::string ConfigPath;
    time_t ConfigLastEditTimestamp = 0;

    ModConfig() {}
};

struct ModConfigSnapshot
{
    bool ModEnabled;
    float MaxLevel, FadeLevel;
    bool PlayerCharactersOnly;
    float SkillCaps[ModConfig::TotalCappedSkills];

    ModConfigSnapshot(ModConfig& config)
    {
        ModEnabled = config.ModEnabled;
        MaxLevel = config.MaxLevel;
        FadeLevel = config.FadeLevel;
        PlayerCharactersOnly = config.PlayerCharactersOnly;

        for (int i = 0; i < ModConfig::TotalCappedSkills; i++)
        {
            SkillCaps[i] = config.SkillsMaxLevels[i].value;
        }
    }
};