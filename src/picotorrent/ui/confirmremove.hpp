#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <boost/log/trivial.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>

#include "translator.hpp"

namespace pt::UI
{
    // Asks before removing torrents, with "Cancel" as the default button and button labels
    // without keyboard shortcuts: a key pressed by mistake (Delete, Enter, or a letter typed for
    // another program while this window had the focus) can no longer confirm it. Every answer
    // goes to the log with where the removal was asked from.
    inline bool ConfirmRemove(wxWindow* parent, size_t count, bool withFiles, const char* source)
    {
        wxMessageDialog dlg(
            parent,
            withFiles ? wxString(i18n("confirm_remove_description")) : wxString(fmt::format(i18n("confirm_remove_from_list_s"), count)),
            i18n("confirm_remove"),
            wxYES_NO | wxNO_DEFAULT | (withFiles ? wxICON_WARNING : wxICON_QUESTION));

        dlg.SetYesNoLabels(
            wxString(i18n(withFiles ? "confirm_remove_files_yes" : "confirm_remove_yes")),
            wxString(i18n("confirm_remove_no")));

        bool confirmed = dlg.ShowModal() == wxID_YES;

        BOOST_LOG_TRIVIAL(info) << "Remove " << (withFiles ? "with files " : "") << "of " << count
            << " torrent(s) asked from " << source << ": " << (confirmed ? "confirmed" : "cancelled");

        return confirmed;
    }
}
