#include "statusbar.hpp"

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <wx/dcclient.h>
#include <wx/statbmp.h>

#include "theme.hpp"
#include "translator.hpp"
#include "../core/utils.hpp"

using pt::UI::StatusBar;

namespace
{
    // Donations will go through PayPal once the account exists. Until then the link opens the
    // "Donar" section of the downloads page, which is where the PayPal link will appear.
    const char* DonateUrl = "https://github.com/Brizna-Soft/hntorrent-descargas#donar";

    wxBitmapBundle Heart(wxColour const& colour)
    {
        std::string svg = fmt::format(
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24">)"
            R"(<path d="M12 20.3C6.4 16.2 3.3 12.9 3.3 9.1 3.3 6.4 5.4 4.3 8 4.3c1.7 0 3.1.9 4 2.3.9-1.4 2.3-2.3 4-2.3 2.6 0 4.7 2.1 4.7 4.8 0 3.8-3.1 7.1-8.7 11.2z")"
            R"( fill="none" stroke="{}" stroke-width="2" stroke-linejoin="round"/></svg>)",
            colour.GetAsString(wxC2S_HTML_SYNTAX).ToStdString());

        return wxBitmapBundle::FromSVG(svg.c_str(), wxSize(14, 14));
    }
}

StatusBar::StatusBar(wxWindow* parent)
    : wxPanel(parent, wxID_ANY),
    m_rates(new wxStaticText(this, wxID_ANY, wxEmptyString)),
    m_heart(nullptr),
    m_donate(new wxStaticText(this, wxID_ANY, i18n("donate")))
{
    auto const& colors = Theme::Colors();

    SetBackgroundColour(colors.bar);
    m_rates->SetForegroundColour(colors.mutedText);
    m_donate->SetForegroundColour(colors.mutedText);

    m_heart = new wxStaticBitmap(this, wxID_ANY, Heart(colors.mutedText));

    // A plain link: no window opens in the program, only the page in the browser
    for (wxWindow* w : std::initializer_list<wxWindow*>{ m_heart, m_donate })
    {
        w->SetCursor(wxCursor(wxCURSOR_HAND));
        w->SetToolTip(i18n("donate_help"));
        w->Bind(wxEVT_LEFT_UP, [](wxMouseEvent&) { wxLaunchDefaultBrowser(DonateUrl); });
        w->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) { SetDonateHover(true); });
        w->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) { SetDonateHover(false); });
    }

    // Thin line between the details panel and this bar
    Bind(wxEVT_PAINT, [this](wxPaintEvent&)
        {
            wxPaintDC dc(this);
            dc.SetPen(wxPen(Theme::Colors().border));
            dc.DrawLine(0, 0, GetClientSize().GetWidth(), 0);
        });

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_rates, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
    sizer->AddStretchSpacer();
    sizer->Add(m_heart, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    sizer->Add(m_donate, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));

    auto outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, 1, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(6));
    SetSizer(outer);

    UpdateTransferRates(0, 0);
}

void StatusBar::UpdateDhtNodesCount(int64_t nodes)
{
    m_dht = nodes < 0
        ? i18n("dht_disabled")
        : fmt::format(i18n("dht_num_nodes"), nodes);

    UpdateToolTip();
}

void StatusBar::UpdateTorrentCount(int64_t torrents)
{
    m_torrents = fmt::format(i18n("num_torrents"), torrents);
    UpdateToolTip();
}

void StatusBar::UpdateTransferRates(int64_t downSpeed, int64_t upSpeed)
{
    m_rates->SetLabel(
        fmt::format(
            L"↓ {}/s  ·  ↑ {}/s",
            Utils::toHumanFileSize(downSpeed),
            Utils::toHumanFileSize(upSpeed)));

    Layout();
}

void StatusBar::UpdateIPFilterStatus(bool enabled)
{
    m_ipFilter = enabled
        ? i18n("ip_filter_enabled")
        : i18n("ip_filter_disabled");

    UpdateToolTip();
}

void StatusBar::UpdateToolTip()
{
    m_rates->SetToolTip(fmt::format(L"{}\n{}\n{}", m_torrents, m_dht, m_ipFilter));
}

void StatusBar::SetDonateHover(bool hover)
{
    auto const& colors = Theme::Colors();
    wxColour colour = hover ? colors.accentText : colors.mutedText;

    m_donate->SetForegroundColour(colour);
    m_donate->Refresh();
    m_heart->SetBitmap(Heart(colour));
}
