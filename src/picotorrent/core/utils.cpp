#include "utils.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <sstream>

#include <Windows.h>
#include <ShlObj.h>
#include <shlwapi.h>

namespace fs = std::filesystem;

using pt::Utils;

void Utils::openAndSelect(fs::path path)
{
    std::wstringstream ss;
    ss << L"explorer.exe /select,\"" << path.native() << L"\"";
    wxExecute(ss.str());
}

std::wstring Utils::toHumanFileSize(int64_t bytes)
{
    TCHAR buf[1024];
    StrFormatByteSize64(bytes, buf, 1024);
    return buf;
}

std::string Utils::toStdString(std::wstring const& input)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &input[0], (int)input.size(), &result[0], size, NULL, NULL);
    return result;
}

std::wstring Utils::toStdWString(std::string const& input)
{
    // With the exact length (not -1) the result does not end in an extra '\0', which used to
    // cut every text joined after it (Windows controls stop at the first '\0')
    if (input.empty()) { return std::wstring(); }

    int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), NULL, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), &result[0], size);
    return result;
}
