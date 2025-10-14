#include <main.h>

using namespace std;

class ModData
{
public:
    LPVOID TargetProcessAbsoluteAddr = 0x0;

} modData;

class ModConfig
{
public:
    bool ModEnabled = true;
    float MaxLevel = 201.0f;
    float FadeLevel = 65.0f;
    bool ShowConsole = true;

    string configPath;
    time_t configLastEditTimestamp = 0;

    ModConfig()
    {
    }

    ModConfig(bool modEnabled, float maxLevel, float fadeLevel, bool showConsole)
    {
        ModEnabled = modEnabled;
        MaxLevel = maxLevel;
        FadeLevel = fadeLevel;
        ShowConsole = showConsole;
    }
} modConfig;

mutex modConfigAndDataMutex;

vector<BYTE> LEVELING_FUNCTION_PATTERN = {

    0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0xF3, 0x0F,
    0x10, 0x05, 0x0A, 0x65, 0xDB, 0x00, 0x0F, 0x29,
    0x74, 0x24, 0x20, 0xF3, 0x0F, 0x10, 0x31, 0x0F,
    0x28, 0xDA, 0x48, 0x8B, 0xD9, 0xF3, 0x0F, 0x5E,
    0xC2, 0xF3, 0x0F, 0x5C, 0xDE, 0xF3, 0x0F, 0x59,
    0xD8, 0x0F, 0x57, 0xC0, 0x0F, 0x2F, 0xC1, 0xF3,
    0x0F, 0x59, 0xDB, 0x73, 0x47, 0x0F, 0x2F, 0xC3,
    0x73, 0x42, 0xF3, 0x0F, 0x10, 0x05, 0x56, 0xD3,
    0xDB, 0x00, 0x0F, 0x2F, 0xC8, 0x77, 0x35, 0x0F,
    0x2F, 0xD8, 0x77, 0x30, 0xF3, 0x0F, 0x59, 0xD9,
    0xF3, 0x0F, 0x58, 0xDE, 0xF3, 0x0F, 0x11, 0x19,
    0x0F, 0x28, 0xC3, 0xFF, 0x15, 0x27, 0x2B, 0x98,
    0x01, 0x84, 0xC0, 0x74, 0x17, 0x0F, 0x28, 0xC6,
    0xF3, 0x0F, 0x11, 0x33, 0xFF, 0x15, 0x16, 0x2B,
    0x98, 0x01, 0x84, 0xC0, 0x74, 0x06, 0xC7, 0x03,
    0x00, 0x00, 0xA0, 0x41, 0x0F, 0x28, 0x74, 0x24,
    0x20, 0x48, 0x83, 0xC4, 0x30, 0x5B, 0xC3, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
};

const char LEVELING_FUNCTION_MASK[] =
    "xxxxxxxx"
    "xx????xx"
    "xxxxxxxx"
    "xxxxxxxx"
    "xxxxxxxx"
    "xxxxxxxx"
    "xxx??xxx"
    "??xxxx??"
    "??xxx??x"
    "xx??xxxx"
    "xxxxxxxx"
    "xxxxx???"
    "?xx??xxx"
    "xxxxxx??"
    "??xx??xx"
    "xxxxxxxx"
    "xxxxxxxx"
    "xxxxxxxx";

//

typedef void (*OriginalFunctionType)(float*, float, float);
OriginalFunctionType originalFunction = nullptr;

void HK_AdjustValueBasedOnFactors(float* valuePointer, float factor1, float factor2)
{
    lock_guard<mutex> lock(modConfigAndDataMutex);

    if (*valuePointer > modConfig.MaxLevel) return;

    float val;
    const float invFactor2 = 1.0f / factor2;

    if (*valuePointer < modConfig.FadeLevel)
    {
        float normalizedDifference = (factor2 - *valuePointer) * invFactor2;
        val = normalizedDifference * normalizedDifference;
    }
    else
    {
        float normalizedProgress = (*valuePointer - modConfig.FadeLevel) / (modConfig.MaxLevel - modConfig.FadeLevel);
        float baseDifficulty = (factor2 - modConfig.FadeLevel) * invFactor2;
        baseDifficulty *= baseDifficulty;

        // linear
        // val = baseDifficulty * (1.0f - normalizedProgress);

        // smooth
        val = baseDifficulty * (1.0 - normalizedProgress) * (1.0 - normalizedProgress);
    }

    // NaN Check
    if (val == val && val > 0.0f && factor1 > 0.0f && factor1 <= 20.0f && val <= 20.0f)
    {
        *valuePointer += val * factor1;
    }
}

//

