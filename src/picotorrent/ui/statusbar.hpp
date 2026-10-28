#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <memory>
#include <string>

namespace pt
{
namespace UI
{
    // The bar under the details panel: transfer rates on the left and a discreet link to
    // donate on the right. The torrent count, DHT and IP filter state are in its tooltip.
    class StatusBar : public wxPanel
    {
    public:
        StatusBar(wxWindow* parent);

        void UpdateDhtNodesCount(int64_t nodes);
        void UpdateIPFilterStatus(bool enabled);
        void UpdateTorrentCount(int64_t torrents);
        void UpdateTransferRates(int64_t downSpeed, int64_t upSpeed);

    private:
        void UpdateToolTip();
        void SetDonateHover(bool hover);

        wxStaticText* m_rates;
        wxStaticBitmap* m_heart;
        wxStaticText* m_donate;

        std::wstring m_torrents;
        std::wstring m_dht;
        std::wstring m_ipFilter;
    };
}
}
