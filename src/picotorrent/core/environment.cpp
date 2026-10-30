#include "environment.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#else
#include <ctime>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/uilocale.h>
#endif

#pragma warning(push)
#pragma warning(disable: 4244)
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#pragma warning(pop)

#include "utils.hpp"

namespace fs = std::filesystem;
using pt::Core::Environment;

namespace expr = boost::log::expressions;

Environment::Environment()
    : m_startupTime(std::chrono::system_clock::now())
{
}

std::shared_ptr<Environment> Environment::Create()
{
    auto env = new Environment();

    // Before anything is written under the new names
    std::string migration = env->MigrateFromBrizna();

    boost::log::add_file_log
    (
        boost::log::keywords::file_name = Utils::toStdString(env->GetLogFilePath().generic_wstring()),
        boost::log::keywords::format = expr::stream
            << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " "
            << "[" << expr::attr<boost::log::attributes::timer::value_type>("Uptime") << "] "
            << "[" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "] "
            << boost::log::trivial::severity << ": "
            << expr::message
    );

    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute("Uptime", boost::log::attributes::timer());

    BOOST_LOG_TRIVIAL(info) << "HNTorrent starting up...";

    if (!migration.empty())
    {
        BOOST_LOG_TRIVIAL(info) << "Data of Brizna: " << migration;
    }

    return std::shared_ptr<Environment>(env);
}

std::string Environment::MigrateFromBrizna()
{
    std::string result;
    std::error_code ec;

#ifndef _WIN32
    // Brizna was only published for Windows: on macOS there is nothing to move
    return result;
#else
    auto note = [&result](std::string const& text)
    {
        result += (result.empty() ? "" : "; ") + text;
    };

    // Brizna was only published as a portable .zip (no installer), so its data is always next
    // to the executable.

    // "Start with Windows" (Preferences > General) pointed to Brizna.exe under the name Brizna
    HKEY run = NULL;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ | KEY_WRITE, &run) == ERROR_SUCCESS)
    {
        wchar_t value[MAX_PATH * 2] = {};
        DWORD size = sizeof(value) - sizeof(wchar_t);

        if (RegQueryValueEx(run, TEXT("Brizna"), NULL, NULL, reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS
            && StrStrI(value, L"\\Brizna.exe") != nullptr)
        {
            TCHAR path[MAX_PATH];
            GetModuleFileName(NULL, path, ARRAYSIZE(path));
            std::wstring quoted = L"\"" + std::wstring(path) + L"\"";

            if (RegSetValueEx(run, TEXT("HNTorrent"), 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted.c_str()), static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS)
            {
                RegDeleteValue(run, TEXT("Brizna"));
                note("moved the start-up entry");
            }
        }

        RegCloseKey(run);
    }

    // The list of torrents and the settings. A journal means a write was interrupted: SQLite
    // repairs it the next time the old file is opened, so the move waits for another start.
    fs::path dir = GetApplicationDataPath();
    fs::path legacy = dir / "Brizna.sqlite";
    fs::path current = dir / "HNTorrent.sqlite";

    if (!fs::exists(current, ec) && fs::exists(legacy, ec))
    {
        fs::path journal = legacy;
        journal += "-journal";
        fs::path wal = legacy;
        wal += "-wal";

        if (fs::exists(journal, ec) || fs::exists(wal, ec))
        {
            note("Brizna.sqlite has a journal, it stays in use until the next start");
        }
        else
        {
            note(MoveFileExW(legacy.c_str(), current.c_str(), 0)
                ? "moved Brizna.sqlite to HNTorrent.sqlite"
                : "could not move Brizna.sqlite (" + std::to_string(GetLastError()) + "), it stays in use");
        }
    }

    return result;
#endif
}

fs::path Environment::GetApplicationDataPath()
{
#ifndef _WIN32
    // macOS: the program is an app bundle, which must not change (and runs from a read-only
    // place until it is moved out of the download), so the data goes in the folder of the user:
    // ~/Library/Application Support/HNTorrent
    fs::path dir = GetKnownFolderPath(KnownFolder::LocalAppData) / "HNTorrent";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
#else
    if (IsInstalled() || IsAppContainerProcess())
    {
        return fs::path(GetKnownFolderPath(KnownFolder::LocalAppData)) / "HNTorrent";
    }

    return GetApplicationPath();
#endif
}

fs::path Environment::GetApplicationPath()
{
#ifndef _WIN32
    // HNTorrent.app/Contents/MacOS
    return fs::path(wxStandardPaths::Get().GetExecutablePath().utf8_string()).parent_path();
#else
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, ARRAYSIZE(path));
    PathRemoveFileSpec(path);

    return path;
#endif
}

fs::path Environment::GetCoreDbFilePath()
{
#ifndef _WIN32
    // HNTorrent.app/Contents/Resources, where the build puts it
    return fs::path(wxStandardPaths::Get().GetResourcesDir().utf8_string()) / "coredb.sqlite";
#else
    return GetApplicationPath() / "coredb.sqlite";
#endif
}

