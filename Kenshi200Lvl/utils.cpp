#define _CRT_SECURE_NO_WARNINGS

#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>

#include <utils.h>

void CreateConsoleWindow()
{
    AllocConsole();
    static_cast<void>(freopen("CONIN$", "r", stdin));
    static_cast<void>(freopen("CONOUT$", "w", stdout));
    static_cast<void>(freopen("CONOUT$", "w", stderr));
}

void DestroyConsoleWindow()
{
    FreeConsole();
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
}

string PathCombine(const string& path1, const string& path2)
{
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;

    char sep = '/';
#ifdef _WIN32
    sep = '\\';
#endif

    string result = path1;
    if (result.back() != sep)
    {
        result += sep;
    }
    result += path2;

    return result;
}

string FormatDouble(double value, int maxDigitsAfterPoint)
{
    ostringstream oss;
    oss << fixed << setprecision(maxDigitsAfterPoint) << value;
    string str = oss.str();

    str.erase(str.find_last_not_of('0') + 1, string::npos);
    if (str.back() == '.')
    {
        str.pop_back();
    }

    return str;
}

string ReadMsvcString(uintptr_t strAddr)
{
    uint64_t size = 0, capacity = 0;
    if (!SafeRead(strAddr + 0x10, size) || !SafeRead(strAddr + 0x18, capacity)) return {};
    if (size == 0 || size > 512) return {};

    char buf[513]{};
    if (capacity > 15)
    {
        uintptr_t heap = 0;
        if (!SafeRead(strAddr, heap) || !heap) return {};
        for (size_t i = 0; i < size && i < 512; ++i)
        {
            SafeRead(heap + i, buf[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < size && i < 15; ++i)
        {
            SafeRead(strAddr + i, buf[i]);
        }
    }

    return string(buf, (size_t)size);
}

string ToLower(string str)
{
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

string ToUpper(string str)
{
    transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}