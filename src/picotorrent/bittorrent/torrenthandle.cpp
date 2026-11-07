#include "torrenthandle.hpp"

#include <set>
#include <string>
#include <vector>

#include <windows.h>
#include <shlobj.h>

#include <libtorrent/torrent_info.hpp>

#include "../core/utils.hpp"

#include <libtorrent/announce_entry.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include "session.hpp"
#include "torrentstatus.hpp"

namespace lt = libtorrent;
using pt::BitTorrent::TorrentHandle;
using pt::BitTorrent::TorrentStatus;

TorrentStatus::State getTorrentStatusState(lt::torrent_status const& ts)
{
    bool paused = ((ts.flags & lt::torrent_flags::paused)
        && !(ts.flags & lt::torrent_flags::auto_managed));

    bool seeding = (ts.state == lt::torrent_status::state_t::seeding
        || ts.state == lt::torrent_status::state_t::finished);

    bool queued = ((ts.flags & lt::torrent_flags::paused)
        && (ts.flags & lt::torrent_flags::auto_managed));

    bool checking = (ts.state == lt::torrent_status::state_t::checking_files
        || ts.state == lt::torrent_status::state_t::checking_resume_data);

    if (ts.errc)
    {
        return TorrentStatus::State::Error;
    }

    if (paused)
    {
        if (seeding)
        {
            return TorrentStatus::State::UploadingPaused;
        }

        return TorrentStatus::State::DownloadingPaused;
    }
    else
    {
        if (queued && !checking)
        {
            return seeding
                ? TorrentStatus::State::UploadingQueued
                : TorrentStatus::State::DownloadingQueued;
        }
        else
        {
            switch (ts.state)
            {
            case lt::torrent_status::state_t::finished:
            case lt::torrent_status::state_t::seeding:
                return paused
                    ? TorrentStatus::State::UploadingPaused
                    : TorrentStatus::State::Uploading;

            case lt::torrent_status::state_t::checking_resume_data:
                return TorrentStatus::State::CheckingResumeData;

            case lt::torrent_status::state_t::checking_files:
                return TorrentStatus::State::DownloadingChecking;

            case lt::torrent_status::state_t::downloading_metadata:
                return TorrentStatus::State::DownloadingMetadata;

            case lt::torrent_status::state_t::downloading:
                return TorrentStatus::State::Downloading;
            }
        }
    }

    return TorrentStatus::State::Unknown;
}

TorrentHandle::TorrentHandle(pt::BitTorrent::Session* session, lt::torrent_handle const& th)
    : m_session(session)
{
    m_th = std::make_unique<lt::torrent_handle>(th);
    m_status = Update(th.status());
}

TorrentHandle::~TorrentHandle()
{
}

void TorrentHandle::AddTracker(lt::announce_entry const& entry)
{
    m_th->add_tracker(entry);
}

void TorrentHandle::FileProgress(std::vector<std::int64_t>& progress, int flags) const
{
    m_th->file_progress(
        progress,
        flags >= 1
            ? lt::torrent_handle::piece_granularity
            : lt::file_progress_flags_t{});
}

void TorrentHandle::ForceReannounce()
{
    m_th->force_reannounce();
}

void TorrentHandle::ForceReannounce(int seconds, int trackerIndex)
{
    m_th->force_reannounce(seconds, trackerIndex);
}

void TorrentHandle::ForceRecheck()
{
    if (Status().paused)
    {
        m_session->PauseAfterRecheck(this);
        Resume();
    }

    m_th->force_recheck();
}

std::vector<lt::download_priority_t> TorrentHandle::GetFilePriorities() const
{
    return m_th->get_file_priorities();
}

void TorrentHandle::GetPeerInfo(std::vector<lt::peer_info>& peers) const
{
    m_th->get_peer_info(peers);
}

lt::info_hash_t TorrentHandle::InfoHash()
{
    return m_th->info_hashes();
}

bool TorrentHandle::IsSequentialDownload()
{
    return (m_th->flags() & lt::torrent_flags::sequential_download) == lt::torrent_flags::sequential_download;
}

bool TorrentHandle::IsValid()
{
    return m_th->is_valid();
}

