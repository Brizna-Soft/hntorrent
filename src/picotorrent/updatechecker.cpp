#include "updatechecker.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <regex>
#include <set>
#include <thread>
#include <vector>

#include <Windows.h>
#include <winhttp.h>

#include <boost/log/trivial.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <wx/mstream.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "buildinfo.hpp"
#include "core/configuration.hpp"
#include "core/environment.hpp"
#include "core/utils.hpp"
#include "ui/translator.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using pt::UpdateChecker;

namespace
{
    // Public half of the key that signs every Brizna release. The private half is the
    // BRIZNA_FIRMA_PRIVADA secret of the private repository (see clave-de-firma/LEEME-CLAVE.txt).
    const std::array<unsigned char, 32> SigningKey =
    {
        0xc7, 0x62, 0xe2, 0x86, 0xee, 0x2c, 0xc2, 0x4f, 0x63, 0x21, 0x74, 0xaf, 0xfc, 0xa9, 0x25, 0x14,
        0x99, 0x2b, 0xf9, 0x10, 0xd1, 0x77, 0x4a, 0x0b, 0x89, 0x42, 0x39, 0xed, 0xf0, 0x87, 0x6b, 0x3f,
    };

    // The only files an update may replace
    const std::array<const char*, 3> PackageFiles = { "Brizna.exe", "coredb.sqlite", "AVISOS-DE-TERCEROS.txt" };

    const char* StagingFolder = "actualizacion-pendiente";
    const wchar_t* BackupSuffix = L".anterior";
    const size_t MaxManifestSize = 64 * 1024;
    const size_t MaxPackageSize = 200 * 1024 * 1024;
    const size_t MaxEntrySize = 150 * 1024 * 1024;
    const int MaxRedirects = 5;

    struct InternetHandle
    {
        explicit InternetHandle(HINTERNET h) : handle(h) {}
        ~InternetHandle() { if (handle != nullptr) { WinHttpCloseHandle(handle); } }
        InternetHandle(InternetHandle const&) = delete;
        InternetHandle& operator=(InternetHandle const&) = delete;

        HINTERNET handle;
    };

    // Releases live on github.com and are served from *.githubusercontent.com
    bool IsAllowedHost(std::wstring host)
    {
        std::transform(host.begin(), host.end(), host.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

        const std::wstring cdn = L".githubusercontent.com";

        return host == L"github.com"
            || (host.size() > cdn.size() && host.compare(host.size() - cdn.size(), cdn.size(), cdn) == 0);
    }

    std::wstring QueryHeader(HINTERNET request, DWORD header)
    {
        DWORD size = 0;
        WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);

        if (size == 0) { return L""; }

        std::wstring value(size / sizeof(wchar_t), L'\0');

        if (!WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, &value[0], &size, WINHTTP_NO_HEADER_INDEX))
        {
            return L"";
        }

