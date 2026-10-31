#include "application.hpp"

#include <boost/log/trivial.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <nlohmann/json.hpp>
#include <wx/cmdline.h>
#include <wx/ipc.h>
#include <wx/persist.h>
#include <wx/snglinst.h>
#include <wx/stdpaths.h>
#include <wx/taskbarbutton.h>

#include "api/libpico_impl.hpp"
#include "persistencemanager.hpp"
#include "updatechecker.hpp"
#include "core/configuration.hpp"
#include "core/database.hpp"
#include "core/environment.hpp"
#include "core/utils.hpp"
#include "ui/mainframe.hpp"
#include "ui/theme.hpp"
#include "ui/translator.hpp"

#ifndef _WIN32
#include <signal.h>
#include <wx/filename.h>

#include "ipc/server.hpp"
#endif

#ifdef __WXOSX__
#include <wx/translation.h>
#endif

using json = nlohmann::json;
using pt::Application;

static const char* SingleInstanceName = "eb50e8eb-2263-44f0-894b-b56aaa9c6c54";

namespace
{
    // Until 0.26 this program was called Brizna, and the updater of those versions installs
    // the new version as Brizna.exe. It renames itself to HNTorrent.exe once and starts again
    // from there (the data files move to the new names in Environment::Create).
    bool RestartAsHNTorrent(pt::CommandLineOptions const& options)
    {
#ifndef _WIN32
        // Brizna was only published for Windows
        (void)options;
        return false;
#else
        std::filesystem::path exe = wxStandardPaths::Get().GetExecutablePath().ToStdWstring();

        if (_wcsicmp(exe.filename().c_str(), L"Brizna.exe") != 0) { return false; }

        std::filesystem::path target = exe.parent_path() / "HNTorrent.exe";

        // A running executable cannot be overwritten, but it can be renamed. If this fails
        // (a read-only folder, for example), it keeps working as Brizna.exe.
        if (!MoveFileExW(exe.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            return false;
        }

        std::wstring commandLine = fmt::format(L"\"{}\" --wait-for-pid={}", target.wstring(), GetCurrentProcessId());

        if (options.silent) { commandLine += L" --silent"; }
        if (!options.save_path.empty()) { commandLine += fmt::format(L" \"--save-path={}\"", pt::Utils::toStdWString(options.save_path)); }
        for (auto const& file : options.files) { commandLine += fmt::format(L" \"{}\"", pt::Utils::toStdWString(file)); }
        for (auto const& magnet : options.magnets) { commandLine += fmt::format(L" \"{}\"", pt::Utils::toStdWString(magnet)); }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessW(target.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, target.parent_path().c_str(), &si, &pi))
        {
            // Better to keep the old name than not to start at all
            MoveFileExW(target.c_str(), exe.c_str(), 0);
            return false;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return true;
#endif
    }
}

Application::Application()
    : wxApp(),
#ifdef _WIN32
    m_singleInstance(std::make_unique<wxSingleInstanceChecker>(SingleInstanceName))
#else
    // Elsewhere the checker is a lock file, which would go in the home folder; the temporary
    // folder of the user keeps it out of sight
    m_singleInstance(std::make_unique<wxSingleInstanceChecker>(SingleInstanceName, wxFileName::GetTempDir()))
#endif
{
#ifdef _WIN32
    SetProcessDPIAware();
#endif
}

Application::~Application()
{
    for (auto plugin : m_plugins)
    {
        delete plugin;
    }
}

bool Application::OnCmdLineParsed(wxCmdLineParser& parser)
{
    long waitForPid = -1;
    wxString save_path = "";

    if (parser.Found("wait-for-pid", &waitForPid))
    {
        m_options.pid = waitForPid;
    }

    m_options.silent = parser.Found("silent");

    if (parser.Found("save-path", &save_path))
    {
        m_options.save_path = Utils::toStdString(save_path.ToStdWstring());
    }

    for (size_t i = 0; i < parser.GetParamCount(); i++)
    {
        std::string arg = Utils::toStdString(parser.GetParam(i).ToStdWstring());

        if (arg.rfind("magnet:?xt", 0) == 0)
        {
            m_options.magnets.push_back(arg);
        }
        else
        {
            m_options.files.push_back(std::filesystem::absolute(arg).string());
        }
    }

    return true;
}

bool Application::OnInit()
{
    if (!wxApp::OnInit()) { return false; }

    if (m_options.pid > 0)
    {
        WaitForPreviousInstance(m_options.pid);

        // The checker was created while the previous instance was still running, so it
        // would keep reporting it (this broke the restart after an update)
        m_singleInstance.reset();
#ifdef _WIN32
        m_singleInstance = std::make_unique<wxSingleInstanceChecker>(SingleInstanceName);
#else
        m_singleInstance = std::make_unique<wxSingleInstanceChecker>(SingleInstanceName, wxFileName::GetTempDir());
#endif
    }

    if (m_singleInstance->IsAnotherRunning())
    {
        ActivateOtherInstance();
        return false;
    }

    // The new process waits for this one to exit before it starts
    if (RestartAsHNTorrent(m_options))
    {
        return false;
    }

    auto env = pt::Core::Environment::Create();
    pt::UpdateChecker::CleanUp(env->GetApplicationPath());

    auto db = std::make_shared<pt::Core::Database>(env);


    if (!db->Migrate())
    {
        wxMessageBox(
            "Failed to run database migrations. Please check log file.",
            "HNTorrent",
            wxICON_ERROR);
        return false;
    }

    auto cfg = std::make_shared<pt::Core::Configuration>(db);

    // Load current locale
    pt::UI::Translator& translator = pt::UI::Translator::GetInstance();
    translator.LoadDatabase(env->GetCoreDbFilePath());
    translator.SetLocale(
        cfg->Get<std::string>("locale_name")
            .value_or(env->GetCurrentLocale()));

#ifdef __WXOSX__
    // On macOS wxWidgets makes some menus itself (HNTorrent, with Hide and Quit, and Window)
    // and takes their texts from its own catalog: the one of the language of HNTorrent, in
    // HNTorrent.app/Contents/Resources/<language>.lproj/wxstd.mo ("es-ES" uses es.lproj).
    // Without a catalog for the language they stay in English. Before the menus are made.
    {
        auto wxTr = new wxTranslations();
        wxTr->SetLanguage(wxString(translator.GetLocale()).BeforeFirst('-'));
        wxTr->AddStdCatalog();
        wxTranslations::Set(wxTr);
    }
#endif

    // Load theme
#ifdef __WXMSW__
    if (cfg->IsDarkMode())
    {
        wxApp::MSWEnableDarkMode();
    }
#else
    // The native controls of macOS take the same choice (the light theme can be forced)
    SetAppearance(cfg->IsDarkMode() ? Appearance::Dark : Appearance::Light);
#endif

    UI::Theme::Install(cfg->IsDarkMode());

    // Load plugins
    for (auto& p : fs::directory_iterator(env->GetApplicationPath()))
    {
        if (p.path().extension() != ".dll") { continue; }

        auto const& filename = p.path().filename().string();

        if (filename.size() < 6) { continue; }
        if (filename.substr(0, 6) != "Plugin") { continue; }

        BOOST_LOG_TRIVIAL(info) << "Loading plugin from " << p.path();

        auto plugin = API::IPlugin::Load(p, env.get(), cfg.get());

        if (plugin != nullptr)
        {
            m_plugins.push_back(plugin);
        }
    }

    // Set up persistence manager
    m_persistence = std::make_unique<PersistenceManager>(db);
    wxPersistenceManager::Set(*m_persistence);

    auto mainFrame = new UI::MainFrame(env, db, cfg, m_options);

    std::for_each(
        m_plugins.begin(),
        m_plugins.end(),
        [mainFrame](auto plugin) { plugin->EmitEvent(libpico_event_mainwnd_created, mainFrame); });

    auto windowState = static_cast<pt::Core::Configuration::WindowState>(cfg->Get<int>("start_position").value());

    switch (windowState)
    {
    case pt::Core::Configuration::WindowState::Hidden:
        // Only valid if we have a notify icon
        if (cfg->Get<bool>("show_in_notification_area").value())
        {
#ifdef __WXMSW__
            mainFrame->MSWGetTaskBarButton()->Hide();
#endif
        }
        else
        {
            mainFrame->Show(true);
        }

        break;

    case pt::Core::Configuration::WindowState::Maximized:
        mainFrame->Show(true);
        mainFrame->Maximize();
        break;

    case pt::Core::Configuration::WindowState::Minimized:
        mainFrame->Iconize();
        mainFrame->Show(true);
        break;

    case pt::Core::Configuration::WindowState::Normal:
        mainFrame->Show(true);
        break;
    }

    mainFrame->HandleParams(m_options);

    return true;
}

#ifdef __WXOSX__
void Application::MacReopenApp()
{
    for (wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext())
    {
        if (auto mainFrame = dynamic_cast<UI::MainFrame*>(node->GetData()))
        {
            mainFrame->ShowAndActivate();
            return;
        }
    }

    wxApp::MacReopenApp();
}
#endif

void Application::OnInitCmdLine(wxCmdLineParser& parser)
{
    static const wxCmdLineEntryDesc cmdLineDesc[] =
    {
        { wxCMD_LINE_OPTION, NULL, "wait-for-pid",  NULL,   wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, NULL, "silent",        NULL,   wxCMD_LINE_VAL_NONE ,  wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_OPTION, NULL, "save-path",     NULL,   wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_PARAM,  NULL, NULL,           "params",wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE },
        { wxCMD_LINE_NONE }
    };

    parser.SetDesc(cmdLineDesc);
    parser.SetSwitchChars("-");
}

void Application::ActivateOtherInstance()
{
    json j;
    j["files"] = m_options.files;
    j["magnet_links"] = m_options.magnets;
    j["silent"] = m_options.silent;
    j["save_path"] = m_options.save_path;

    wxClient client;
    auto conn = client.MakeConnection(
        "localhost",
#ifdef __WXMSW__
        "HNTorrent",
#else
        IPC::Server::SocketPath(),
#endif
        "ApplicationOptions");

    if (conn)
    {
#ifdef _WIN32
        conn->Execute(j.dump());
#else
        // Paths and links as UTF-8 (the other instance reads them back with utf8_string)
        conn->Execute(wxString::FromUTF8(j.dump()));
#endif
        conn->Disconnect();
    }
}

void Application::WaitForPreviousInstance(long pid)
{
#ifdef _WIN32
    HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProc == NULL) { return; }
    // After an update the old version may need a while to save the state of every torrent
    WaitForSingleObject(hProc, 120000);
    CloseHandle(hProc);
#else
    // Up to two minutes, like on Windows: the previous instance may need a while to save the
    // state of every torrent (kill with signal 0 only checks that the process still exists)
    for (int i = 0; i < 1200 && kill(static_cast<pid_t>(pid), 0) == 0; i++)
    {
        wxMilliSleep(100);
    }
#endif
}