void TorrentHandle::MoveStorage(std::string const& newPath)
{
    m_th->move_storage(newPath);
}

void TorrentHandle::Pause()
{
    m_th->unset_flags(lt::torrent_flags::auto_managed);
    m_th->pause(lt::torrent_handle::graceful_pause);
}

void TorrentHandle::QueueUp()
{
    m_th->queue_position_up();
}

void TorrentHandle::QueueDown()
{
    m_th->queue_position_down();
}

void TorrentHandle::QueueTop()
{
    m_th->queue_position_top();
}

void TorrentHandle::QueueBottom()
{
    m_th->queue_position_bottom();
}

void TorrentHandle::ReplaceTrackers(std::vector<lt::announce_entry> const& trackers)
{
    m_th->replace_trackers(trackers);
}

void TorrentHandle::Remove()
{
    m_session->RemoveTorrent(this);
}

// «Quitar y abrir la carpeta»: HNTorrent never deletes downloaded files. It removes the torrent from the list (the files
// stay) and opens File Explorer in its save folder with the torrent's files or folder selected, so the person decides
// whether to delete them (File Explorer sends them to the Recycle Bin).
void TorrentHandle::RemoveFiles()
{
    std::vector<std::wstring> top;
    std::wstring folder;
    if (auto ti = m_th->torrent_file())
    {
        folder = pt::Utils::toStdWString(m_th->status(lt::torrent_handle::query_save_path).save_path);
        auto const& files = ti->files();
        std::set<std::wstring> seen;
        for (lt::file_index_t i : files.file_range())
        {
            if (files.pad_file_at(i)) continue;
            std::string p = files.file_path(i);
            std::string first = p.substr(0, p.find_first_of("\\/"));
            if (first.empty() || first == "." || first == ".." || first.find(':') != std::string::npos) continue;
            std::wstring name = pt::Utils::toStdWString(first);
            if (seen.insert(name).second) top.push_back(name);
        }
    }

    m_session->RemoveTorrent(this);

    if (folder.empty()) return;
    PIDLIST_ABSOLUTE parent = ILCreateFromPathW(folder.c_str());
    if (!parent) return;
    std::vector<PIDLIST_ABSOLUTE> items;
    for (auto const& name : top)
    {
        std::wstring full = folder + L"\\" + name;
        if (GetFileAttributesW(full.c_str()) == INVALID_FILE_ATTRIBUTES) continue;   // never downloaded
        if (PIDLIST_ABSOLUTE item = ILCreateFromPathW(full.c_str())) items.push_back(item);
    }
    // Selecting only opens and marks: nothing is deleted, moved or run
    std::vector<PCUITEMID_CHILD> children;
    for (auto item : items) children.push_back(ILFindLastID(item));
    SHOpenFolderAndSelectItems(parent, (UINT)children.size(), children.empty() ? nullptr : children.data(), 0);
    for (auto item : items) ILFree(item);
    ILFree(parent);
}

void TorrentHandle::Resume()
{
    m_th->set_flags(lt::torrent_flags::auto_managed);
    m_th->clear_error();
    m_th->resume();
}

void TorrentHandle::ResumeForce()
{
    m_th->unset_flags(lt::torrent_flags::auto_managed);
    m_th->clear_error();
    m_th->resume();
}

void TorrentHandle::ScrapeTracker(int trackerIndex)
{
    m_th->scrape_tracker(trackerIndex);
}

void TorrentHandle::SetFilePriorities(std::vector<lt::download_priority_t> priorities)
{
    m_th->prioritize_files(priorities);
}

void TorrentHandle::SetFilePriority(lt::file_index_t index, lt::download_priority_t priority)
{
    m_th->file_priority(index, priority);
}

void TorrentHandle::SetSequentialDownload(bool seq)
{
    if (seq)
    {
        m_th->set_flags(lt::torrent_flags::sequential_download);
    }
    else
    {
        m_th->unset_flags(lt::torrent_flags::sequential_download);
    }
}

TorrentStatus TorrentHandle::Status() const
{
    return *m_status.get();
}

std::vector<lt::announce_entry> TorrentHandle::Trackers() const
{
    return m_th->trackers();
}

