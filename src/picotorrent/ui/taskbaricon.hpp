#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/taskbar.h>

namespace pt
{
namespace UI
{
    class TaskBarIcon : public wxTaskBarIcon
    {
    public:
        TaskBarIcon(wxWindow* parent);

        void Hide();
        void Show();

#ifndef __WXMSW__
        // wxTaskBarIcon has balloons only on Windows; on macOS this is a system notification
        bool ShowBalloon(const wxString& title, const wxString& text);
#endif

    private:
        wxMenu* CreatePopupMenu() wxOVERRIDE;

        wxWindow* m_parent;
#ifdef __WXMSW__
        wxIcon m_icon;
#else
        wxBitmapBundle m_icon;
#endif
    };
}
}
