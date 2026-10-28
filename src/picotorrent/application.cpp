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
    }
}

Application::Application()
    : wxApp(),
    m_singleInstance(std::make_unique<wxSingleInstanceChecker>(SingleInstanceName))
{
    SetProcessDPIAware();
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
        m_singleInstance = std::make_unique<wxSingleInstanceChecker>(SingleInstanceName);
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
    
    // Load theme
    if (cfg->IsDarkMode())
    {
        wxApp::MSWEnableDarkMode();
    }

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
            mainFrame->MSWGetTaskBarButton()->Hide();
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
        "HNTorrent",
        "ApplicationOptions");

    if (conn)
    {
        conn->Execute(j.dump());
        conn->Disconnect();
    }
}

void Application::WaitForPreviousInstance(long pid)
{
    HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProc == NULL) { return; }
    // After an update the old version may need a while to save the state of every torrent
    WaitForSingleObject(hProc, 120000);
    CloseHandle(hProc);
}