void TorrentHandle::ClearLabel()
{
    m_labelId = -1;
    m_session->UpdateTorrentLabel(this);
}

int TorrentHandle::Label()
{
    return m_labelId;
}

void TorrentHandle::BuildStatus(libtorrent::torrent_status const& ts)
{
    m_status = Update(ts);
}

void TorrentHandle::SetLabel(int id, std::string const& name, bool muted)
{
    m_labelId = id;
    m_labelName = name;

    if (!muted)
    {
        m_session->UpdateTorrentLabel(this);
    }
}

std::unique_ptr<TorrentStatus> TorrentHandle::Update(lt::torrent_status const& ts)
{
    std::stringstream hash;

    if (ts.info_hashes.has_v2())
    {
        hash << ts.info_hashes.v2;
    }
    else
    {
        hash << ts.info_hashes.v1;
    }

    if (!m_th->is_valid())
    {
        TorrentStatus invalid;
        invalid.infoHash = hash.str();
        return std::make_unique<TorrentStatus>(invalid);
    }

    auto eta = std::chrono::seconds(0);

    if ((ts.total_wanted - ts.total_wanted_done > 0) && ts.download_payload_rate > 0)
    {
        eta = std::chrono::seconds((ts.total_wanted - ts.total_wanted_done) / ts.download_payload_rate);
    }

    float ratio = 0;
    auto ti = ts.torrent_file.lock();

    if (ti && ts.all_time_upload > 0)
    {
        ratio = static_cast<float>(ts.all_time_upload) / static_cast<float>(ti->total_size());
    }

    std::string error;
    std::string error_details;

    if (ts.errc)
    {
        error = ts.errc.message();

        // If we have an error in a file, get file info

        if (ts.error_file >= lt::file_index_t{ 0 } && ti)
        {
            error_details = ti->files().file_path(ts.error_file);
        }
    }

    TorrentStatus nts;
    nts.addedOn = wxDateTime(ts.added_time);
    nts.allTimeDownload = ts.all_time_download;
    nts.allTimeUpload = ts.all_time_upload;
    nts.availability = ts.distributed_copies;
    nts.completedOn = ts.completed_time > 0 ? wxDateTime(ts.completed_time) : wxDateTime();
    nts.downloadPayloadRate = ts.download_payload_rate;
    nts.error = error;
    nts.errorDetails = error_details;
    nts.eta = eta;
    nts.forced = (!(ts.flags & lt::torrent_flags::paused) && !(ts.flags & lt::torrent_flags::auto_managed));
    nts.infoHash = hash.str();
    nts.labelName = m_labelName;
    nts.lastDownload = ts.last_download.time_since_epoch().count() > 0 ? std::chrono::seconds(lt::total_seconds(lt::clock_type::now() - ts.last_download)) : std::chrono::seconds(-1);
    nts.lastUpload = ts.last_upload.time_since_epoch().count() > 0 ? std::chrono::seconds(lt::total_seconds(lt::clock_type::now() - ts.last_upload)) : std::chrono::seconds(-1);
    nts.name = ts.name.empty() ? nts.infoHash : ts.name;
    nts.paused = (m_th->flags() & lt::torrent_flags::paused) == lt::torrent_flags::paused;
    nts.peersCurrent = ts.num_peers - ts.num_seeds;
    nts.peersTotal = ts.list_peers - ts.list_seeds;
    nts.pieces = ts.pieces;
    nts.progress = ts.progress;
    nts.queuePosition = static_cast<int>(ts.queue_position);
    nts.ratio = ratio;
    nts.savePath = ts.save_path;
    nts.seedsCurrent = ts.num_seeds;
    nts.seedsTotal = ts.list_seeds;
    nts.state = getTorrentStatusState(ts);
    nts.torrentFile = ts.torrent_file;
    nts.totalWanted = ts.total_wanted;
    nts.totalWantedRemaining = ts.total_wanted - ts.total_wanted_done;
    nts.uploadPayloadRate = ts.upload_payload_rate;

    return std::make_unique<TorrentStatus>(nts);
}

lt::torrent_handle& TorrentHandle::WrappedHandle()
{
    return *m_th.get();
}
