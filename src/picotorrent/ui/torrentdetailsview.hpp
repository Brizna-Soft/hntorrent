#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <map>
#include <memory>

#include <libtorrent/info_hash.hpp>

class wxSimplebook;

namespace pt::BitTorrent { class TorrentHandle; }
namespace pt::Core { class Configuration; }

namespace pt::UI
{
    class TorrentDetailsFilesPanel;
    class TorrentDetailsOverviewPanel;
    class TorrentDetailsPeersPanel;
    class TorrentDetailsTrackersPanel;

    // The panel under the torrent list: a row of plain tabs (General, Files, Peers,
    // Trackers) above the selected page.
    class TorrentDetailsView : public wxPanel
    {
    public:
        TorrentDetailsView(wxWindow* parent, wxWindowID id, std::shared_ptr<Core::Configuration> cfg);
        virtual ~TorrentDetailsView();

        void Refresh(std::map<libtorrent::info_hash_t, BitTorrent::TorrentHandle*> const& torrents);
        void ReloadConfiguration();
        void Reset();

    private:
        std::shared_ptr<Core::Configuration> m_cfg;

        wxSimplebook* m_book;
        TorrentDetailsOverviewPanel* m_overview;
        TorrentDetailsFilesPanel* m_files;
        TorrentDetailsPeersPanel* m_peers;
        TorrentDetailsTrackersPanel* m_trackers;
    };
}
