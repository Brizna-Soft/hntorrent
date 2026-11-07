#include "aboutdialog.hpp"

#include <sstream>

#include <boost/version.hpp>
#include <fmt/core.h>
#include <libtorrent/version.hpp>
#include <nlohmann/json.hpp>
#include <openssl/opensslv.h>
#include <sqlite3.h>
#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/filename.h>
#include <wx/hyperlink.h>
#include <wx/listctrl.h>
#include <wx/statbmp.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <wx/version.h>

#include "../../buildinfo.hpp"
#include "../brizna.hpp"
#include "../translator.hpp"

using json = nlohmann::json;
using pt::UI::Dialogs::AboutDialog;

AboutDialog::AboutDialog(wxWindow* parent, wxWindowID id)
    : wxDialog(parent, id, i18n("about_picotorrent"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxIcon icon(L"AppIcon", wxBITMAP_TYPE_ICO_RESOURCE, 128, 128);
    auto bmp = new wxStaticBitmap(this, wxID_ANY, icon);

    auto lv = new wxListView(this);
    lv->AppendColumn(i18n("component"), wxLIST_FORMAT_LEFT, FromDIP(200));
    lv->AppendColumn(i18n("version"), wxLIST_FORMAT_LEFT, FromDIP(80));
    lv->AppendColumn(i18n("commitish"), wxLIST_FORMAT_LEFT, FromDIP(80));

    std::stringstream boostVersion;
    boostVersion << BOOST_VERSION / 100000 << "."
        << BOOST_VERSION / 100 % 1000 << "."
        << BOOST_VERSION % 100;

    std::stringstream fmtVersion;
    fmtVersion << FMT_VERSION / 10000 << "."
        << FMT_VERSION % 10000 / 100 << "."
        << FMT_VERSION % 100;

    std::stringstream nljson;
    nljson << NLOHMANN_JSON_VERSION_MAJOR << "."
        << NLOHMANN_JSON_VERSION_MINOR << "."
        << NLOHMANN_JSON_VERSION_PATCH;

    lv->InsertItem(lv->GetItemCount(), "Boost");
    lv->SetItem(lv->GetItemCount() - 1, 1, boostVersion.str());
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), "fmt");
    lv->SetItem(lv->GetItemCount() - 1, 1, fmtVersion.str());
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), "nlohmann-json");
    lv->SetItem(lv->GetItemCount() - 1, 1, nljson.str());
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), OPENSSL_VERSION_TEXT);
    lv->SetItem(lv->GetItemCount() - 1, 1, "-");
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), "HNTorrent");
    lv->SetItem(lv->GetItemCount() - 1, 1, BuildInfo::semver());
    lv->SetItem(lv->GetItemCount() - 1, 2, BuildInfo::commitish());

    lv->InsertItem(lv->GetItemCount(), "Rasterbar-libtorrent");
    lv->SetItem(lv->GetItemCount() - 1, 1, LIBTORRENT_VERSION);
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), "SQLite");
    lv->SetItem(lv->GetItemCount() - 1, 1, SQLITE_VERSION);
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    lv->InsertItem(lv->GetItemCount(), "wxWidgets");
    lv->SetItem(lv->GetItemCount() - 1, 1, wxVERSION_NUM_DOT_STRING_T);
    lv->SetItem(lv->GetItemCount() - 1, 2, "-");

    auto desc = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(-1, 120)), wxTE_MULTILINE | wxTE_READONLY);
    desc->AppendText(i18n("picotorrent_description"));
    desc->AppendText("\n\n");
    desc->AppendText(i18n("based_on_picotorrent_long"));
    desc->AppendText("\n");
    desc->AppendText(i18n("made_with_ai_help"));

    // The third-party licenses file sits next to the executable
    wxFileName notices(wxStandardPaths::Get().GetExecutablePath());
    notices.SetFullName("AVISOS-DE-TERCEROS.txt");

    auto noticesLink = new wxHyperlinkCtrl(this, wxID_ANY, i18n("third_party_notices"), notices.GetFullPath());
    noticesLink->Bind(wxEVT_HYPERLINK, [](wxHyperlinkEvent& evt) { wxLaunchDefaultApplication(evt.GetURL()); });

    auto footerSizer = new wxBoxSizer(wxHORIZONTAL);
    footerSizer->Add(new wxHyperlinkCtrl(this, wxID_ANY, i18n("hntorrent_on_github"), Brizna::Repositorio));
    footerSizer->AddStretchSpacer();
    footerSizer->Add(noticesLink);

    auto linksSizer = new wxBoxSizer(wxHORIZONTAL);
    linksSizer->Add(new wxHyperlinkCtrl(this, wxID_ANY, i18n("brizna_soft_page"), Brizna::PaginaEstudio));
    linksSizer->AddStretchSpacer();
    linksSizer->Add(new wxHyperlinkCtrl(this, wxID_ANY, i18n("donate_paypal"), Brizna::PaginaDonar));

    // «Copiar datos para el reporte»: solo la versión de HNTorrent y la de Windows
    auto copy = new wxButton(this, wxID_ANY, i18n("copy_report_data"));
    copy->SetToolTip(i18n("copy_report_data_help"));
    copy->Bind(wxEVT_BUTTON, [copy](wxCommandEvent&)
        {
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(Brizna::DatosParaReporte()));
                wxTheClipboard->Close();
                copy->SetLabel(i18n("report_data_copied"));
            }
            else
            {
                copy->SetLabel(i18n("report_data_not_copied"));
            }
        });
    auto close = new wxButton(this, wxID_OK, i18n("close"));
    auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(copy);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(close);
    SetAffirmativeId(wxID_OK);
    SetEscapeId(wxID_OK);
    close->SetDefault();

    auto border = FromDIP(11);
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(bmp, 0, wxLEFT | wxRIGHT | wxTOP | wxCENTER, border);
    sizer->AddSpacer(FromDIP(11));
    sizer->Add(desc, 0, wxEXPAND | wxLEFT | wxRIGHT, border);
    sizer->AddSpacer(FromDIP(7));
    sizer->Add(lv, wxSizerFlags(1).Border(wxLEFT | wxRIGHT, border).Expand());
    sizer->AddSpacer(FromDIP(7));
    sizer->Add(footerSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, border);
    sizer->AddSpacer(FromDIP(7));
    sizer->Add(linksSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, border);
    sizer->AddSpacer(FromDIP(11));
    sizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);

    this->SetSizerAndFit(sizer);
    this->SetSize(FromDIP(wxSize(380, 640)));   // cabe la fila de enlaces y los botones de abajo
}

AboutDialog::~AboutDialog()
{
}