        value.resize(size / sizeof(wchar_t));
        return value;
    }

    // Blocking HTTPS GET. Redirects are followed by hand, and only to GitHub hosts over HTTPS.
    bool HttpsGet(std::string const& url, size_t maxSize, std::string& body, std::string& error)
    {
        InternetHandle session(WinHttpOpen(L"Brizna/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));

        if (session.handle == nullptr)
        {
            error = fmt::format("WinHttpOpen failed ({})", GetLastError());
            return false;
        }

        WinHttpSetTimeouts(session.handle, 15000, 15000, 30000, 60000);

        std::wstring current = pt::Utils::toStdWString(url);

        for (int hop = 0; hop <= MaxRedirects; hop++)
        {
            URL_COMPONENTS uc = { sizeof(URL_COMPONENTS) };
            uc.dwHostNameLength = DWORD(-1);
            uc.dwUrlPathLength = DWORD(-1);
            uc.dwExtraInfoLength = DWORD(-1);

            if (!WinHttpCrackUrl(current.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS)
            {
                error = "only https addresses are allowed";
                return false;
            }

            std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
            std::wstring path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) + std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);

            if (!IsAllowedHost(host))
            {
                error = "unexpected host " + pt::Utils::toStdString(host);
                return false;
            }

            InternetHandle connect(WinHttpConnect(session.handle, host.c_str(), uc.nPort, 0));
            InternetHandle request(connect.handle != nullptr
                ? WinHttpOpenRequest(connect.handle, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                : nullptr);

            DWORD disableRedirects = WINHTTP_DISABLE_REDIRECTS;

            if (request.handle == nullptr
                || !WinHttpSetOption(request.handle, WINHTTP_OPTION_DISABLE_FEATURE, &disableRedirects, sizeof(disableRedirects))
                || !WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                || !WinHttpReceiveResponse(request.handle, nullptr))
            {
                error = fmt::format("request failed ({})", GetLastError());
                return false;
            }

            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
            {
                std::wstring location = QueryHeader(request.handle, WINHTTP_QUERY_LOCATION);

                if (location.empty())
                {
                    error = "redirect without a location";
                    return false;
                }

                // A path on the same host; anything else must be a full https address,
                // which the next round checks again (scheme and host)
                current = location[0] == L'/' ? L"https://" + host + location : location;
                continue;
            }

            if (status != 200)
            {
                error = fmt::format("HTTP {}", status);
                return false;
            }

            body.clear();

            for (;;)
            {
                DWORD available = 0;

                if (!WinHttpQueryDataAvailable(request.handle, &available))
                {
                    error = fmt::format("read failed ({})", GetLastError());
                    return false;
                }

                if (available == 0) { return true; }

                if (body.size() + available > maxSize)
                {
                    error = "response too large";
                    return false;
                }

                size_t offset = body.size();
                body.resize(offset + available);

                DWORD read = 0;

                if (!WinHttpReadData(request.handle, &body[offset], available, &read))
                {
                    error = fmt::format("read failed ({})", GetLastError());
                    return false;
                }

                body.resize(offset + read);
            }
        }

        error = "too many redirects";
        return false;
    }

    std::string ToHex(unsigned char const* data, size_t length)
    {
        static const char* digits = "0123456789abcdef";
        std::string hex;
        hex.reserve(length * 2);

        for (size_t i = 0; i < length; i++)
        {
            hex += digits[data[i] >> 4];
            hex += digits[data[i] & 0x0f];
        }

        return hex;
    }

    bool FromHex(std::string const& hex, std::vector<unsigned char>& bytes)
    {
        if (hex.size() % 2 != 0) { return false; }

        auto nibble = [](char c) -> int
        {
            if (c >= '0' && c <= '9') { return c - '0'; }
            if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
            if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
            return -1;
        };

        bytes.clear();

        for (size_t i = 0; i < hex.size(); i += 2)
        {
            int hi = nibble(hex[i]);
            int lo = nibble(hex[i + 1]);
            if (hi < 0 || lo < 0) { return false; }
            bytes.push_back(static_cast<unsigned char>((hi << 4) | lo));
        }

        return true;
    }

    std::string Sha256Hex(std::string const& data)
    {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int length = 0;

        if (EVP_Digest(data.data(), data.size(), digest, &length, EVP_sha256(), nullptr) != 1)
        {
            return "";
        }

        return ToHex(digest, length);
    }

    // The signature covers the version and the hash of the package together, so an
    // older (signed) package cannot be presented as a newer version.
    bool VerifySignature(std::string const& version, std::string const& sha256, std::string const& signatureHex)
    {
        std::vector<unsigned char> signature;

        if (!FromHex(signatureHex, signature) || signature.size() != 64)
        {
            return false;
        }

        std::string message = "brizna-actualizacion-v1\nversion=" + version + "\nsha256=" + sha256 + "\n";

        EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, SigningKey.data(), SigningKey.size());
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();

        bool valid = key != nullptr
            && ctx != nullptr
            && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1
            && EVP_DigestVerify(ctx, signature.data(), signature.size(), reinterpret_cast<unsigned char const*>(message.data()), message.size()) == 1;

        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);

        return valid;
    }

    bool ParseVersion(std::string const& text, std::array<int, 3>& parts)
    {
        static const std::regex pattern("^(\\d{1,5})\\.(\\d{1,5})\\.(\\d{1,5})$");
        std::smatch match;

        if (!std::regex_match(text, match, pattern)) { return false; }

        for (size_t i = 0; i < 3; i++) { parts[i] = std::stoi(match[i + 1].str()); }

        return true;
    }

    // Writes exactly one copy of each known file into the staging folder, with size limits
    bool ExtractPackage(std::string const& zip, fs::path const& staging, std::string& error)
    {
        std::error_code ec;
        fs::remove_all(staging, ec);
        fs::create_directories(staging, ec);

        if (ec)
        {
            error = "cannot create " + staging.string();
            return false;
        }

        wxMemoryInputStream input(zip.data(), zip.size());
        wxZipInputStream archive(input);
        std::unique_ptr<wxZipEntry> entry;
        std::set<std::string> extracted;
        std::vector<char> buffer(64 * 1024);
        size_t packageTotal = 0;

        for (entry.reset(archive.GetNextEntry()); entry != nullptr; entry.reset(archive.GetNextEntry()))
        {
            std::string name = entry->GetName(wxPATH_UNIX).ToStdString();

            // Only the known files, by exact name: nothing with folders or other names is written
            if (std::find(PackageFiles.begin(), PackageFiles.end(), name) == PackageFiles.end())
            {
                continue;
            }

            if (!extracted.insert(name).second)
            {
                error = "the package has " + name + " twice";
                return false;
            }

            wxFFileOutputStream output((staging / name).wstring());

            if (!output.IsOk())
            {
                error = "cannot write " + name;
                return false;
            }

            size_t total = 0;

            for (;;)
            {
                archive.Read(buffer.data(), buffer.size());
                size_t read = archive.LastRead();

                if (read == 0) { break; }

                total += read;
                packageTotal += read;

                if (total > MaxEntrySize || packageTotal > MaxPackageSize)
                {
                    error = name + " is too large";
                    return false;
                }

                if (!output.WriteAll(buffer.data(), read))
                {
                    error = "cannot write " + name;
                    return false;
                }
            }

            // A CRC mismatch shows up as a read error at the end of the entry
            if (archive.GetLastError() != wxSTREAM_EOF && archive.GetLastError() != wxSTREAM_NO_ERROR)
            {
                error = "cannot extract " + name;
                return false;
            }
        }

        // A damaged central directory or entry shows up as a read error when the entries run out
        if (archive.GetLastError() == wxSTREAM_READ_ERROR || input.GetLastError() == wxSTREAM_READ_ERROR)
        {
            error = "the package is damaged";
            return false;
        }

        if (extracted.size() != PackageFiles.size())
        {
            error = "the package is incomplete";
            return false;
        }

        return true;
    }
}

