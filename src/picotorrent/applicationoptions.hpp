#pragma once

#include <string>
#include <vector>

namespace pt
{
    struct CommandLineOptions
    {
        CommandLineOptions() : pid(-1), silent(false), justUpdated(false), reverted(false) {}
        long pid;
        bool silent;
        bool justUpdated;   // --recien-actualizado: started by the updater right after replacing the files
        bool reverted;      // --actualizacion-revertida: the new version did not start and this one came back
        std::vector<std::string> files;
        std::vector<std::string> magnets;
        std::string save_path;
    };
}