std::string Environment::GetCurrentLocale()
{
#ifndef _WIN32
    // macOS: the first preferred language in System Settings, for example "es-419" or "es-HN"
    // (the translator takes the closest one it has)
    auto preferred = wxUILocale::GetPreferredUILanguages();

    if (!preferred.empty() && !preferred[0].empty())
    {
        return preferred[0].utf8_string();
    }

    wxString tag = wxUILocale::GetSystemLocaleId().GetTag(wxLOCALE_TAGTYPE_BCP47);

    if (tag.empty())
    {
        BOOST_LOG_TRIVIAL(error) << "Failed to get current locale - defaulting to 'en'";
        return "en";
    }

    return tag.utf8_string();
#else
    TCHAR loc[512];
    int res = GetUserDefaultLocaleName(loc, 512);

    if (res == 0)
    {
        BOOST_LOG_TRIVIAL(error) << "Failed to get current locale - defaulting to 'en'";
        return "en";
    }

    return Utils::toStdString(
        std::wstring(loc, res));
#endif
}

fs::path Environment::GetDatabaseFilePath()
{
    fs::path current = GetApplicationDataPath() / "HNTorrent.sqlite";
    fs::path legacy = GetApplicationDataPath() / "Brizna.sqlite";
    std::error_code ec;

    // While Brizna.sqlite could not be moved (see MigrateFromBrizna), it is the one in use
    if (!fs::exists(current, ec) && fs::exists(legacy, ec))
    {
        return legacy;
    }

    return current;
}

fs::path Environment::GetKnownFolderPath(Environment::KnownFolder knownFolder)
{
#ifndef _WIN32
    switch (knownFolder)
    {
    case KnownFolder::LocalAppData:
        return fs::path(wxFileName::GetHomeDir().utf8_string()) / "Library" / "Application Support";

    case KnownFolder::UserDownloads:
        return fs::path(wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Downloads).utf8_string());

    default:
        BOOST_LOG_TRIVIAL(fatal) << "Unknown KnownFolder specified: " << static_cast<int>(knownFolder);
        throw std::runtime_error("Unknown folder");
    }
#else
    KNOWNFOLDERID fid = { 0 };

    switch (knownFolder)
    {
    case KnownFolder::LocalAppData:
        fid = FOLDERID_LocalAppData;
        break;

    case KnownFolder::UserDownloads:
        fid = FOLDERID_Downloads;
        break;

    default:
        BOOST_LOG_TRIVIAL(fatal) << "Unknown KnownFolder specified: " << static_cast<int>(knownFolder);
        throw std::runtime_error("Unknown folder");
    }

    PWSTR result;
    HRESULT hr = SHGetKnownFolderPath(fid, 0, nullptr, &result);

    if (SUCCEEDED(hr))
    {
        fs::path p(result);
        CoTaskMemFree(result);
        return p;
    }

    BOOST_LOG_TRIVIAL(fatal) << "Failed to get KnownFolder: " << static_cast<int>(knownFolder);

    throw std::runtime_error("Could not get known folder path");
#endif
}

fs::path Environment::GetLogFilePath()
{
    std::time_t tim = std::chrono::system_clock::to_time_t(m_startupTime);
    tm t;
#ifdef _WIN32
    localtime_s(&t, &tim);
#else
    localtime_r(&tim, &t);
#endif

    char frmt[100] = { 0 };
    snprintf(frmt,
#ifdef _WIN32
        ARRAYSIZE(frmt),
#else
        sizeof(frmt),
#endif
        "HNTorrent.%d%02d%02d%02d%02d%02d.log",
        t.tm_year + 1900,
        t.tm_mon + 1,
        t.tm_mday,
        t.tm_hour,
        t.tm_min,
        t.tm_sec);

    return GetApplicationDataPath() / "logs" / frmt;
}

bool Environment::IsAppContainerProcess()
{
#ifndef _WIN32
    // An MSIX package: Windows only
    return false;
#else
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, ARRAYSIZE(path));
    PathRemoveFileSpec(path);
    PathCombine(path, path, TEXT("appx.dummy"));
    DWORD dwAttr = GetFileAttributes(path);

    return (dwAttr != INVALID_FILE_ATTRIBUTES && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY));
#endif
}

bool Environment::IsInstalled()
{
#ifndef _WIN32
    // The installer (and its registry key) is Windows only
    return false;
#else
    HKEY hKey = NULL;

    LSTATUS lStatus = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        TEXT("Software\\HNTorrent"),
        0,
        KEY_READ,
        &hKey);

    if (lStatus != ERROR_SUCCESS)
    {
        if (hKey != NULL) { RegCloseKey(hKey); }
        return false;
    }

    TCHAR installDirectory[MAX_PATH];
    DWORD bufSize = MAX_PATH;

    lStatus = RegQueryValueEx(
        hKey,
        L"InstallDirectory",
        NULL,
        NULL,
        (LPBYTE)installDirectory,
        &bufSize);

    if (lStatus != ERROR_SUCCESS)
    {
        if (hKey != NULL) { RegCloseKey(hKey); }
        return false;
    }

    TCHAR currentLocation[MAX_PATH];
    GetModuleFileName(NULL, currentLocation, ARRAYSIZE(currentLocation));

    TCHAR installedFile[MAX_PATH];
    PathCombine(installedFile, installDirectory, TEXT("HNTorrent.exe"));

    if (StrCmp(currentLocation, installedFile) == 0)
    {
        if (hKey != NULL) { RegCloseKey(hKey); }
        return true;
    }

    if (hKey != NULL) { RegCloseKey(hKey); }

    return false;
#endif
}
