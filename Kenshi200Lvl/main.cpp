#include <main.h>

#include <utils.h>
#include <hook.h>

using namespace std;

ModConfig modConfig;

//

typedef void (*OriginalFunctionType)(float*, float, float);
OriginalFunctionType levelingFunction = nullptr;

typedef bool(__fastcall* IsPlayerCharacter_fn)(void* character);
IsPlayerCharacter_fn g_isPlayerCharacter = nullptr;

// Ingame Functions

bool CallIsPlayerCharacter_SEH(void* character)
{
    if (!character || !g_isPlayerCharacter) return false;

    bool r = false;
    __try
    {
        r = g_isPlayerCharacter(character);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        r = false;
    }
    return r;
}

// Ingame Callbacks

void HK_AdjustValueBasedOnFactors(float* valuePointer, float factor1, float factor2)
{
    lock_guard<mutex> lock(modConfig.MutexLock);

    uintptr_t character = 0, stats = 0;
    Skill currentSkill = Skill::Unknown;
    ResolveFromStatPtr(valuePointer, stats, character, currentSkill);

    // std::string name = character ? ReadMsvcString(character + 0x18) : "UNKNOWN";

    float val;
    const float invFactor2 = 1.0f / factor2;

    if (*valuePointer < modConfig.FadeLevel)
    {
        float normalizedDifference = (factor2 - *valuePointer) * invFactor2;
        val = normalizedDifference * normalizedDifference;
    }
    else
    {

        const bool dontUsePlayerCurve = !character || (modConfig.PlayerCharactersOnly && !CallIsPlayerCharacter_SEH(reinterpret_cast<void*>(character)));

        if (dontUsePlayerCurve) // NPC + .ini flag or junk
        {
            if (*valuePointer > 101.0f) return;

            float normalizedDifference = (factor2 - *valuePointer) * invFactor2;
            val = normalizedDifference * normalizedDifference;
        }
        else
        {
            float maxLevel = modConfig.MaxLevel;
            for (auto& skill : modConfig.SkillsMaxLevels)
            {
                if (currentSkill == skill.skill)
                {
                    maxLevel = min<float>(skill.value, modConfig.MaxLevel);
                    break;
                }
            }

            if (*valuePointer > maxLevel) return;
            if (maxLevel <= modConfig.FadeLevel) return;

            float normalizedProgress = (*valuePointer - modConfig.FadeLevel) / (maxLevel - modConfig.FadeLevel);
            float baseDifficulty = (factor2 - modConfig.FadeLevel) * invFactor2;
            baseDifficulty *= baseDifficulty;

            // linear
            // val = baseDifficulty * (1.0f - normalizedProgress);

            // smooth (vanilla based)
            val = baseDifficulty * (1.0 - normalizedProgress) * (1.0 - normalizedProgress);
        }
    }

    // NaN Check
    if (val == val && val > 0.0f && factor1 > 0.0f && factor1 <= 20.0f && val <= 20.0f)
    {
        *valuePointer += val * factor1;
    }
}

//

bool ResolveIsPlayerCharacter(const std::string& exePath, const std::string& exeName)
{
    DWORD rva = FindPatternInFile(exePath, IS_PLAYER_CHARACTER_PATTERN, IS_PLAYER_CHARACTER_MASK);
    if (!rva) return false;

    HMODULE base = GetModuleHandleA(exeName.c_str());
    if (!base)
    {
        base = GetModuleHandleW(nullptr);
    }

    g_isPlayerCharacter = reinterpret_cast<IsPlayerCharacter_fn>((DWORD_PTR)base + rva);

    if (modConfig.ShowConsole)
    {
        ConsoleOut("IsPlayerCharacter @ %p (RVA 0x%X)", g_isPlayerCharacter, rva);
    }

    return g_isPlayerCharacter != nullptr;
}

bool ResolveLevelingFunction(const std::string& exePath, const std::string& exeName)
{
    DWORD rva = FindPatternInFile(exePath, LEVELING_FUNCTION_PATTERN, LEVELING_FUNCTION_MASK);
    if (!rva) return false;

    HMODULE base = GetModuleHandleA(exeName.c_str());
    if (!base) base = GetModuleHandleW(nullptr);

    modConfig.LevelingFunctionAbsoluteAddr = reinterpret_cast<LPVOID>((DWORD_PTR)base + rva);

    if (modConfig.ShowConsole)
    {
        ConsoleOut("Leveling function @ %p (RVA 0x%X)", modConfig.LevelingFunctionAbsoluteAddr, rva);
    }

    return modConfig.LevelingFunctionAbsoluteAddr != nullptr;
}