UpdateChecker::UpdateChecker(wxFrame* frame, std::shared_ptr<pt::Core::Configuration> cfg, std::shared_ptr<pt::Core::Environment> env)
    : m_frame(frame),
    m_cfg(cfg),
    m_env(env),
    m_state(std::make_shared<State>())
{
}

UpdateChecker::~UpdateChecker()
{
    std::lock_guard<std::mutex> guard(m_state->lock);
    m_state->alive = false;
}

void UpdateChecker::PostToUi(std::shared_ptr<State> const& state, std::function<void()> fn)
{
    std::lock_guard<std::mutex> guard(state->lock);

    if (state->alive)
    {
        wxTheApp->CallAfter(
            [state, fn]()
            {
                // Runs on the UI thread, where the checker is destroyed, so no lock is needed here
                if (state->alive) { fn(); }
            });
    }
}

void UpdateChecker::CleanUp(fs::path const& appDir)
{
    std::error_code ec;
    fs::remove_all(appDir / StagingFolder, ec);
}

void UpdateChecker::Check(bool userInitiated)
{
    if (m_state->busy.exchange(true))
    {
        return;
    }

    std::string baseUrl = m_cfg->Get<std::string>("update_checks.url").value_or("");
    std::string current = pt::BuildInfo::version();
    auto state = m_state;

    std::thread(
        [this, state, baseUrl, current, userInitiated]()
        {
            std::string body;
            std::string error;
            Manifest manifest;
            bool newer = false;

            if (baseUrl.rfind("https://", 0) != 0)
            {
                error = "no update address is configured";
            }
            else if (HttpsGet(baseUrl + "/releases/latest/download/actualizacion.json", MaxManifestSize, body, error))
            {
                try
                {
                    json j = json::parse(body);
                    manifest.version = j.at("version").get<std::string>();
                    manifest.sha256 = j.at("sha256").get<std::string>();
                    manifest.signature = j.at("firma").get<std::string>();
                }
                catch (std::exception const& ex)
                {
                    error = std::string("invalid manifest: ") + ex.what();
                }

                std::array<int, 3> available{};
                std::array<int, 3> installed{};

                if (error.empty() && (!ParseVersion(manifest.version, available) || !ParseVersion(current, installed)))
                {
                    error = "invalid version number";
                }
                else if (error.empty() && !VerifySignature(manifest.version, manifest.sha256, manifest.signature))
                {
                    error = "bad signature";
                }
                else if (error.empty())
                {
                    newer = available > installed;
                }
            }

            BOOST_LOG_TRIVIAL(info) << "Update check: current " << current << ", available " << manifest.version
                << (error.empty() ? "" : ", error: " + error);

            PostToUi(state,
                [this, state, manifest, baseUrl, error, newer, userInitiated]()
                {
                    if (!error.empty())
                    {
                        state->busy = false;

                        if (error == "bad signature")
                        {
                            ShowError(i18n("update_bad_signature"));
                        }
                        else if (userInitiated)
                        {
                            ShowError(fmt::format(i18n("update_failed_s"), Utils::toStdWString(error)));
                        }

                        return;
                    }

                    if (!newer)
                    {
                        state->busy = false;

                        if (userInitiated)
                        {
                            wxMessageBox(
                                fmt::format(i18n("update_up_to_date_s"), Utils::toStdWString(pt::BuildInfo::version())),
                                i18n("update_title"),
                                wxOK | wxICON_INFORMATION,
                                m_frame);
                        }

                        return;
                    }

                    Ask(manifest, baseUrl);
                });
        }).detach();
}

