#include "torrentlistview.hpp"

#include <algorithm>
#include <memory>

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <wx/graphics.h>
#include <wx/itemattr.h>
#include <wx/persist.h>

#include "../bittorrent/torrenthandle.hpp"
#include "confirmremove.hpp"
#include "models/torrentlistmodel.hpp"
#include "persistence/persistenttorrentlistview.hpp"
#include "theme.hpp"
#include "translator.hpp"

using pt::UI::TorrentListView;
using pt::UI::Models::TorrentListModel;

namespace
{
    // A thin rounded bar with the percentage next to it. The model picks the colour of the
    // bar from the torrent state (TorrentListModel::GetAttrByRow).
    class ProgressBarRenderer : public wxDataViewCustomRenderer
    {
    public:
        // The default alignment centres the bar vertically in the row
        ProgressBarRenderer()
            : wxDataViewCustomRenderer("long", wxDATAVIEW_CELL_INERT)
        {
        }

        bool SetValue(const wxVariant& value) override
        {
            m_percent = std::clamp(value.GetLong(), 0L, 100L);
            return true;
        }

        bool GetValue(wxVariant& value) const override
        {
            value = m_percent;
            return true;
        }

        wxSize GetSize() const override
        {
            // Like wxDataViewProgressRenderer: any width fits, so it takes the whole column
            return GetView()->FromDIP(wxSize(-1, 16));
        }

        bool Render(wxRect rect, wxDC* dc, int state) override
        {
            auto const& colors = pt::UI::Theme::Colors();
            wxWindow* view = GetView();

            const int barHeight = view->FromDIP(6);
            const int gap = view->FromDIP(8);
            const int labelWidth = std::min(view->FromDIP(38), rect.width / 2);

            wxRect bar(rect.x, rect.y + (rect.height - barHeight) / 2, rect.width - gap - labelWidth, barHeight);
            wxRect label(bar.GetRight() + gap, rect.y, labelWidth, rect.height);

            const wxColour fill = GetAttr().HasColour() ? GetAttr().GetColour() : colors.accent;
            const wxColour track = (state & wxDATAVIEW_CELL_SELECTED) ? colors.selectedTrack : colors.track;
            const double radius = barHeight / 2.0;
            const double filled = m_percent > 0 ? std::max(bar.width * m_percent / 100.0, static_cast<double>(barHeight)) : 0.0;

            if (bar.width > barHeight)
            {
                std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(*dc));

                if (gc)
                {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(track));
                    gc->DrawRoundedRectangle(bar.x, bar.y, bar.width, bar.height, radius);

                    if (filled > 0)
                    {
                        gc->SetBrush(wxBrush(fill));
                        gc->DrawRoundedRectangle(bar.x, bar.y, filled, bar.height, radius);
                    }
                }
                else
                {
                    wxDCPenChanger pen(*dc, *wxTRANSPARENT_PEN);
                    wxDCBrushChanger brush(*dc, wxBrush(track));
                    dc->DrawRoundedRectangle(bar, radius);

                    if (filled > 0)
                    {
                        dc->SetBrush(wxBrush(fill));
                        dc->DrawRoundedRectangle(wxRect(bar.x, bar.y, static_cast<int>(filled), bar.height), radius);
                    }
                }
            }

            wxDCTextColourChanger text(*dc, colors.mutedText);
            dc->DrawLabel(wxString::Format("%ld %%", m_percent), label, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);

            return true;
        }

    private:
        long m_percent = 0;
    };
}

