#include "torrentdetailsoverviewpanel.hpp"

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

#include <libtorrent/torrent_status.hpp>

#include "../bittorrent/torrenthandle.hpp"
#include "../bittorrent/torrentstatus.hpp"
#include "../core/utils.hpp"
#include "../core/configuration.hpp"
#include "theme.hpp"
#include "translator.hpp"
#include "widgets/pieceprogressbar.hpp"

using pt::UI::TorrentDetailsOverviewPanel;

// Labels in grey and values in the normal text colour, as in the HNTorrent design
static wxStaticText* MutedLabel(wxWindow* parent, wxWindowID id, wxString const& text)
{
    auto s = new wxStaticText(parent, id, text);
    s->SetForegroundColour(pt::UI::Theme::Colors().mutedText);
    return s;
}

std::wstring SecondsToFriendly(std::chrono::seconds secs)
{
    std::chrono::hours hours_left = std::chrono::duration_cast<std::chrono::hours>(secs);
    std::chrono::minutes min_left = std::chrono::duration_cast<std::chrono::minutes>(secs - hours_left);
    std::chrono::seconds sec_left = std::chrono::duration_cast<std::chrono::seconds>(secs - hours_left - min_left);

    if (hours_left.count() <= 0)
    {
        if (min_left.count() <= 0)
        {
            return fmt::format(
                i18n("eta_s_format"),
                sec_left.count());
        }

        return fmt::format(
            i18n("eta_ms_format"),
            min_left.count(),
            sec_left.count());
    }

    return fmt::format(
        i18n("eta_hms_format"),
        hours_left.count(),
        min_left.count(),
        sec_left.count());
}

class CopyableStaticText : public wxStaticText
{
public:
    CopyableStaticText(wxWindow* parent)
        : wxStaticText(parent, wxID_ANY, "-", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE)
    {
        this->Bind(wxEVT_RIGHT_DOWN,
            [this](wxMouseEvent const&)
            {
                if (this->GetLabel() == "-")
                {
                    return;
                }

                wxMenu menu;
                menu.Append(9999, i18n("copy"));
                menu.Bind(wxEVT_MENU,
                    [this](wxCommandEvent const&)
                    {
                        if (wxTheClipboard->Open())
                        {
                            wxTheClipboard->SetData(new wxTextDataObject(this->GetLabel()));
                            wxTheClipboard->Close();
                        }
                    });

                PopupMenu(&menu);
            });
    }
};

TorrentDetailsOverviewPanel::TorrentDetailsOverviewPanel(wxWindow* parent, wxWindowID id, bool isDarkMode, int cols, bool showPieceProgress)
    : wxScrolledWindow(parent, id),
    m_pieceProgress(nullptr),
    m_name(new CopyableStaticText(this)),
    m_isDarkMode(isDarkMode),
    m_infoHash(new CopyableStaticText(this)),
    m_savePath(new CopyableStaticText(this)),
    m_pieces(new CopyableStaticText(this)),
    m_comment(new CopyableStaticText(this)),
    m_size(new CopyableStaticText(this)),
    m_priv(new CopyableStaticText(this)),
    m_ratio(new CopyableStaticText(this)),
    m_lastDownload(new CopyableStaticText(this)),
    m_lastUpload(new CopyableStaticText(this)),
    m_totalDownload(new CopyableStaticText(this)),
    m_totalUpload(new CopyableStaticText(this))
{
    SetBackgroundColour(Theme::Colors().window);

    // One line that sums up the selected torrent: where it is saved, peers and time left
    m_summary = new wxStaticText(this, wxID_ANY, i18n("summary_select"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_summary->SetForegroundColour(Theme::Colors().mutedText);

    m_sizer = new wxFlexGridSizer(cols * 2, FromDIP(10), FromDIP(10));

    for (int i = 0; i < cols; i++)
    {
        m_sizer->AddGrowableCol(i * 2 + 1, 1);
    }

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("name")));
    m_sizer->Add(m_name, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("info_hash")));
    m_sizer->Add(m_infoHash, 0, wxEXPAND);

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("save_path")));
    m_sizer->Add(m_savePath, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("pieces")));
    m_sizer->Add(m_pieces, 0, wxEXPAND);

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("comment")));
    m_sizer->Add(m_comment, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("size")));
    m_sizer->Add(m_size, 0, wxEXPAND);

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("private")));
    m_sizer->Add(m_priv, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("ratio")));
    m_sizer->Add(m_ratio, 0, wxEXPAND);

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("last_download")));
    m_sizer->Add(m_lastDownload, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("last_upload")));
    m_sizer->Add(m_lastUpload, 0, wxEXPAND);

    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("total_download")));
    m_sizer->Add(m_totalDownload, 0, wxEXPAND);
    m_sizer->Add(MutedLabel(this, wxID_ANY, i18n("total_upload")));
    m_sizer->Add(m_totalUpload, 0, wxEXPAND);

    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    m_mainSizer->Add(m_summary, 0, wxEXPAND | wxTOP | wxRIGHT | wxLEFT, FromDIP(12));

    // The piece bar, when shown, goes right after the summary (index 1, see UpdateView)
    if (showPieceProgress)
    {
        m_pieceProgress = new Widgets::PieceProgressBar(this, wxID_ANY, m_isDarkMode);
        m_mainSizer->Add(m_pieceProgress, 0, wxEXPAND | wxTOP | wxRIGHT | wxLEFT, FromDIP(10));
    }

    m_mainSizer->Add(m_sizer, 1, wxALL | wxEXPAND, FromDIP(12));

    this->SetSizer(m_mainSizer);
    this->FitInside();
    this->SetScrollRate(5, 5);
}

