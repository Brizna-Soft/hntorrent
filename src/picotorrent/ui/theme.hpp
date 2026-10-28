#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// Colours of the HNTorrent design: a full table with a details panel underneath, thin
// coloured progress bars and a light status bar.
namespace pt::UI::Theme
{
    struct Palette
    {
        wxColour window;        // list, details and toolbar background
        wxColour bar;           // column headers and status bar background
        wxColour border;        // thin separators
        wxColour text;
        wxColour mutedText;     // headers, labels and the status bar
        wxColour selection;     // selected rows
        wxColour accent;        // brand blue: downloading, links on hover
        wxColour accentText;    // the selected tab
        wxColour complete;      // finished or seeding
        wxColour paused;
        wxColour error;
        wxColour track;         // empty part of a progress bar
        wxColour selectedTrack; // the same, on a selected row
    };

    // Picks the light or dark colours and installs the renderer that draws selected rows
    // and the splitter with them. Call once, before the main window is created.
    void Install(bool darkMode);

    Palette const& Colors();
}
