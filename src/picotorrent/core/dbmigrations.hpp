#pragma once

#include <cstddef>

// macOS only: the SQL files of res/dbmigrations, compiled into the program by CMake (see
// CMakeLists.txt). On Windows they are the DBMIGRATION resources of resources.rc. The names
// are in capitals and sorted, as Windows returns the resource names.
namespace pt::Core::DbMigrations
{
    struct Entry
    {
        const char* name;
        const unsigned char* sql;
        std::size_t size;
    };

    extern const Entry Entries[];
    extern const std::size_t Count;
}
