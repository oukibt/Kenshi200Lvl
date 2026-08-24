#include <hook.h>
#include <MinHook.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>

DWORD RvaToOffset(DWORD rva, PIMAGE_SECTION_HEADER sectionHeader, unsigned int numberOfSections)
{
    for (unsigned int i = 0; i < numberOfSections; ++i)
    {
        if (rva >= sectionHeader[i].VirtualAddress && rva < sectionHeader[i].VirtualAddress + sectionHeader[i].Misc.VirtualSize)
        {
            return rva - sectionHeader[i].VirtualAddress + sectionHeader[i].PointerToRawData;
        }
    }
    return rva;
}

DWORD FindPatternInFile(const string& filename, const span<const BYTE>& pattern, const char* mask)
{
    ifstream file(filename, ios::binary | ios::ate);
    if (!file)
    {
        cerr << "Error: Cannot open file!" << endl;
        return 0;
    }

    streamsize fileSize = file.tellg();
    vector<BYTE> buffer(fileSize);
    file.seekg(0, ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    if (!file)
    {
        cerr << "Error: Cannot read file!" << endl;
        return 0;
    }

    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        cerr << "Error: Not a valid PE file!" << endl;
        return 0;
    }

    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        cerr << "Error: Invalid NT header!" << endl;
        return 0;
    }

    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

    size_t maskLength = strlen(mask);
    if (maskLength != pattern.size())
    {
        cerr << "Error: Mask length does not match pattern size!" << endl;
        return 0;
    }

    size_t patternSize = pattern.size();
    size_t bufferSize = buffer.size();
    size_t diffSize = bufferSize - patternSize;

    for (size_t i = 0; i <= diffSize; i++)
    {
        bool found = true;
        for (size_t j = 0; j < patternSize; j++)
        {
            if (mask[j] == 'x' && buffer[i + j] != pattern[j])
            {
                found = false;
                break;
            }
        }

        if (found)
        {
            DWORD rva = static_cast<DWORD>(i);
            DWORD offset = RvaToOffset(rva, sectionHeader, ntHeaders->FileHeader.NumberOfSections);
            return rva + (rva - offset);
        }
    }

    return 0;
}