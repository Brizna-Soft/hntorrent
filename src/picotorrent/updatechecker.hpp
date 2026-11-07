#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace pt
{
namespace Core
{
    class Configuration;
    class Environment;
}

    // Looks for a newer HNTorrent in the public downloads repository, checks the Ed25519
    // signature of the release and replaces the files next to the running executable.
    class UpdateChecker
    {
    public:
        UpdateChecker(wxFrame* frame, std::shared_ptr<Core::Configuration> cfg, std::shared_ptr<Core::Environment> env);
        ~UpdateChecker();

        void Check(bool userInitiated);

        // Removes the staging folder left by an update that did not finish
        static void CleanUp(std::filesystem::path const& appDir);

        // «HNTorrent.exe --probar-inicio»: before installing, the updater starts the new version like this to see if
        // Windows lets it run (Smart App Control can block programs without a digital signature). In that mode the
        // program exits right away with ProbeExitCode, before the database, the window or another running copy.
        static constexpr const wchar_t* ProbeArgument = L"--probar-inicio";
        static constexpr unsigned long ProbeExitCode = 0x48544F4B; // "HTOK"

        // «HNTorrent.exe.anterior --vigilar-actualizacion=<pid>»: right after an update, the previous version stays
        // watching the new one. If the new one ends with an error before saying it started, it puts the previous
        // files back, explains it and opens the previous version.
        static constexpr const wchar_t* GuardArgument = L"--vigilar-actualizacion=";

        // Called first thing in the Application constructor: if this process is a test start or an update guard,
        // it does that and ends the process here. Otherwise it returns and HNTorrent starts as always.
        static void RunUpdaterModes();

        // Called at the end of a normal start: lets the update guard know the new version started fine
        static void SignalStarted();

    private:
        struct Manifest
        {
            std::string version;
            std::string sha256;
            std::string signature;
        };

        // Shared with the worker threads. The UpdateChecker dies with the main frame, before
        // wxTheApp, so a worker that sees alive == true while holding the lock can post to wxTheApp.
        struct State
        {
            std::mutex lock;
            bool alive = true;
            std::atomic<bool> busy{ false };
        };

        static void PostToUi(std::shared_ptr<State> const& state, std::function<void()> fn);

        // What happened when the new version was started with --probar-inicio
        enum class Probe { Started, Blocked, Failed };
        static Probe ProbeNewVersion(std::filesystem::path const& exe, unsigned long& detail);
        static void RunGuard(unsigned long newPid);

        void Ask(Manifest const& manifest, std::string const& baseUrl);
        void Install(std::filesystem::path const& staging);
        void ShowError(std::wstring const& message);

        wxFrame* m_frame;
        std::shared_ptr<Core::Configuration> m_cfg;
        std::shared_ptr<Core::Environment> m_env;
        std::shared_ptr<State> m_state;
    };
}
