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

        void Ask(Manifest const& manifest, std::string const& baseUrl);
        void Install(std::filesystem::path const& staging);
        void ShowError(std::wstring const& message);

        wxFrame* m_frame;
        std::shared_ptr<Core::Configuration> m_cfg;
        std::shared_ptr<Core::Environment> m_env;
        std::shared_ptr<State> m_state;
    };
}