void TorrentDetailsOverviewPanel::Refresh(pt::BitTorrent::TorrentHandle* torrent)
{
    auto status = torrent->Status();

    if (m_pieceProgress != nullptr)
    {
        m_pieceProgress->UpdateBitfield(status.pieces);
    }

    m_name->SetLabel(Utils::toStdWString(status.name));
    m_savePath->SetLabel(Utils::toStdWString(status.savePath));
    m_infoHash->SetLabel(status.infoHash);
    m_pieces->SetLabel(
        fmt::format(
            i18n("d_of_d"),
            status.pieces.count(),
            status.pieces.size()));
    m_ratio->SetLabel(
        fmt::format("{:.3f}", status.ratio));

    std::wstring activityDown = i18n("last_activity_unknown");
    std::wstring activityUp = i18n("last_activity_unknown");

    if (status.lastDownload.count() >= 0)
    {
        activityDown = status.lastDownload.count() < 2
            ? i18n("last_activity_just_now")
            : SecondsToFriendly(status.lastDownload);
    }

    if (status.lastUpload.count() >= 0)
    {
        activityUp = status.lastUpload.count() < 2
            ? i18n("last_activity_just_now")
            : SecondsToFriendly(status.lastUpload);
    }

    m_lastDownload->SetLabel(activityDown);
    m_lastUpload->SetLabel(activityUp);
    m_totalDownload->SetLabel(
        Utils::toHumanFileSize(status.allTimeDownload));
    m_totalUpload->SetLabel(
        Utils::toHumanFileSize(status.allTimeUpload));

    std::wstring summary = fmt::format(i18n("summary_saved_in_s"), Utils::toStdWString(status.savePath));

    if (!status.paused)
    {
        summary += L" · " + fmt::format(i18n("summary_peers_d"), status.peersCurrent);
    }

    if (status.state == pt::BitTorrent::TorrentStatus::State::Error)
    {
        summary += L" · " + fmt::format(i18n("state_error"), Utils::toStdWString(status.error));
    }
    else if (status.progress >= 1.0f)
    {
        summary += L" · " + i18n("summary_complete");
    }
    else if (status.paused)
    {
        summary += L" · " + i18n("summary_paused");
    }
    else if (status.eta.count() > 0)
    {
        summary += L" · " + fmt::format(i18n("summary_ready_in_s"), SecondsToFriendly(status.eta));
    }

    m_summary->SetLabel(summary);

    if (auto tf = status.torrentFile.lock())
    {
        m_comment->SetLabel(tf->comment());
        m_priv->SetLabel(
            tf->priv()
            ? i18n("yes")
            : i18n("no"));

        if (tf->total_size() != status.totalWanted)
        {
            m_size->SetLabel(
                fmt::format(
                    i18n("d_of_d"),
                    Utils::toHumanFileSize(status.totalWanted),
                    Utils::toHumanFileSize(tf->total_size())));
        }
        else
        {
            m_size->SetLabel(
                Utils::toHumanFileSize(status.totalWanted));
        }
    }

    this->Layout();
}

void TorrentDetailsOverviewPanel::Reset()
{
    if (m_pieceProgress != nullptr)
    {
        m_pieceProgress->UpdateBitfield({});
    }

    m_summary->SetLabel(i18n("summary_select"));
    m_name->SetLabel("-");
    m_infoHash->SetLabel("-");
    m_savePath->SetLabel("-");
    m_pieces->SetLabel("-");
    m_comment->SetLabel("-");
    m_size->SetLabel("-");
    m_priv->SetLabel("-");
    m_ratio->SetLabel("-");
    m_lastDownload->SetLabel("-");
    m_lastUpload->SetLabel("-");
    m_totalDownload->SetLabel("-");
    m_totalUpload->SetLabel("-");
}

void TorrentDetailsOverviewPanel::UpdateView(int cols, bool showPieceProgress)
{
    if (showPieceProgress && m_pieceProgress == nullptr)
    {
        m_pieceProgress = new Widgets::PieceProgressBar(this, wxID_ANY, m_isDarkMode);
        m_mainSizer->Insert(1, m_pieceProgress, 0, wxEXPAND | wxTOP | wxRIGHT | wxLEFT, FromDIP(10));
    }
    else if (!showPieceProgress && m_pieceProgress != nullptr)
    {
        m_mainSizer->Remove(1);

        delete m_pieceProgress;
        m_pieceProgress = nullptr;
    }

    for (int i = 0; i < m_sizer->GetCols(); i++)
    {
        if (m_sizer->IsColGrowable(i))
        {
            m_sizer->RemoveGrowableCol(i);
        }
    }

    m_sizer->SetCols(cols * 2);

    for (int i = 0; i < cols; i++)
    {
        m_sizer->AddGrowableCol(i * 2 + 1, 1);
    }

    this->Layout();
    this->FitInside();
}
