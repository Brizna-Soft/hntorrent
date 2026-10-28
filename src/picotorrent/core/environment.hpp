#pragma once

#include <chrono>
#include <filesystem>

namespace pt
{
namespace Core
{
    class Environment
    {
    public:
        enum class KnownFolder
        {
            LocalAppData,
            UserDownloads
        };

        static std::shared_ptr<Environment> Create();

        std::filesystem::path GetApplicationDataPath();
        std::filesystem::path GetApplicationPath();
        std::filesystem::path GetCoreDbFilePath();
        std::string GetCurrentLocale();
        std::filesystem::path GetDatabaseFilePath();
        std::filesystem::path GetKnownFolderPath(KnownFolder knownFolder);
        std::filesystem::path GetLogFilePath();
        bool IsAppContainerProcess();
        bool IsInstalled();

    private:
        Environment();

        // Moves the data of Brizna (the name of this program until 0.26) to the new names.
        // Returns what was done, for the log.
        std::string MigrateFromBrizna();

        std::chrono::system_clock::time_point m_startupTime;
    };
}
}
