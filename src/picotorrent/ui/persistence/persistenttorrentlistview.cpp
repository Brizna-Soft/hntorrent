#include "persistenttorrentlistview.hpp"

#include <map>

#include <nlohmann/json.hpp>

#include "../models/torrentlistmodel.hpp"
#include "../torrentlistview.hpp"

using json = nlohmann::json;
using pt::UI::TorrentListView;
using pt::UI::Models::TorrentListModel;
using pt::UI::Persistence::PersistentTorrentListView;

static std::map<TorrentListModel::Columns, std::string> ColumnNames =
{
    { TorrentListModel::Columns::Name,          "Col_Name" },
    { TorrentListModel::Columns::QueuePosition, "Col_QueuePosition" },
    { TorrentListModel::Columns::Size,          "Col_Size" },
    { TorrentListModel::Columns::SizeRemaining, "Col_SizeRemaining" },
    { TorrentListModel::Columns::Status,        "Col_Status" },
    { TorrentListModel::Columns::Progress,      "Col_Progress" },
    { TorrentListModel::Columns::ETA,           "Col_ETA" },
    { TorrentListModel::Columns::DownloadSpeed, "Col_DownloadSpeed" },
    { TorrentListModel::Columns::UploadSpeed,   "Col_UploadSpeed" },
    { TorrentListModel::Columns::Availability,  "Col_Availability" },
    { TorrentListModel::Columns::Ratio,         "Col_Ratio" },
    { TorrentListModel::Columns::Seeds,         "Col_Seeds" },
    { TorrentListModel::Columns::Peers,         "Col_Peers" },
    { TorrentListModel::Columns::AddedOn,       "Col_AddedOn" },
    { TorrentListModel::Columns::CompletedOn,   "Col_CompletedOn" },
    { TorrentListModel::Columns::Label,         "Col_Label" },
};

PersistentTorrentListView::PersistentTorrentListView(TorrentListView* tlv)
    : wxPersistentWindow<TorrentListView>(tlv),
    m_tlv(tlv)
{
}

wxString PersistentTorrentListView::GetKind() const
{
    // A new name with the HNTorrent design (0.27), so that its default columns replace
    // the ones saved by earlier versions once
    return "torrentList2";
}

bool PersistentTorrentListView::Restore()
{
    wxString hiddenJson;
    wxString modelsJson;
    wxString widthsJson;

    if (!RestoreValue("Hidden", &hiddenJson)
        || !RestoreValue("Models", &modelsJson)
        || !RestoreValue("Widths", &widthsJson))
    {
        return false;
    }

    json hidden;
    json models;
    json widths;

    try
    {
        hidden = json::parse(hiddenJson.ToStdString());
        models = json::parse(modelsJson.ToStdString());
        widths = json::parse(widthsJson.ToStdString());
    }
    catch (std::exception)
    {
        return false;
    }

    if (hidden.size() != m_tlv->GetColumnCount()
        || models.size() != m_tlv->GetColumnCount()
        || widths.size() != m_tlv->GetColumnCount())
    {
        return false;
    }

#ifndef __WXMSW__
    // macOS: the native list deletes a column when it is removed, so the columns cannot be put
    // back in the saved order. They keep their order; the hidden ones and the widths come back.
    for (uint32_t i = 0; i < m_tlv->GetColumnCount(); i++)
    {
        auto col = m_tlv->GetColumn(i);
        int64_t modelIndex = static_cast<int64_t>(col->GetModelColumn());

        auto iter = std::find(models.begin(), models.end(), modelIndex);
        if (iter == models.end()) { return false; }
        auto saved = static_cast<size_t>(std::distance(models.begin(), iter));

        col->SetHidden(hidden[saved]);
        col->SetWidth(widths[saved]);
    }
#else
    std::vector<wxDataViewColumn*> cols;
    cols.resize(m_tlv->GetColumnCount());

    for (uint32_t i = m_tlv->GetColumnCount(); i > 0 ; i--)
    {
        auto col = m_tlv->GetColumnAt(i - 1);

        // Calculate its order based on the index in the models array

        int64_t modelIndex = static_cast<int64_t>(col->GetModelColumn());

        auto iter = std::find(models.begin(), models.end(), modelIndex);
        if (iter == models.end()) { return false; }
        auto dist = std::distance(models.begin(), iter);

        cols[dist] = col;

        m_tlv->DeleteColumn(col);
    }

    for (uint32_t i = 0; i < cols.size(); i++)
    {
        auto col = cols.at(i);

        m_tlv->AppendColumn(col);

        col->SetHidden(hidden[i]);
        col->SetWidth(widths[i]);
    }
#endif

    int sortIndex = -1;
    bool sortAscending = false;

    if (RestoreValue("SortIndex", &sortIndex)
        && RestoreValue("SortAscending", &sortAscending))
    {
#ifndef __WXMSW__
        // A position outside the list would end the program on macOS
        if (sortIndex >= 0 && sortIndex < static_cast<int>(m_tlv->GetColumnCount()))
        {
            m_tlv->GetColumn(sortIndex)->SetSortOrder(sortAscending);
        }
#else
        m_tlv->GetColumn(sortIndex)->SetSortOrder(sortAscending);
#endif
    }

    return true;
}

void PersistentTorrentListView::Save() const
{
    if (auto sortingColumn = m_tlv->GetSortingColumn())
    {
        SaveValue("SortIndex", m_tlv->GetColumnIndex(sortingColumn));
        SaveValue("SortAscending", sortingColumn->IsSortOrderAscending());
    }

    json hidden;
    json models;
    json widths;

    // subtract one from the column count since we have a fake column
    // at the very end

    for (uint32_t i = 0; i < m_tlv->GetColumnCount() - 1; i++)
    {
        auto col = m_tlv->GetColumnAt(i);

        hidden.push_back(col->IsHidden());
        models.push_back(col->GetModelColumn());
        widths.push_back(col->GetWidth());
    }

    SaveValue("Hidden", hidden.dump());
    SaveValue("Models", models.dump());
    SaveValue("Widths", widths.dump());
}

PersistentTorrentListView* wxCreatePersistentObject(TorrentListView* lv)
{
    return new PersistentTorrentListView(lv);
}
