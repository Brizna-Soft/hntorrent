#include "taskbaricon.hpp"

#include "ids.hpp"
#include "translator.hpp"

#ifndef __WXMSW__
#include <wx/notifmsg.h>

#include "macicons.hpp"
#endif

using pt::UI::TaskBarIcon;

TaskBarIcon::TaskBarIcon(wxWindow* parent)
    : m_parent(parent),
#ifdef __WXMSW__
    m_icon(wxIcon(wxICON(AppIcon)))
#else
    // The logo in the menu bar of macOS, at the size of its icons
    m_icon(wxBitmapBundle::FromSVG(MacIcons::AppLogo, wxSize(18, 18)))
#endif
{
}

#ifndef __WXMSW__
bool TaskBarIcon::ShowBalloon(const wxString& title, const wxString& text)
{
    wxNotificationMessage notification(title, text, m_parent);
    return notification.Show();
}
#endif

void TaskBarIcon::Hide()
{
    RemoveIcon();
}

void TaskBarIcon::Show()
{
    SetIcon(m_icon, "HNTorrent");
}

wxMenu* TaskBarIcon::CreatePopupMenu()
{
    wxMenu* menu = new wxMenu();
#ifdef __WXOSX__
    // On macOS a click on the icon only opens this menu (Windows brings the window back with
    // the click itself), so the window comes back from here, also when it was closed to the icon
    menu->Append(ptID_EVT_SHOW_MAIN_WINDOW, i18n("show_hntorrent"));
    menu->AppendSeparator();
#endif
    menu->Append(ptID_EVT_ADD_TORRENT, i18n("amp_add_torrent"));
    menu->Append(ptID_EVT_ADD_MAGNET_LINK, i18n("amp_add_magnet_link_s"));
    menu->AppendSeparator();
    menu->Append(ptID_EVT_VIEW_PREFERENCES, i18n("preferences"));
    menu->AppendSeparator();
    menu->Append(ptID_EVT_EXIT, i18n("amp_exit"));
    return menu;
}
