cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
cmake_policy(SET CMP0092 NEW) # don't add /W3 as default
cmake_policy(SET CMP0115 OLD) # Source file extensions must be explicit.

set(VCPKG_OVERLAY_PORTS    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/vcpkg-overlays/ports)
set(VCPKG_OVERLAY_TRIPLETS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/vcpkg-overlays/triplets)
set(CMAKE_TOOLCHAIN_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/vendor/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

# macOS: runs on macOS 11 (Big Sur) and newer, like the libraries built by vcpkg with the
# arm64-osx and x64-osx triplets in vendor/vcpkg-overlays. It has to be set before project().
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project("PicoTorrent")

set(CMAKE_CXX_STANDARD 17)

# --------- antlr4 options
option(WITH_STATIC_CRT "" Off)
# -----------------------

# --------- wx options
set(wxBUILD_SHARED        OFF   CACHE BOOL   "" FORCE)
set(wxBUILD_COMPATIBILITY "3.2" CACHE STRING "" FORCE)
set(wxUSE_EXCEPTIONS      OFF   CACHE BOOL   "" FORCE)
# --------------------

# vendor libs
add_subdirectory(vendor/wx)

# pico components
add_subdirectory(src/pql)

# vcpkg packages
find_package(Boost                      REQUIRED COMPONENTS log)
find_package(fmt                 CONFIG REQUIRED)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
find_package(nlohmann_json       CONFIG REQUIRED)
find_package(OpenSSL                    REQUIRED)
find_package(unofficial-sqlite3  CONFIG REQUIRED)

# Generate buildinfo file
configure_file("${CMAKE_SOURCE_DIR}/src/picotorrent/buildinfo.cpp.in" "${CMAKE_SOURCE_DIR}/src/picotorrent/buildinfo.cpp" @ONLY)

if(MSVC)
# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")

# Release flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO /MAP /OPT:REF /OPT:ICF")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO /MAP /OPT:REF /OPT:ICF")
endif()

add_executable(
    PicoTorrent-coredb
    src/l10n/main
    lang/ca-ES.json
    lang/de-DE.json
    lang/el-GR.json
    lang/en-US.json
    lang/es-ES.json
    lang/fi-FI.json
    lang/fr-FR.json
    lang/hu-HU.json
    lang/hy-AM.json
    lang/id-ID.json
    lang/it-IT.json
    lang/ka-GE.json
    lang/ko-KR.json
    lang/lt-LT.json
    lang/lv-LV.json
    lang/nl-NL.json
    lang/pl-PL.json
    lang/pt-BR.json
    lang/ro-RO.json
    lang/ru-RU.json
    lang/sv-SE.json
    lang/tr-TR.json
    lang/uk-UA.json
    lang/zh-CN.json
)

target_link_libraries(
    PicoTorrent-coredb
    PRIVATE
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
)

add_executable(
    PicoTorrent
    WIN32

    src/picotorrent/application
    src/picotorrent/buildinfo
    src/picotorrent/main
    src/picotorrent/persistencemanager
    src/picotorrent/resources.rc
    src/picotorrent/updatechecker

    # API
    src/picotorrent/api/libpico

    # BitTorrent
    src/picotorrent/bittorrent/session
    src/picotorrent/bittorrent/torrenthandle

    # Core
    src/picotorrent/core/configuration
    src/picotorrent/core/database
    src/picotorrent/core/environment
    src/picotorrent/core/utils

    # Http
    src/picotorrent/http/httpclient

    # IPC
    src/picotorrent/ipc/applicationoptionsconnection
    src/picotorrent/ipc/server

    # Dialogs
    src/picotorrent/ui/dialogs/aboutdialog
    src/picotorrent/ui/dialogs/addmagnetlinkdialog
    src/picotorrent/ui/dialogs/addtrackerdialog
    src/picotorrent/ui/dialogs/addtorrentdialog
    src/picotorrent/ui/dialogs/createtorrentdialog
    src/picotorrent/ui/dialogs/listeninterfacedialog
    src/picotorrent/ui/dialogs/preferencesadvancedpage
    src/picotorrent/ui/dialogs/preferencesconnectionpage
    src/picotorrent/ui/dialogs/preferencesdialog
    src/picotorrent/ui/dialogs/preferencesdownloadspage
    src/picotorrent/ui/dialogs/preferencesgeneralpage
    src/picotorrent/ui/dialogs/preferenceslabelspage
    src/picotorrent/ui/dialogs/preferencesproxypage
    src/picotorrent/ui/dialogs/textoutputdialog

    # Filters
    src/picotorrent/ui/filters/pqltorrentfilter

    # Models
    src/picotorrent/ui/models/filestoragemodel
    src/picotorrent/ui/models/peerlistmodel
    src/picotorrent/ui/models/torrentlistmodel
    src/picotorrent/ui/models/trackerlistmodel

    # Persistence
    src/picotorrent/ui/persistence/persistenttorrentlistview

    # UI
    src/picotorrent/ui/console
    src/picotorrent/ui/mainframe
    src/picotorrent/ui/statusbar
    src/picotorrent/ui/taskbaricon
    src/picotorrent/ui/theme
    src/picotorrent/ui/torrentcontextmenu
    src/picotorrent/ui/torrentdetailsfilespanel
    src/picotorrent/ui/torrentdetailsoverviewpanel
    src/picotorrent/ui/torrentdetailspeerspanel
    src/picotorrent/ui/torrentdetailstrackerspanel
    src/picotorrent/ui/torrentdetailsview
    src/picotorrent/ui/torrentfilelistview
    src/picotorrent/ui/torrentlistview
    src/picotorrent/ui/translator

    # Widgets
    src/picotorrent/ui/widgets/pieceprogressbar

    # Win32 specific stuff
    src/picotorrent/ui/win32/openfiledialog
)

if(WIN32)
target_compile_definitions(
    PicoTorrent
    PRIVATE
    -D_UNICODE
    -D_WIN32
    -D_WIN32_WINNT=0x0A00
    -DNOMINMAX
    -DPICOJSON_USE_INT64
    -DUNICODE
    -DWIN32
    -DWIN32_LEAN_AND_MEAN

    -DLIBPICO_API_EXPORT

    -DPICO_GIT_COMMITISH=${GITVERSION_VAR_SHORTSHA}
    -DPICO_VERSION_MAJOR=${GITVERSION_VAR_VERSION_MAJOR}
    -DPICO_VERSION_MINOR=${GITVERSION_VAR_VERSION_MINOR}
    -DPICO_VERSION_PATCH=${GITVERSION_VAR_VERSION_PATCH}

    -DPICO_WX_GIT_COMMITISH=${PICO_WX_GIT_COMMITISH}
)
else()
# macOS: the same, without the Windows definitions
target_compile_definitions(
    PicoTorrent
    PRIVATE
    -DPICOJSON_USE_INT64

    -DLIBPICO_API_EXPORT

    -DPICO_GIT_COMMITISH=${GITVERSION_VAR_SHORTSHA}
    -DPICO_VERSION_MAJOR=${GITVERSION_VAR_VERSION_MAJOR}
    -DPICO_VERSION_MINOR=${GITVERSION_VAR_VERSION_MINOR}
    -DPICO_VERSION_PATCH=${GITVERSION_VAR_VERSION_PATCH}

    -DPICO_WX_GIT_COMMITISH=${PICO_WX_GIT_COMMITISH}
)
endif()

if(MSVC)
target_compile_options(PicoTorrent PRIVATE /guard:cf /W4 /WX)
endif()

target_include_directories(
    PicoTorrent
    PRIVATE

    include
)

if(WIN32)
target_link_libraries(
    PicoTorrent

    PRIVATE

    # boost
    Boost::boost
    Boost::log

    # wxWidgets
    wxcore wxbase wxpropgrid

    # Windows
    Comctl32
    crypt32
    iphlpapi
    legacy_stdio_definitions
    propsys
    shlwapi
    wininet
    winhttp

    # fmt
    fmt::fmt

    # nlohmann-json
    nlohmann_json::nlohmann_json

    # OpenSSL (update signature check)
    OpenSSL::Crypto

    # Rasterbar-libtorrent
    LibtorrentRasterbar::torrent-rasterbar

    # PQL
    PicoTorrentPQL

    unofficial::sqlite3::sqlite3
)
else()
# macOS: the same libraries, without the Windows ones. wxnet has the IPC between
# instances there (wxTCPServer over a Unix socket; on Windows it is DDE, in wxbase).
target_link_libraries(
    PicoTorrent

    PRIVATE

    # boost
    Boost::boost
    Boost::log

    # wxWidgets
    wxcore wxbase wxpropgrid wxnet

    # fmt
    fmt::fmt

    # nlohmann-json
    nlohmann_json::nlohmann_json

    # OpenSSL
    OpenSSL::Crypto

    # Rasterbar-libtorrent
    LibtorrentRasterbar::torrent-rasterbar

    # PQL
    PicoTorrentPQL

    unofficial::sqlite3::sqlite3
)
endif()

if(WIN32)
set_property(TARGET PicoTorrent PROPERTY ENABLE_EXPORTS 1)
endif()
set_property(TARGET PicoTorrent PROPERTY OUTPUT_NAME HNTorrent)

if(APPLE)
    # HNTorrent.app, with Info.plist from packaging/macos
    set_target_properties(
        PicoTorrent
        PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "HNTorrent"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.brizna-soft.hntorrent"
        MACOSX_BUNDLE_BUNDLE_VERSION "${GITVERSION_VAR_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${GITVERSION_VAR_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "HNTorrent"
        MACOSX_BUNDLE_COPYRIGHT "Brizna Soft. Basado en PicoTorrent (MIT)."
    )

    # The database migrations go inside the program, like the DBMIGRATION resources of
    # resources.rc on Windows: same order (by file name) and same names (in capitals).
    file(GLOB PICO_DB_MIGRATIONS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/res/dbmigrations/*.sql")
    list(SORT PICO_DB_MIGRATIONS)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PICO_DB_MIGRATIONS})

    set(PICO_DB_MIGRATIONS_CPP "// Generated by CMake from res/dbmigrations. Do not edit.\n#include \"${CMAKE_SOURCE_DIR}/src/picotorrent/core/dbmigrations.hpp\"\n\nnamespace pt::Core::DbMigrations\n{\n")
    set(PICO_DB_MIGRATIONS_ENTRIES "")
    set(PICO_DB_MIGRATIONS_COUNT 0)

    foreach(PICO_SQL IN LISTS PICO_DB_MIGRATIONS)
        get_filename_component(PICO_SQL_NAME "${PICO_SQL}" NAME_WE)
        string(TOUPPER "${PICO_SQL_NAME}" PICO_SQL_NAME)
        file(READ "${PICO_SQL}" PICO_SQL_HEX HEX)
        string(LENGTH "${PICO_SQL_HEX}" PICO_SQL_SIZE)
        math(EXPR PICO_SQL_SIZE "${PICO_SQL_SIZE} / 2")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," PICO_SQL_HEX "${PICO_SQL_HEX}")
        string(APPEND PICO_DB_MIGRATIONS_CPP "    static const unsigned char Sql${PICO_DB_MIGRATIONS_COUNT}[] = { ${PICO_SQL_HEX} 0x00 };\n")
        string(APPEND PICO_DB_MIGRATIONS_ENTRIES "        { \"${PICO_SQL_NAME}\", Sql${PICO_DB_MIGRATIONS_COUNT}, ${PICO_SQL_SIZE} },\n")
        math(EXPR PICO_DB_MIGRATIONS_COUNT "${PICO_DB_MIGRATIONS_COUNT} + 1")
    endforeach()

    string(APPEND PICO_DB_MIGRATIONS_CPP "\n    const Entry Entries[] =\n    {\n${PICO_DB_MIGRATIONS_ENTRIES}    };\n\n    const std::size_t Count = ${PICO_DB_MIGRATIONS_COUNT};\n}\n")
    file(WRITE "${CMAKE_BINARY_DIR}/generated/dbmigrations.cpp" "${PICO_DB_MIGRATIONS_CPP}")
    target_sources(PicoTorrent PRIVATE "${CMAKE_BINARY_DIR}/generated/dbmigrations.cpp")

    # Draws the icon of the app (HNTorrent.icns) from the logo in ui/macicons.hpp
    add_executable(HNTorrent-icono packaging/macos/icono.cpp)
    target_include_directories(
        HNTorrent-icono
        PRIVATE
        src/picotorrent
        vendor/wx/3rdparty/nanosvg/src
        vendor/wx/3rdparty/nanosvg/example
    )

    # coredb.sqlite and the icon go in HNTorrent.app/Contents/Resources
    add_custom_command(
        TARGET PicoTorrent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:PicoTorrent>/Resources"
        COMMAND PicoTorrent-coredb "${CMAKE_SOURCE_DIR}/lang" "$<TARGET_BUNDLE_CONTENT_DIR:PicoTorrent>/Resources"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/HNTorrent.iconset"
        COMMAND HNTorrent-icono "${CMAKE_BINARY_DIR}/HNTorrent.iconset"
        COMMAND iconutil -c icns "${CMAKE_BINARY_DIR}/HNTorrent.iconset" -o "$<TARGET_BUNDLE_CONTENT_DIR:PicoTorrent>/Resources/HNTorrent.icns"
    )
else()
# Generate coredb
add_custom_command(
    TARGET PicoTorrent POST_BUILD
    COMMAND PicoTorrent-coredb "${CMAKE_SOURCE_DIR}/lang" $<TARGET_FILE_DIR:PicoTorrent>
)
endif()