bool SetupLevelingHook(LPVOID absoluteAddr)
{
    if (MH_Initialize() != MH_OK)
    {
        cerr << "Failed to initialize MinHook." << endl;
        return false;
    }

    if (MH_CreateHook(absoluteAddr, &HK_AdjustValueBasedOnFactors, (LPVOID*)&levelingFunction) != MH_OK)
    {
        cerr << "Failed to create the hook." << endl;
        return false;
    }

    return true;
}

bool EnableLevelingHook(LPVOID absoluteAddr)
{
    if (MH_EnableHook(absoluteAddr) != MH_OK)
    {
        cerr << "Failed to enable the hook." << endl;
        return false;
    }

    return true;
}

bool DisableLevelingHook(LPVOID absoluteAddr)
{
    if (MH_DisableHook(absoluteAddr) != MH_OK)
    {
        cerr << "Failed to disable the hook." << endl;
        return false;
    }

    return true;
}

void DetachDLL(LPVOID absoluteAddr, HMODULE hModule)
{
    if (absoluteAddr) DisableLevelingHook(absoluteAddr);
    FreeLibraryAndExitThread(hModule, 0);
}

//

static int ProcessConfigIni(ModConfig& config)
{
    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile(config.ConfigPath.c_str());
    if (rc != SI_OK) return rc;

    config.ModEnabled = !!ini.GetLongValue("Parameters", "Enabled", config.ModEnabled);
    config.MaxLevel = max<float>((float)ini.GetDoubleValue("Parameters", "Max Level", config.MaxLevel), 101.0f);
    config.FadeLevel = clamp<float>((float)ini.GetDoubleValue("Parameters", "Fade Level", config.FadeLevel), 0.0f, 101.0f);
    config.ShowConsole = !!ini.GetLongValue("Parameters", "Debug Console", config.ShowConsole);
    config.PlayerCharactersOnly = !!ini.GetLongValue("Parameters", "Player Characters Only", config.PlayerCharactersOnly);

    for (auto& skill : config.SkillsMaxLevels)
    {
        skill.value = max<float>((float)ini.GetDoubleValue("Parameters", skill.iniName, skill.value), 101.0f);
    }

    return rc;
}

void UpdateModConfigAndData()
{
    if (!filesystem::exists(modConfig.ConfigPath)) return;

    time_t editTimestamp = filesystem::last_write_time(modConfig.ConfigPath).time_since_epoch().count();
    if (editTimestamp == modConfig.ConfigLastEditTimestamp) return;

    modConfig.ConfigLastEditTimestamp = editTimestamp;

    ModConfigSnapshot snapshot(modConfig);

    unique_lock<mutex> lock(modConfig.MutexLock);
    ProcessConfigIni(modConfig);

    if (modConfig.ModEnabled != snapshot.ModEnabled)
    {
        if (modConfig.ModEnabled)
        {
            EnableLevelingHook(modConfig.LevelingFunctionAbsoluteAddr);
        }
        else
        {
            DisableLevelingHook(modConfig.LevelingFunctionAbsoluteAddr);
        }
    }

    lock.unlock();

    if (modConfig.ShowConsole)
    {
        auto currentZone = chrono::current_zone();

        auto now = chrono::system_clock::now();
        chrono::zoned_time localTime(currentZone, now);

        //

        long long totalMilliseconds = chrono::duration_cast<chrono::milliseconds>(localTime.get_local_time().time_since_epoch()).count();

        int hour = (totalMilliseconds / 3600000) % 24;
        int minute = (totalMilliseconds / 60000) % 60;
        int second = (totalMilliseconds / 1000) % 60;
        int millisecond = totalMilliseconds % 1000;

        ConsoleOut("[%02d:%02d:%02d.%03d] Config updated", hour, minute, second, millisecond);
        if (modConfig.ModEnabled != snapshot.ModEnabled)
        {
            ConsoleOut("  Mod %s -> %s",
                snapshot.ModEnabled ? "Enabled" : "Disabled",
                modConfig.ModEnabled ? "Enabled" : "Disabled");
        }
        if (modConfig.MaxLevel != snapshot.MaxLevel)
        {
            ConsoleOut("  Max Level %s -> %s",
                FormatDouble(snapshot.MaxLevel, 4).c_str(),
                FormatDouble(modConfig.MaxLevel, 4).c_str());
        }
        if (modConfig.FadeLevel != snapshot.FadeLevel)
        {
            ConsoleOut("  Fade Level %s -> %s",
                FormatDouble(snapshot.FadeLevel, 4).c_str(),
                FormatDouble(modConfig.FadeLevel, 4).c_str());
        }
        if (modConfig.PlayerCharactersOnly != snapshot.PlayerCharactersOnly)
        {
            ConsoleOut("  Player Characters Only %s -> %s",
                snapshot.PlayerCharactersOnly ? "Enabled" : "Disabled",
                modConfig.PlayerCharactersOnly ? "Enabled" : "Disabled");
        }

        for (int i = 0; i < ModConfig::TotalCappedSkills; i++)
        {
            if (snapshot.SkillCaps[i] != modConfig.SkillsMaxLevels[i].value)
            {
                ConsoleOut("  %s %s -> %s",
                    modConfig.SkillsMaxLevels[i].iniName,
                    FormatDouble(snapshot.SkillCaps[i], 4).c_str(),
                    FormatDouble(modConfig.SkillsMaxLevels[i].value, 4).c_str());
            }
        }
    }
}