void UpdateChecker::Ask(Manifest const& manifest, std::string const& baseUrl)
{
    wxMessageDialog dlg(
        m_frame,
        fmt::format(i18n("update_available_s"), Utils::toStdWString(manifest.version), Utils::toStdWString(pt::BuildInfo::version())),
        i18n("update_title"),
        wxYES_NO | wxICON_INFORMATION);

    dlg.SetYesNoLabels(wxString(i18n("update_install_now")), wxString(i18n("update_not_now")));

    if (dlg.ShowModal() != wxID_YES)
    {
        m_state->busy = false;
        return;
    }

    std::string url = baseUrl + "/releases/download/v" + manifest.version + "/Brizna-" + manifest.version + "-x64.zip";
    fs::path staging = m_env->GetApplicationPath() / StagingFolder;
    auto state = m_state;

    BOOST_LOG_TRIVIAL(info) << "Downloading update from " << url;

    std::thread(
        [this, state, manifest, url, staging]()
        {
            std::string zip;
            std::string error;

            if (HttpsGet(url, MaxPackageSize, zip, error))
            {
                if (Sha256Hex(zip) != manifest.sha256)
                {
                    error = "the downloaded file does not match its signature";
                }
                else
                {
                    ExtractPackage(zip, staging, error);
                }
            }

            PostToUi(state,
                [this, state, staging, error]()
                {
                    state->busy = false;

                    if (!error.empty())
                    {
                        BOOST_LOG_TRIVIAL(error) << "Update failed: " << error;
                        ShowError(fmt::format(i18n("update_failed_s"), Utils::toStdWString(error)));
                        return;
                    }

                    Install(staging);
                });
        }).detach();
}

void UpdateChecker::Install(fs::path const& staging)
{
    fs::path exe = wxStandardPaths::Get().GetExecutablePath().ToStdWstring();
    fs::path dir = exe.parent_path();

    struct Replacement
    {
        fs::path target;
        fs::path backup;
        bool hadOriginal;
    };

    std::vector<Replacement> replacements;

    for (const char* name : PackageFiles)
    {
        fs::path target = std::string(name) == "Brizna.exe" ? exe : dir / name;
        fs::path backup = target;
        backup += BackupSuffix;
        replacements.push_back({ target, backup, fs::exists(target) });
    }

    // Puts the original files back (newest change first) and removes the new ones that had no
    // original. Returns false if something could not be put back.
    auto restore = [&replacements](size_t count) -> bool
    {
        bool restored = true;

        for (size_t i = count; i-- > 0;)
        {
            Replacement const& r = replacements[i];

            if (r.hadOriginal)
            {
                restored = (MoveFileExW(r.backup.c_str(), r.target.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE) && restored;
            }
            else if (!DeleteFileW(r.target.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
            {
                restored = false;
            }
        }

        return restored;
    };

    auto fail = [this, &restore](std::wstring const& what, size_t count)
    {
        bool restored = restore(count);
        BOOST_LOG_TRIVIAL(error) << "Update failed while installing: " << Utils::toStdString(what) << (restored ? "" : " (could not restore)");
        ShowError(fmt::format(i18n(restored ? "update_failed_s" : "update_restore_failed_s"), what));
    };

    // 1. Move the current files aside. A running executable cannot be overwritten, but it
    //    can be renamed. The previous version stays as <name>.anterior until the next update.
    for (size_t i = 0; i < replacements.size(); i++)
    {
        Replacement const& r = replacements[i];

        if (r.hadOriginal && !MoveFileExW(r.target.c_str(), r.backup.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            fail(fmt::format(L"rename {} ({})", r.target.filename().wstring(), GetLastError()), i);
            return;
        }
    }

    // 2. Put the new files in place; on any failure, go back to the previous version
    for (Replacement const& r : replacements)
    {
        fs::path source = staging / r.target.filename();

        if (r.target == exe) { source = staging / "Brizna.exe"; }

        if (!MoveFileExW(source.c_str(), r.target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        {
            fail(fmt::format(L"move {} ({})", r.target.filename().wstring(), GetLastError()), replacements.size());
            return;
        }
    }

    CleanUp(dir);

    // The new version waits for this process to exit before it starts
    std::wstring commandLine = fmt::format(L"\"{}\" --wait-for-pid={}", exe.wstring(), GetCurrentProcessId());
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
    {
        // The new version is in place; it will run the next time Brizna is opened
        ShowError(fmt::format(i18n("update_failed_s"), fmt::format(L"start ({})", GetLastError())));
        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    BOOST_LOG_TRIVIAL(info) << "Update installed, restarting";
    m_frame->Close(true);
}

void UpdateChecker::ShowError(std::wstring const& message)
{
    wxMessageBox(message, i18n("update_title"), wxOK | wxICON_ERROR, m_frame);
}