TorrentListView::TorrentListView(wxWindow* parent, wxWindowID id, pt::UI::Models::TorrentListModel* model)
    : wxDataViewCtrl(parent, id, wxDefaultPosition, wxDefaultSize, wxDV_MULTIPLE | wxBORDER_NONE, wxDefaultValidator, "TorrentListView"),
    m_model(model)
{
    this->AssociateModel(m_model);
    m_model->DecRef();

    auto const& colors = Theme::Colors();
    this->SetBackgroundColour(colors.window);
    this->SetRowHeight(FromDIP(30));
    this->SetHeaderAttr(wxItemAttr(colors.mutedText, colors.bar, wxNullFont));

    auto defaultFlags = wxDATAVIEW_COL_REORDERABLE | wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;

    // Shown by default, as in the HNTorrent design: name, size, progress, download speed and
    // status. The other columns can be turned on from the context menu of the column headers.
    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("name"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Name,
                FromDIP(340),
                wxALIGN_NOT,
                defaultFlags)));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("size"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Size,
                FromDIP(90),
                wxALIGN_RIGHT,
                defaultFlags)));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("progress"),
                new ProgressBarRenderer(),
                TorrentListModel::Columns::Progress,
                FromDIP(190),
                wxALIGN_NOT,
                defaultFlags)));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("dl"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::DownloadSpeed,
                FromDIP(100),
                wxALIGN_RIGHT,
                defaultFlags)));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("status"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Status,
                FromDIP(150),
                wxALIGN_NOT,
                defaultFlags)));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("queue_position"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::QueuePosition,
                FromDIP(30),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("size_remaining"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::SizeRemaining,
                FromDIP(80),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("eta"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::ETA,
                FromDIP(80),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("ul"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::UploadSpeed,
                FromDIP(80),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("availability"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Availability,
                FromDIP(80),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("ratio"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Ratio,
                FromDIP(80),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("seeds"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Seeds,
                FromDIP(80),
                wxALIGN_RIGHT,
                wxDATAVIEW_COL_REORDERABLE | wxDATAVIEW_COL_RESIZABLE),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("peers"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::Peers,
                FromDIP(80),
                wxALIGN_RIGHT,
                wxDATAVIEW_COL_REORDERABLE | wxDATAVIEW_COL_RESIZABLE),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("added_on"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::AddedOn,
                FromDIP(120),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("completed_on"),
                new wxDataViewTextRenderer(),
                TorrentListModel::Columns::CompletedOn,
                FromDIP(120),
                wxALIGN_RIGHT,
                defaultFlags),
            true));

    m_columns.push_back(
        ColumnMetadata(
            new wxDataViewColumn(
                i18n("label"),
                new wxDataViewIconTextRenderer(),
                TorrentListModel::Columns::Label,
                FromDIP(80),
                wxALIGN_LEFT,
                defaultFlags),
            true));

    /*
    nameCol->GetRenderer()->EnableEllipsize(wxELLIPSIZE_END);
    statusCol->GetRenderer()->EnableEllipsize(wxELLIPSIZE_END);*/

    // Just add all columns. The persistence manager will set the saved
    // widths, order and hidden attributes. Otherwise we use our defaults.
    for (auto const& cmd : m_columns)
    {
        InsertColumn(GetColumnCount(), cmd.column);
    }

    if (!wxPersistenceManager::Get().RegisterAndRestore(this))
    {
        for (auto const& cmd : m_columns)
        {
            cmd.column->SetHidden(cmd.hidden);
        }
    }

    // insert the "fake" column last, always
    AppendColumn(new wxDataViewColumn(wxEmptyString, new wxDataViewTextRenderer(), TorrentListModel::Columns::_Max, 0, wxALIGN_CENTER, 0));

    // Keyboard accelerators
    std::vector<wxAcceleratorEntry> entries =
    {
        wxAcceleratorEntry(wxACCEL_CTRL,   int('A'),   ptID_KEY_SELECT_ALL),
        wxAcceleratorEntry(wxACCEL_NORMAL, WXK_DELETE, ptID_KEY_DELETE),
        wxAcceleratorEntry(wxACCEL_SHIFT,  WXK_DELETE, ptID_KEY_DELETE_FILES),
    };

    this->SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(entries.size()), entries.data()));

    this->Bind(wxEVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK, &TorrentListView::ShowHeaderContextMenu, this);

    this->Bind(
        wxEVT_MENU,
        [&](wxCommandEvent&)
        {
            wxDataViewItemArray items;
            this->GetSelections(items);

            if (items.IsEmpty()) { return; }

            // A stray key press must not remove torrents silently, so ask like the toolbar does
            if (!ConfirmRemove(m_parent, items.size(), false, "the Delete key")) {
                return;
            }

            for (wxDataViewItem& item : items)
            {
                m_model->GetTorrentFromItem(item)->Remove();
            }
        },
        ptID_KEY_DELETE);

    this->Bind(
        wxEVT_MENU,
        [&](wxCommandEvent&)
        {
            wxDataViewItemArray items;
            this->GetSelections(items);

            if (items.IsEmpty()) { return; }

            if (!ConfirmRemove(m_parent, items.size(), true, "Shift+Delete")) {
                return;
            }

            for (wxDataViewItem& item : items)
            {
                m_model->GetTorrentFromItem(item)->RemoveFiles();
            }
        },
        ptID_KEY_DELETE_FILES);

    this->Bind(
        wxEVT_MENU,
        [&](wxCommandEvent&)
        {
            this->SelectAll();
            wxPostEvent(
                GetParent(),
                wxCommandEvent(wxEVT_DATAVIEW_SELECTION_CHANGED, this->GetId()));
        },
        ptID_KEY_SELECT_ALL);
}

TorrentListView::~TorrentListView()
{
}

void TorrentListView::ShowHeaderContextMenu(wxCommandEvent&)
{
    wxMenu menu;

    // Do not iterate through the last column since it is the one we
    // append to prevent the last real column to stretch

    for (size_t i = 0; i < GetColumnCount() - 1; i++)
    {
        auto col = GetColumnAt(i);
        auto item = menu.Append(wxID_HIGHEST + i, col->GetTitle());
        item->SetCheckable(true);
        item->Check(!col->IsHidden());
    }

    menu.Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& evt)
        {
            auto col = GetColumnAt(evt.GetId() - wxID_HIGHEST);
            col->SetHidden(!col->IsHidden());
        });

    PopupMenu(&menu);
}
