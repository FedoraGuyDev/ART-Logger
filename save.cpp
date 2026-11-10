#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <commdlg.h>

#include <fstream>
#include <filesystem>
#include <string>

bool SaveTXTDialog(const std::string& content)
{
    wchar_t filename[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};

    ofn.lStructSize = sizeof(ofn);

    ofn.lpstrFilter =
        L"Text Files (*.txt)\0*.txt\0"
        L"All Files (*.*)\0*.*\0";

    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";

    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return false;

    std::ofstream file{std::filesystem::path(filename)};

    if (!file.is_open())
        return false;

    file << content;

    return true;
}
