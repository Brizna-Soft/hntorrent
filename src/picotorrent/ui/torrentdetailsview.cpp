#include "torrentdetailsview.hpp"

#include <functional>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>

#include "../core/configuration.hpp"
#include "theme.hpp"
#include "torrentdetailsfilespanel.hpp"
#include "torrentdetailsoverviewpanel.hpp"
#include "torrentdetailspeerspanel.hpp"
#include "torrentdetailstrackerspanel.hpp"
#include "translator.hpp"

using pt::UI::TorrentDetailsView;

namespace
{
    // Tabs drawn as plain words; the selected one is bold and in the accent colour
    class TabBar : public wxWindow
    {
    public:
        TabBar(wxWindow* parent, std::function<void(size_t)> onSelect)
            : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE),
            m_onSelect(onSelect)
        {
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            SetMinSize(wxSize(-1, FromDIP(32)));

            Bind(wxEVT_PAINT, &TabBar::OnPaint, this);
            Bind(wxEVT_LEFT_DOWN, &TabBar::OnLeftDown, this);
            Bind(wxEVT_MOTION, &TabBar::OnMotion, this);
            Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) { SetHover(-1); });
        }

        void AddTab(wxString const& label)
        {
            m_labels.push_back(label);
            Refresh();
        }

    private:
        // Each tab gets the width of its bold text, so nothing moves when the selection changes
        std::vector<wxRect> TabRects()
        {
            wxClientDC dc(this);
            dc.SetFont(GetFont().Bold());

            std::vector<wxRect> rects;
            int x = FromDIP(12);
            const int padding = FromDIP(4);

            for (auto const& label : m_labels)
            {
                int width = dc.GetTextExtent(label).GetWidth();
                rects.push_back(wxRect(x - padding, 0, width + 2 * padding, GetClientSize().GetHeight()));
                x += width + FromDIP(16);
            }

            return rects;
        }

        int HitTest(wxPoint const& point)
        {
            auto rects = TabRects();

            for (size_t i = 0; i < rects.size(); i++)
            {
                if (rects[i].Contains(point)) { return static_cast<int>(i); }
            }

            return -1;
        }

        void SetHover(int hover)
        {
            if (hover == m_hover) { return; }

            m_hover = hover;
            SetCursor(hover >= 0 ? wxCursor(wxCURSOR_HAND) : wxNullCursor);
            Refresh();
        }

        void OnLeftDown(wxMouseEvent& evt)
        {
            int tab = HitTest(evt.GetPosition());

            if (tab >= 0 && static_cast<size_t>(tab) != m_selected)
            {
                m_selected = static_cast<size_t>(tab);
                m_onSelect(m_selected);
                Refresh();
            }
        }

        void OnMotion(wxMouseEvent& evt)
        {
            SetHover(HitTest(evt.GetPosition()));
        }

        void OnPaint(wxPaintEvent&)
        {
            auto const& colors = pt::UI::Theme::Colors();
            wxAutoBufferedPaintDC dc(this);

            dc.SetBackground(wxBrush(colors.window));
            dc.Clear();

            // Thin line between the torrent list and the details
            dc.SetPen(wxPen(colors.border));
            dc.DrawLine(0, 0, GetClientSize().GetWidth(), 0);

            auto rects = TabRects();
            const int padding = FromDIP(4);

            for (size_t i = 0; i < m_labels.size(); i++)
            {
                bool selected = i == m_selected;

                dc.SetFont(selected ? GetFont().Bold() : GetFont());
                dc.SetTextForeground(selected
                    ? colors.accentText
                    : (static_cast<int>(i) == m_hover ? colors.text : colors.mutedText));

                wxSize extent = dc.GetTextExtent(m_labels[i]);
                dc.DrawText(m_labels[i], rects[i].x + padding, (rects[i].height - extent.GetHeight()) / 2);
            }
        }

        std::function<void(size_t)> m_onSelect;
        std::vector<wxString> m_labels;
        size_t m_selected = 0;
        int m_hover = -1;
    };
}

TorrentDetailsView::TorrentDetailsView(wxWindow* parent, wxWindowID id, std::shared_ptr<pt::Core::Configuration> cfg)
    : wxPanel(parent, id),
    m_cfg(cfg),
    m_book(new wxSimplebook(this, wxID_ANY)),
    m_overview(new TorrentDetailsOverviewPanel(m_book, wxID_ANY, cfg->IsDarkMode())),
    m_files(new TorrentDetailsFilesPanel(m_book, wxID_ANY)),
    m_peers(new TorrentDetailsPeersPanel(m_book, wxID_ANY)),
    m_trackers(new TorrentDetailsTrackersPanel(m_book, wxID_ANY))
{
    SetBackgroundColour(Theme::Colors().window);
    m_book->SetBackgroundColour(Theme::Colors().window);

    auto tabs = new TabBar(this, [this](size_t page) { m_book->ChangeSelection(page); });

    m_book->AddPage(m_overview, i18n("overview"));
    m_book->AddPage(m_files,    i18n("files"));
    m_book->AddPage(m_peers,    i18n("peers"));
    m_book->AddPage(m_trackers, i18n("trackers"));

    for (size_t i = 0; i < m_book->GetPageCount(); i++)
    {
        tabs->AddTab(m_book->GetPageText(i));
    }

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(tabs, 0, wxEXPAND);
    sizer->Add(m_book, 1, wxEXPAND);
    SetSizer(sizer);

    this->ReloadConfiguration();
}

TorrentDetailsView::~TorrentDetailsView()
{
}

void TorrentDetailsView::Refresh(std::map<lt::info_hash_t, pt::BitTorrent::TorrentHandle*> const& torrents)
{
    if (torrents.size() != 1)
    {
        this->Reset();
        return;
    }

    m_overview->Refresh(torrents.begin()->second);
    m_files->Refresh(torrents.begin()->second);
    m_peers->Refresh(torrents.begin()->second);
    m_trackers->Refresh(torrents.begin()->second);
}

void TorrentDetailsView::ReloadConfiguration()
{
    auto showPieceProgress = m_cfg->Get<bool>("ui.torrent_overview.show_piece_progress");
    auto cols = m_cfg->Get<int>("ui.torrent_overview.columns");

    m_overview->UpdateView(
        cols.value_or(2),
        showPieceProgress.value_or(true));
}

void TorrentDetailsView::Reset()
{
    m_overview->Reset();
    m_files->Reset();
    m_peers->Reset();
    m_trackers->Reset();
}