void MainThreadFunction(HMODULE hModule)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char dllPath[MAX_PATH];
    GetModuleFileNameA(hModule, dllPath, MAX_PATH);
    string directory = filesystem::path(dllPath).parent_path().string();

    string configPath = PathCombine(directory, "Kenshi200Lvl_config.ini");
    modConfig.ConfigPath = configPath;

    string exeName = filesystem::path(exePath).filename().generic_string();

    //

    SI_Error rc = SI_FAIL;
    bool configExists = filesystem::exists(modConfig.ConfigPath);
    if (!configExists && ToLower(filesystem::path(directory).filename().string()) == "re_kenshi")
    {
        modConfig.ConfigPath = PathCombine(filesystem::path(directory).parent_path().string(), "Kenshi200Lvl_config.ini");
        configExists = filesystem::exists(modConfig.ConfigPath);
    }

    if (configExists)
    {
        rc = ProcessConfigIni(modConfig);
        modConfig.ConfigLastEditTimestamp = filesystem::last_write_time(modConfig.ConfigPath).time_since_epoch().count();
    }

    if (modConfig.ShowConsole)
    {
        CreateConsoleWindow();

        if (configExists)
        {
            ConsoleOut("Loading config from: %s ...", modConfig.ConfigPath.c_str());
            if (rc == SI_OK) ConsoleOut("Config processed");
		    else ConsoleOut("Error: Cannot process config file %d", (int)rc);
        }
        else
        {
            ConsoleOut("CONFIG NOT FOUND: %s", modConfig.ConfigPath.c_str());
            ConsoleOut("Using default values");
        }

        ConsoleOut("");
        ConsoleOut("Game path: %s", exePath);
        ConsoleOut("Game process: %s", exeName.c_str());
        ConsoleOut("Dll path: %s", dllPath);
        ConsoleOut("Config path: %s", modConfig.ConfigPath.c_str());

        ConsoleOut("");
        ConsoleOut("Max Level: %s", FormatDouble(modConfig.MaxLevel, 4).c_str());
        ConsoleOut("Fade Level: %s", FormatDouble(modConfig.FadeLevel, 4).c_str());
        ConsoleOut("");
    }

    //

    do
    {
        if (!ResolveLevelingFunction(exePath, exeName))
        {
            ConsoleOut("Error: Leveling function pattern not found");
            break;
        }
        if (!SetupLevelingHook(modConfig.LevelingFunctionAbsoluteAddr))
        {
            ConsoleOut("Error: Leveling function hook setup failed");
            break;
        }

        if (!ResolveIsPlayerCharacter(exePath, exeName))
        {
            ConsoleOut("Error: IsPlayerCharacter function pattern not found");
            break;
        }

        if (modConfig.ModEnabled)
        {
            EnableLevelingHook(modConfig.LevelingFunctionAbsoluteAddr);
        }

        ConsoleOut("Do not close this window.");

        while (true)
        {
            this_thread::sleep_for(chrono::milliseconds(1000));
            UpdateModConfigAndData();
        }
    } while (false);

    this_thread::sleep_for(chrono::milliseconds(5000));

    DetachDLL(modConfig.LevelingFunctionAbsoluteAddr, hModule);
    DestroyConsoleWindow();
    this_thread::sleep_for(chrono::milliseconds(200));
    exit(1);
}

// Plugins.cfg (Nexus)
extern "C" void __declspec(dllexport) dllStartPlugin(void)
{
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThreadFunction, 0, 0, nullptr);
}

// RE_Kenshi (Steam)
void __declspec(dllexport) startPlugin()
{
    HMODULE hModule = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCSTR>(&startPlugin), &hModule);

    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThreadFunction, hModule, 0, nullptr);
}