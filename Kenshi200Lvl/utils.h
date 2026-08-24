#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <cstdio>
#include <Windows.h>

using namespace std;

#define ConsoleOut(formatString,...) printf(formatString"\n", __VA_ARGS__)

void CreateConsoleWindow();
void DestroyConsoleWindow();
string PathCombine(const string& path1, const string& path2);
string FormatDouble(double value, int maxDigitsAfterPoint = 6);
string ReadMsvcString(uintptr_t strAddr);
string ToLower(string str);
string ToUpper(string str);

template<typename T>
bool SafeRead(uintptr_t addr, T& out)
{
    __try
    {
        out = *reinterpret_cast<const T*>(addr);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

#endif