bool SetupLevelingHook(LPVOID absoluteAddr)
{
    if (MH_Initialize() != MH_OK)
    {
        cerr << "Failed to initialize MinHook." << endl;
        return false;
    }

    if (MH_CreateHook(absoluteAddr, &HK_AdjustValueBasedOnFactors, (LPVOID*)&originalFunction) != MH_OK)
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

void UpdateModConfigAndData()
{
    if (!filesystem::exists(modConfig.configPath)) return;

    time_t editTimestamp = filesystem::last_write_time(modConfig.configPath).time_since_epoch().count();
    if (editTimestamp == modConfig.configLastEditTimestamp) return;

    modConfig.configLastEditTimestamp = editTimestamp;

    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile(modConfig.configPath.c_str());
    if (rc != SI_OK) return;

    unique_lock<mutex> lock(modConfigAndDataMutex);

    bool lastModEnabledState = modConfig.ModEnabled;

    modConfig.ModEnabled = !!ini.GetLongValue("Parameters", "Enabled", modConfig.ModEnabled);
    modConfig.MaxLevel = max<float>((float)ini.GetDoubleValue("Parameters", "Max Level", modConfig.MaxLevel), 101.0f);
    modConfig.FadeLevel = clamp<float>((float)ini.GetDoubleValue("Parameters", "Fade Level", modConfig.FadeLevel), 0.0f, 101.0f);

    if (modConfig.ModEnabled != lastModEnabledState)
    {
        if (modConfig.ModEnabled)
        {
            EnableLevelingHook(modData.TargetProcessAbsoluteAddr);
        }
        else
        {
            DisableLevelingHook(modData.TargetProcessAbsoluteAddr);
        }
    }

    lock.unlock();

    if (modConfig.ShowConsole) // The console open only on start and this value sets only on start too
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
    }
}

void MainThreadFunction(HMODULE hModule)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    string directory = filesystem::path(exePath).parent_path().string();

    string configPath = PathCombine(directory, "Kenshi200Lvl_config.ini");
    modConfig.configPath = configPath;

    string exeName = filesystem::path(exePath).filename().generic_string();

    //

    bool configExists = filesystem::exists(modConfig.configPath);
    if (configExists)
    {
        CSimpleIniA ini;
        ini.SetUnicode();

        modConfig.configLastEditTimestamp = filesystem::last_write_time(modConfig.configPath).time_since_epoch().count();

        SI_Error rc = ini.LoadFile(modConfig.configPath.c_str());
        if (rc == SI_OK)
        {
            modConfig.ModEnabled = !!ini.GetLongValue("Parameters", "Enabled", modConfig.ModEnabled);
            modConfig.MaxLevel = max<float>((float)ini.GetDoubleValue("Parameters", "Max Level", modConfig.MaxLevel), 101.0f);
            modConfig.FadeLevel = clamp<float>((float)ini.GetDoubleValue("Parameters", "Fade Level", modConfig.FadeLevel), 0.0f, 101.0f);
            modConfig.ShowConsole = !!ini.GetLongValue("Parameters", "Debug Console", modConfig.ShowConsole);
        }
    }

    if (modConfig.ShowConsole)
    {
        CreateConsoleWindow();

        if (configExists)
        {
            ConsoleOut("Loading config from: %s ...", modConfig.configPath.c_str());
            ConsoleOut("Config processed");
        }
        else
        {
            ConsoleOut("CONFIG NOT FOUND: %s", modConfig.configPath.c_str());
            ConsoleOut("Using default values");
        }

        ConsoleOut("");
        ConsoleOut("Game path: %s", exePath);
        ConsoleOut("Game process: %s", exeName.c_str());

        ConsoleOut("");
        ConsoleOut("Max Level: %s", FormatDouble(modConfig.MaxLevel, 4).c_str());
        ConsoleOut("Fade Level: %s", FormatDouble(modConfig.FadeLevel, 4).c_str());
        ConsoleOut("");
    }

    //

    DWORD relativeAddr = FindPatternInFile(exePath, LEVELING_FUNCTION_PATTERN, LEVELING_FUNCTION_MASK);
    if (relativeAddr)
    {
        modData.TargetProcessAbsoluteAddr = (LPVOID)(relativeAddr + (DWORD_PTR)GetModuleHandleA(exeName.c_str()));
        if (SetupLevelingHook(modData.TargetProcessAbsoluteAddr))
        {
            if (modConfig.ShowConsole) ConsoleOut("Function successfully hooked. Do not close this window");

            if (modConfig.ModEnabled)
            {
                EnableLevelingHook(modData.TargetProcessAbsoluteAddr);
            }

            while (true)
            {
                this_thread::sleep_for(chrono::milliseconds(1000));
                UpdateModConfigAndData();
            }
        }
    }

    if (modConfig.ShowConsole) ConsoleOut("Error: Cannot find target function in %s", exePath);

    DetachDLL(modData.TargetProcessAbsoluteAddr, hModule);
    this_thread::sleep_for(chrono::milliseconds(200));
    exit(1);
}

extern "C" void __declspec(dllexport) dllStartPlugin(void)
{
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThreadFunction, 0, 0, nullptr);
}