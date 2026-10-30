#include "utils.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <shlwapi.h>
#else
#include <wx/filename.h>
#endif

namespace fs = std::filesystem;

using pt::Utils;

void Utils::openAndSelect(fs::path path)
{
#ifdef _WIN32
    std::wstringstream ss;
    ss << L"explorer.exe /select,\"" << path.native() << L"\"";
    wxExecute(ss.str());
#else
    // macOS: a Finder window with the file selected
    const char* const argv[] = { "/usr/bin/open", "-R", path.c_str(), nullptr };
    wxExecute(argv, wxEXEC_ASYNC);
#endif
}

std::wstring Utils::toHumanFileSize(int64_t bytes)
{
#ifdef _WIN32
    TCHAR buf[1024];
    StrFormatByteSize64(bytes, buf, 1024);
    return buf;
#else
    // Like the Windows function: units of 1024, "KB", "MB"... with one decimal
    return wxFileName::GetHumanReadableSize(wxULongLong(bytes > 0 ? static_cast<wxULongLong_t>(bytes) : 0), "0 B", 1, wxSIZE_CONV_TRADITIONAL).ToStdWstring();
#endif
}

std::string Utils::toStdString(std::wstring const& input)
{
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &input[0], (int)input.size(), &result[0], size, NULL, NULL);
    return result;
#else
    // wchar_t is UTF-32 on macOS. Anything that is not a character becomes U+FFFD, as on Windows.
    std::string result;
    result.reserve(input.size());

    for (wchar_t w : input)
    {
        char32_t c = static_cast<char32_t>(w);

        if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) { c = 0xFFFD; }

        if (c < 0x80)
        {
            result += static_cast<char>(c);
        }
        else if (c < 0x800)
        {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
        else
        {
            result += static_cast<char>(0xF0 | (c >> 18));
            result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    return result;
#endif
}

std::wstring Utils::toStdWString(std::string const& input)
{
    // With the exact length (not -1) the result does not end in an extra '\0', which used to
    // cut every text joined after it (Windows controls stop at the first '\0')
    if (input.empty()) { return std::wstring(); }

#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), NULL, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), &result[0], size);
    return result;
#else
    // UTF-8 to UTF-32 (wchar_t on macOS). A byte that does not start a valid character becomes
    // U+FFFD and decoding goes on with the next one, as MultiByteToWideChar does.
    std::wstring result;
    result.reserve(input.size());

    size_t i = 0;

    while (i < input.size())
    {
        unsigned char b = static_cast<unsigned char>(input[i]);
        char32_t c = 0;
        size_t length = 0;
        char32_t minimum = 0;

        if (b < 0x80) { c = b; length = 1; }
        else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; length = 2; minimum = 0x80; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; length = 3; minimum = 0x800; }
        else if ((b & 0xF8) == 0xF0) { c = b & 0x07; length = 4; minimum = 0x10000; }

        bool valid = length > 0 && i + length <= input.size();

        for (size_t k = 1; valid && k < length; k++)
        {
            unsigned char next = static_cast<unsigned char>(input[i + k]);

            if ((next & 0xC0) != 0x80) { valid = false; break; }

            c = (c << 6) | (next & 0x3F);
        }

        if (valid && length > 1 && (c < minimum || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)))
        {
            valid = false;
        }

        if (!valid)
        {
            result += static_cast<wchar_t>(0xFFFD);
            i += 1;
            continue;
        }

        result += static_cast<wchar_t>(c);
        i += length;
    }

    return result;
#endif
}
