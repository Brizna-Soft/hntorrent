#include "theme.hpp"

#include <wx/renderer.h>

using pt::UI::Theme::Palette;

namespace
{
    const Palette Light =
    {
        wxColour(0xFF, 0xFF, 0xFF), // window
        wxColour(0xF6, 0xF8, 0xFB), // bar
        wxColour(0xE3, 0xE8, 0xEF), // border
        wxColour(0x1F, 0x29, 0x33), // text
        wxColour(0x5F, 0x6B, 0x7A), // mutedText
        wxColour(0xE6, 0xF1, 0xFB), // selection
        wxColour(0x1A, 0x8F, 0xD6), // accent
        wxColour(0x18, 0x5F, 0xA5), // accentText
        wxColour(0x1D, 0x9E, 0x75), // complete
        wxColour(0x88, 0x87, 0x80), // paused
        wxColour(0xD6, 0x45, 0x45), // error
        wxColour(0xE3, 0xE8, 0xEF), // track
        wxColour(0xB5, 0xD4, 0xF4), // selectedTrack
    };

    const Palette Dark =
    {
        wxColour(0x20, 0x20, 0x20), // window
        wxColour(0x2A, 0x2B, 0x2E), // bar
        wxColour(0x3A, 0x3D, 0x42), // border
        wxColour(0xE8, 0xEA, 0xED), // text
        wxColour(0xA0, 0xA7, 0xB0), // mutedText
        wxColour(0x1E, 0x3A, 0x55), // selection
        wxColour(0x3B, 0xA3, 0xE6), // accent
        wxColour(0x6C, 0xB8, 0xF0), // accentText
        wxColour(0x2D, 0xB8, 0x8A), // complete
        wxColour(0x9A, 0x99, 0x92), // paused
        wxColour(0xE0, 0x66, 0x66), // error
        wxColour(0x3A, 0x3D, 0x42), // track
        wxColour(0x2F, 0x5D, 0x85), // selectedTrack
    };

    Palette g_palette = Light;

    // Selected rows (lists and trees) become a soft band in the selection colour and keep
    // the normal text colour; the splitter between the list and the details is a thin line.
    class Renderer : public wxDelegateRendererNative
    {
    public:
        // Everything else is drawn by the native renderer (the default constructor would
        // delegate to the generic, non-native one)
        Renderer()
            : wxDelegateRendererNative(wxRendererNative::GetDefault())
        {
        }

        void DrawItemSelectionRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags) override
        {
            // Cell selection is not used in this program; keep the native look for it
            if ((flags & wxCONTROL_CELL) || !(flags & wxCONTROL_SELECTED))
            {
                wxDelegateRendererNative::DrawItemSelectionRect(win, dc, rect, flags);
                return;
            }

            wxDCBrushChanger brush(dc, wxBrush(g_palette.selection));
            wxDCPenChanger pen(dc, *wxTRANSPARENT_PEN);
            dc.DrawRectangle(rect);
        }

        void DrawItemText(wxWindow* win, wxDC& dc, const wxString& text, const wxRect& rect, int align, int flags, wxEllipsizeMode ellipsizeMode) override
        {
            if (!(flags & wxCONTROL_SELECTED))
            {
                wxDelegateRendererNative::DrawItemText(win, dc, text, rect, align, flags, ellipsizeMode);
                return;
            }

            // wxDataViewCtrl gives selected rows the "highlight text" colour (white), made for a
            // dark selection; this selection is light, so they keep the normal text colour
            wxDCTextColourChanger colour(dc, g_palette.text);
            wxDelegateRendererNative::DrawItemText(win, dc, text, rect, align, flags & ~wxCONTROL_SELECTED, ellipsizeMode);
        }

        void DrawSplitterBorder(wxWindow*, wxDC&, const wxRect&, int) override
        {
        }

        void DrawSplitterSash(wxWindow* win, wxDC& dc, const wxSize& size, wxCoord position, wxOrientation orient, int) override
        {
            const int width = GetSplitterParams(win).widthSash;

            wxDCPenChanger pen(dc, *wxTRANSPARENT_PEN);
            wxDCBrushChanger brush(dc, wxBrush(g_palette.window));

            if (orient == wxVERTICAL)
            {
                dc.DrawRectangle(position, 0, width, size.y);
                dc.SetBrush(wxBrush(g_palette.border));
                dc.DrawRectangle(position + width / 2, 0, 1, size.y);
            }
            else
            {
                dc.DrawRectangle(0, position, size.x, width);
                dc.SetBrush(wxBrush(g_palette.border));
                dc.DrawRectangle(0, position + width / 2, size.x, 1);
            }
        }
    };
}

void pt::UI::Theme::Install(bool darkMode)
{
    g_palette = darkMode ? Dark : Light;

    // The first Get() sets up the global renderer and would throw away one set before it,
    // so it goes first. wxWidgets owns the new renderer; the previous one is ours to delete.
    wxRendererNative::Get();
    delete wxRendererNative::Set(new Renderer());
}

Palette const& pt::UI::Theme::Colors()
{
    return g_palette;
}
