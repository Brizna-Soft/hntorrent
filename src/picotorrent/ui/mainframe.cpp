#include "mainframe.hpp"

#include <filesystem>
#include <fstream>
#include <regex>

#include <boost/log/trivial.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <wx/persist.h>
#include <wx/persist/toplevel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>
#include <wx/taskbarbutton.h>
#include <wx/toolbar.h>

#include "../applicationoptions.hpp"
#include "../bittorrent/addparams.hpp"
#include "../bittorrent/session.hpp"
#include "../bittorrent/sessionstatistics.hpp"
#include "../bittorrent/torrenthandle.hpp"
#include "../bittorrent/torrentstatistics.hpp"
#include "../bittorrent/torrentstatus.hpp"
#include "../core/configuration.hpp"
#include "../core/database.hpp"
#include "../core/environment.hpp"
#include "../core/utils.hpp"
#include "../ipc/server.hpp"
#include "../updatechecker.hpp"
#include "console.hpp"
#include "dialogs/aboutdialog.hpp"
#include "dialogs/addmagnetlinkdialog.hpp"
#include "dialogs/addtorrentdialog.hpp"
#include "dialogs/createtorrentdialog.hpp"
#include "confirmremove.hpp"
#include "dialogs/preferencesdialog.hpp"
#include "filters/torrentfilter.hpp"
#include "ids.hpp"
#include "models/torrentlistmodel.hpp"
#include "statusbar.hpp"
#include "taskbaricon.hpp"
#include "theme.hpp"
#include "torrentcontextmenu.hpp"
#include "torrentdetailsview.hpp"
#include "torrentlistview.hpp"
#include "toolbaricons.hpp"
#include "translator.hpp"

#ifdef _WIN32
#include "win32/openfiledialog.hpp"
#else
#include <wx/filedlg.h>
#include <wx/stdpaths.h>
#endif

#ifdef __WXOSX__
#include "macapp.hpp"
#endif

namespace fs = std::filesystem;
using pt::UI::MainFrame;

const char* WindowTitle = "HNTorrent";

namespace
{
    // Keeps the torrents whose name contains the text of the search box (any case)
    class NameFilter : public pt::UI::Filters::TorrentFilter
    {
    public:
        explicit NameFilter(wxString const& text)
            : m_text(text.Lower())
        {
        }

        bool Includes(pt::BitTorrent::TorrentHandle const& torrent) override
        {
            wxString name = pt::Utils::toStdWString(torrent.Status().name);
            return name.Lower().Find(m_text) != wxNOT_FOUND;
        }

    private:
        wxString m_text;
    };
}

#define LABEL_ICON_SIZE 16

MainFrame::MainFrame(std::shared_ptr<pt::Core::Environment> env, std::shared_ptr<pt::Core::Database> db, std::shared_ptr<pt::Core::Configuration> cfg, pt::CommandLineOptions const& options)
    : wxFrame(nullptr, wxID_ANY, WindowTitle, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "MainFrame"),
    m_env(env),
    m_db(db),
    m_cfg(cfg),
    m_options(options),
    m_session(new BitTorrent::Session(this, db, cfg, env)),
    m_splitter(new wxSplitterWindow(this, ptID_MAIN_SPLITTER)),
    m_statusBar(new StatusBar(this)),
    m_taskBarIcon(new TaskBarIcon(this)),
    m_torrentDetails(new TorrentDetailsView(m_splitter, ptID_MAIN_TORRENT_DETAILS, cfg)),
    m_torrentListModel(new Models::TorrentListModel()),
    m_torrentList(new TorrentListView(m_splitter, ptID_MAIN_TORRENT_LIST, m_torrentListModel)),
    m_torrentsCount(0),
    m_menuItemFilters(nullptr),
    m_ipc(std::make_unique<IPC::Server>(this))
{
    m_console = new Console(this, wxID_ANY, m_torrentListModel, m_cfg->IsDarkMode());

    m_splitter->SetWindowStyleFlag(
        m_splitter->GetWindowStyleFlag() | wxSP_LIVE_UPDATE);
    m_splitter->SetMinimumPaneSize(10);
    m_splitter->SetSashGravity(0.5);
    m_splitter->SplitHorizontally(
        m_torrentList,
        m_torrentDetails);

    m_torrentListModel->SetBackgroundColorEnabled(
        m_cfg->Get<bool>("use_label_as_list_bgcolor").value());

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_console, 0, wxEXPAND);
    sizer->Add(m_splitter, 1, wxEXPAND, 0);
    sizer->Add(m_statusBar, 0, wxEXPAND);
    sizer->SetSizeHints(this);

    // Keyboard accelerators
    std::vector<wxAcceleratorEntry> entries =
    {
        wxAcceleratorEntry(wxACCEL_CTRL,   int('U'),   ptID_KEY_ADD_MAGNET_LINK),
        wxAcceleratorEntry(wxACCEL_CTRL,   int('O'),   ptID_KEY_ADD_TORRENT),
        wxAcceleratorEntry(wxACCEL_NORMAL, WXK_F1,     ptID_KEY_VIEW_HELP),
    };

    this->SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(entries.size()), entries.data()));
#ifdef __WXMSW__
    // On macOS the icon of the Dock comes from the app bundle (HNTorrent.icns)
    this->SetIcon(wxICON(AppIcon));
#endif
    this->SetMenuBar(this->CreateMainMenu());
    this->CreateMainToolBar();
    this->SetSizerAndFit(sizer);

    this->CreateFilterMenuItems();
    this->CreateLabelMenuItems();
    this->UpdateLabels();

    // Set checked on menu items
    m_menuItemConsoleInput->SetCheckable(true);
    m_menuItemConsoleInput->Check(m_cfg->Get<bool>("ui.show_console_input").value());
    m_menuItemDetailsPanel->SetCheckable(true);
    m_menuItemDetailsPanel->Check(m_cfg->Get<bool>("ui.show_details_panel").value());
    m_menuItemStatusBar->SetCheckable(true);
    m_menuItemStatusBar->Check(m_cfg->Get<bool>("ui.show_status_bar").value());

    if (!m_cfg->Get<bool>("ui.show_console_input").value()) { m_console->Hide(); }
    if (!m_cfg->Get<bool>("ui.show_details_panel").value()) { m_splitter->Unsplit(); }
    if (!m_cfg->Get<bool>("ui.show_status_bar").value()) { m_statusBar->Hide(); }

    if (!wxPersistenceManager::Get().RegisterAndRestore(this))
    {
        // Wide enough for the default columns of the list
        this->SetSize(FromDIP(wxSize(940, 560)));
    }

    // Session events
    this->Bind(ptEVT_SESSION_STATISTICS, [this](pt::BitTorrent::SessionStatisticsEvent& evt)
        {
            bool dhtEnabled = m_cfg->Get<bool>("libtorrent.enable_dht").value();
            m_statusBar->UpdateDhtNodesCount(dhtEnabled ? evt.GetData().dhtNodes : -1);
        });

    this->Bind(ptEVT_TORRENT_ADDED, [this](wxCommandEvent& evt)
        {
            m_torrentsCount++;
            m_statusBar->UpdateTorrentCount(m_torrentsCount);
            m_torrentListModel->AddTorrent(static_cast<BitTorrent::TorrentHandle*>(evt.GetClientData()));
        });

    this->Bind(ptEVT_TORRENT_FINISHED, [this](wxCommandEvent& evt)
        {
            auto torrent = static_cast<BitTorrent::TorrentHandle*>(evt.GetClientData());
            m_taskBarIcon->ShowBalloon(
                i18n("torrent_finished"),
                Utils::toStdWString(torrent->Status().name));
        });

    this->Bind(ptEVT_TORRENT_REMOVED, [this](pt::BitTorrent::InfoHashEvent& evt)
        {
            m_torrentsCount--;
            m_statusBar->UpdateTorrentCount(m_torrentsCount);
            m_torrentListModel->RemoveTorrent(evt.GetData());

            // If this torrent is in our selection, remove it and clear the details view
            auto iter = m_selection.find(evt.GetData());

            if (iter != m_selection.end())
            {
                m_selection.erase(iter);
                m_torrentDetails->Reset();
            }
        });

    this->Bind(ptEVT_TORRENT_STATISTICS, [this](pt::BitTorrent::TorrentStatisticsEvent& evt)
        {
            auto stats = evt.GetData();

            m_statusBar->UpdateTransferRates(
                stats.totalPayloadDownloadRate,
                stats.totalPayloadUploadRate);

#ifdef __WXMSW__
            if (wxTaskBarButton* tbb = MSWGetTaskBarButton())
            {
                if (stats.isDownloadingAny && stats.totalWanted > 0)
                {
                    float totalProgress = stats.totalWantedDone / static_cast<float>(stats.totalWanted);

                    tbb->SetProgressState(wxTaskBarButtonState::wxTASKBAR_BUTTON_NORMAL);
                    tbb->SetProgressRange(static_cast<int>(100));
                    tbb->SetProgressValue(static_cast<int>(totalProgress * 100));
                }
                else
                {
                    tbb->SetProgressState(wxTaskBarButtonState::wxTASKBAR_BUTTON_NO_PROGRESS);
                }
            }
#endif
        });

    this->Bind(ptEVT_TORRENTS_UPDATED, [this](pt::BitTorrent::TorrentsUpdatedEvent& evt)
        {
            auto torrents = evt.GetData();
            m_torrentListModel->UpdateTorrents(torrents);

            std::map<lt::info_hash_t, pt::BitTorrent::TorrentHandle*> selectedUpdated;

            for (auto torrent : torrents)
            {
                auto infoHash = torrent->InfoHash();

                if (m_selection.find(infoHash) != m_selection.end())
                {
                    selectedUpdated.insert({ infoHash, torrent });
                }
            }

            if (selectedUpdated.size() > 0)
            {
                m_torrentDetails->Refresh(selectedUpdated);
            }

            this->CheckDiskSpace(torrents);
        });

    this->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxCommandEvent&)
        {
            for (auto const& sel : m_selection)
            {
                auto status = sel.second->Status();
                fs::path p = fs::path(status.savePath) / status.name;
                if (fs::exists(p)) { Utils::openAndSelect(p); }
            }
        }, ptID_MAIN_TORRENT_LIST);

    this->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &MainFrame::ShowTorrentContextMenu, this, ptID_MAIN_TORRENT_LIST);

    this->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
        [this](wxCommandEvent&)
        {
            wxDataViewItemArray items;
            m_torrentList->GetSelections(items);

            m_selection.clear();

            if (items.IsEmpty())
            {
                m_torrentDetails->Reset();
                return;
            }

            for (wxDataViewItem& item : items)
            {
                auto torrent = m_torrentListModel->GetTorrentFromItem(item);
                m_selection.insert({ torrent->InfoHash(), torrent });
            }

            m_torrentDetails->Refresh(m_selection);
        }, ptID_MAIN_TORRENT_LIST);

    // connect events
    this->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this, wxID_ANY);
    this->Bind(wxEVT_ICONIZE, &MainFrame::OnIconize, this, wxID_ANY);
    this->Bind(wxEVT_MENU, &MainFrame::OnFileAddTorrent, this, ptID_EVT_ADD_TORRENT);
    this->Bind(wxEVT_MENU, &MainFrame::OnFileAddMagnetLink, this, ptID_EVT_ADD_MAGNET_LINK);
    this->Bind(wxEVT_MENU, &MainFrame::OnFileCreateTorrent, this, ptID_EVT_CREATE_TORRENT);
    this->Bind(wxEVT_MENU, [this](wxCommandEvent&) { this->Close(true); }, ptID_EVT_EXIT);
    this->Bind(wxEVT_MENU, &MainFrame::OnViewPreferences, this, ptID_EVT_VIEW_PREFERENCES);
    this->Bind(wxEVT_MENU, &MainFrame::OnViewHelp, this, ptID_EVT_VIEW_HELP);
    this->Bind(wxEVT_MENU, &MainFrame::OnHelpAbout, this, ptID_EVT_ABOUT);
    this->Bind(wxEVT_MENU, &MainFrame::OnHelpCheckForUpdate, this, ptID_EVT_CHECK_FOR_UPDATE);
#ifdef __WXOSX__
    // On macOS About and Preferences have the standard ids (see CreateMainMenu)
    this->Bind(wxEVT_MENU, &MainFrame::OnHelpAbout, this, wxID_ABOUT);
    this->Bind(wxEVT_MENU, &MainFrame::OnViewPreferences, this, wxID_PREFERENCES);
#endif

    // Toolbar (adding torrents reuses the menu ids above)
    this->Bind(wxEVT_MENU, [this](wxCommandEvent&) { for (auto const& sel : m_selection) { sel.second->Resume(); } }, ptID_TOOLBAR_RESUME);
    this->Bind(wxEVT_MENU, [this](wxCommandEvent&) { for (auto const& sel : m_selection) { sel.second->Pause(); } }, ptID_TOOLBAR_PAUSE);
    this->Bind(wxEVT_MENU, &MainFrame::OnToolBarRemove, this, ptID_TOOLBAR_REMOVE);
    this->Bind(wxEVT_UPDATE_UI, &MainFrame::OnToolBarUpdateUI, this, ptID_TOOLBAR_RESUME, ptID_TOOLBAR_REMOVE);

    // Keyboard shortcuts
    this->Bind(wxEVT_MENU, &MainFrame::OnFileAddTorrent, this, ptID_KEY_ADD_TORRENT);
    this->Bind(wxEVT_MENU, &MainFrame::OnFileAddMagnetLink, this, ptID_KEY_ADD_MAGNET_LINK);
    this->Bind(wxEVT_MENU, &MainFrame::OnViewHelp, this, ptID_KEY_VIEW_HELP);

    m_taskBarIcon->Bind(wxEVT_MENU, &MainFrame::OnFileAddTorrent, this, ptID_EVT_ADD_TORRENT);
    m_taskBarIcon->Bind(wxEVT_MENU, &MainFrame::OnFileAddMagnetLink, this, ptID_EVT_ADD_MAGNET_LINK);
    m_taskBarIcon->Bind(wxEVT_MENU, [this](wxCommandEvent&) { this->Close(true); }, ptID_EVT_EXIT);
    m_taskBarIcon->Bind(wxEVT_MENU, &MainFrame::OnViewPreferences, this, ptID_EVT_VIEW_PREFERENCES);
    m_taskBarIcon->Bind(wxEVT_TASKBAR_LEFT_DOWN, &MainFrame::OnTaskBarLeftDown, this, wxID_ANY);
#ifdef __WXOSX__
    // On macOS the click on the icon opens its menu instead (see TaskBarIcon::CreatePopupMenu)
    m_taskBarIcon->Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowAndActivate(); }, ptID_EVT_SHOW_MAIN_WINDOW);
#endif

    this->Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&)
        {
            m_cfg->Set("ui.show_console_input", m_menuItemConsoleInput->IsChecked());

            if (m_menuItemConsoleInput->IsChecked()) { m_console->Show(); }
            else { m_console->Hide(); }

            this->SendSizeEvent();
        }, ptID_EVT_SHOW_CONSOLE);

    this->Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&)
        {
            m_cfg->Set("ui.show_details_panel", m_menuItemDetailsPanel->IsChecked());

            if (m_menuItemDetailsPanel->IsChecked())
            {
                m_splitter->SplitHorizontally(
                    m_torrentList,
                    m_torrentDetails);
            }
            else
            {
                m_splitter->Unsplit();
            }
        }, ptID_EVT_SHOW_DETAILS);

    this->Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&)
        {
            m_cfg->Set("ui.show_status_bar", m_menuItemStatusBar->IsChecked());

            if (m_menuItemStatusBar->IsChecked()) { m_statusBar->Show(); }
            else { m_statusBar->Hide(); }

            this->SendSizeEvent();
        }, ptID_EVT_SHOW_STATUS_BAR);

    this->Bind(
        ptEVT_TORRENT_METADATA_FOUND,
        [this](pt::BitTorrent::MetadataFoundEvent& evt)
        {
            for (auto dlg : m_addDialogs)
            {
                wxPostEvent(dlg, evt);
            }
        });

    this->Bind(
        ptEVT_FILTER_CHANGED,
        [this](wxCommandEvent& evt)
        {
            std::string filter = evt.GetString().ToStdString();
            m_cfg->Set("current_filter", filter);
        });

    // Update status bar
    m_statusBar->UpdateDhtNodesCount(m_cfg->Get<bool>("libtorrent.enable_dht").value() ? 0 : -1);
    m_statusBar->UpdateTorrentCount(m_torrentsCount);
    m_statusBar->UpdateIPFilterStatus(m_cfg->Get<bool>("ipfilter.enabled").value());

    // Show taskbar icon
    if (m_cfg->Get<bool>("show_in_notification_area").value())
    {
        m_taskBarIcon->Show();
    }

    // Set up console
    if (auto currentFilter = m_cfg->Get<std::string>("current_filter"))
    {
        m_console->SetText(currentFilter.value_or(""));
    }

    // Look for a new version in the background
    m_updateChecker = std::make_unique<UpdateChecker>(this, m_cfg, m_env);

    if (m_cfg->Get<bool>("update_checks.enabled").value_or(true))
    {
        CallAfter([this]() { m_updateChecker->Check(false); });
    }
}

MainFrame::~MainFrame()
{
    m_taskBarIcon->Hide();
    delete m_taskBarIcon;
}

void MainFrame::AddTorrents(std::vector<lt::add_torrent_params>& params, bool use_commandline_options)
{
    bool didRemove = false;

    for (auto it = params.begin(); it != params.end();)
    {
        lt::info_hash_t ih;
        if (it->ti) { ih = it->ti->info_hashes(); }
        else { ih = it->info_hashes; }

        if (m_session->HasTorrent(ih))
        {
            it = params.erase(it);
            didRemove = true;
        }
        else
        {
            ++it;
        }
    }

    if (didRemove)
    {
        if (!m_options.silent || !use_commandline_options)
        {
            auto err = i18n("some_torrents_already_in_session");
            if (params.empty()) err = i18n("all_torrents_already_in_session");

            wxMessageBox(err, "HNTorrent", wxOK, this);
        }
    }

    if (params.empty())
    {
        return;
    }

    std::vector<lt::info_hash_t> hashes;

    // Set up default values for all params
    // Also match labels

    auto labels = m_cfg->GetLabels();

    for (lt::add_torrent_params& p : params)
    {
        auto our = new BitTorrent::AddParams();

        p.flags |= lt::torrent_flags::duplicate_is_error;
        if (!m_options.save_path.empty() && use_commandline_options)
        {
            p.save_path = m_options.save_path;
        }
        else
        {
            p.save_path = m_cfg->Get<std::string>("default_save_path").value();
        }
        p.userdata = lt::client_data_t(our);

        // If we have a param with an info hash and no torrent info,
        // let the session find metadata for us

        if (
            ((p.info_hashes.has_v1() && !p.info_hashes.v1.is_all_zeros())
                || (p.info_hashes.has_v2() && !p.info_hashes.v2.is_all_zeros()))
            && !p.ti)
        {
            hashes.push_back(p.info_hashes);
        }

        // match any label that has an apply filter
        for (auto const& label : labels)
        {
            if (!label.applyFilterEnabled
                || label.applyFilter.empty())
            {
                continue;
            }

            std::string name;
            if (auto ti = p.ti) { name = ti->name(); }
            if (p.name.size() > 0) { name = p.name; }

            if (name.empty())
            {
                continue;
            }

            std::regex re(label.applyFilter, std::regex_constants::ECMAScript | std::regex_constants::icase);
            
            if (std::regex_search(name, re))
            {
                // we have a match
                our->labelId = label.id;

                if (label.savePath.size() > 0
                    && label.savePathEnabled)
                {
                    p.save_path = label.savePath;
                }

                break;
            }
        }
    }

    if (m_cfg->Get<bool>("skip_add_torrent_dialog").value() || (m_options.silent && use_commandline_options))
    {
        for (lt::add_torrent_params& p : params)
        {
            m_session->AddTorrent(p);
        }

        return;
    }

    for (auto& param : params)
    {
        auto dlg = new Dialogs::AddTorrentDialog(this, wxID_ANY, param, m_db, m_cfg, m_session);
        dlg->Bind(
            wxEVT_CLOSE_WINDOW,
            [this, dlg, hashes](wxCloseEvent& evt)
            {
                evt.Skip();
                m_addDialogs.erase(dlg);
                m_session->RemoveMetadataSearch(hashes);
            });
        dlg->Show();

        m_addDialogs.insert(dlg);
    }

    m_session->AddMetadataSearch(hashes);
}

void MainFrame::HandleParams(pt::CommandLineOptions const& options)
{
    std::vector<lt::add_torrent_params> params;

    m_options = options;
    if (!m_options.files.empty())
    {
        std::vector<std::wstring> converted;
        for (auto const& file : m_options.files)
        {
            converted.push_back(Utils::toStdWString(file));
        }

        ParseTorrentFiles(params, converted);
    }

    if (!m_options.magnets.empty())
    {
        for (std::string const& magnet : m_options.magnets)
        {
            lt::error_code ec;
            lt::add_torrent_params tp = lt::parse_magnet_uri(magnet, ec);

            if (ec)
            {
                BOOST_LOG_TRIVIAL(warning) << "Failed to parse magnet uri: " << magnet << ", error: " << ec;
                continue;
            }

            params.push_back(tp);
        }
    }

    AddTorrents(params, true);
}

void MainFrame::CheckDiskSpace(std::vector<pt::BitTorrent::TorrentHandle*> const& torrents)
{
    bool shouldCheck = m_cfg->Get<bool>("pause_on_low_disk_space").value();
    int limit = m_cfg->Get<int>("pause_on_low_disk_space_limit").value();

    if (!shouldCheck)
    {
        return;
    }

    for (auto torrent : torrents)
    {
        auto status = torrent->Status();

#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailableToCaller;
        ULARGE_INTEGER totalNumberOfBytes;
        ULARGE_INTEGER totalNumberOfFreeBytes;

        BOOL res = GetDiskFreeSpaceEx(
            Utils::toStdWString(status.savePath).c_str(),
            &freeBytesAvailableToCaller,
            &totalNumberOfBytes,
            &totalNumberOfFreeBytes);
#else
        std::error_code spaceError;
        fs::space_info space = fs::space(fs::path(status.savePath), spaceError);
        bool res = !spaceError && space.capacity > 0;
#endif

        if (res)
        {
#ifdef _WIN32
            float diskSpaceAvailable = static_cast<float>(freeBytesAvailableToCaller.QuadPart) / static_cast<float>(totalNumberOfBytes.QuadPart);
#else
            float diskSpaceAvailable = static_cast<float>(space.available) / static_cast<float>(space.capacity);
#endif
            float diskSpaceLimit = limit / 100.0f;

            if (diskSpaceAvailable < diskSpaceLimit)
            {
                BOOST_LOG_TRIVIAL(info) << "Pausing torrent "
                    << status.infoHash << " due to disk space too low (avail: "
                    << diskSpaceAvailable << ", limit: "
                    << diskSpaceLimit << ")";

                torrent->Pause();

                m_taskBarIcon->ShowBalloon(
                    i18n("pause_on_low_disk_space_alert"),
                    status.name);
            }
        }
    }
}

void MainFrame::CreateFilterMenuItems()
{
    for (int i = static_cast<int>(m_filtersMenu->GetMenuItemCount()) - 1; i >= 0; i--)
    {
        wxMenuItem* item = m_filtersMenu->FindItemByPosition(i);
        if (item->GetId() <= ptID_EVT_FILTERS_USER) { continue; }
        m_filtersMenu->Delete(item);
    }

    for (auto const& filter : m_cfg->GetFilters())
    {
        m_filtersMenu->Append(ptID_EVT_FILTERS_USER + filter.id, Utils::toStdWString(filter.name));
    }
}

void MainFrame::CreateLabelMenuItems()
{
    for (int i = static_cast<int>(m_labelsMenu->GetMenuItemCount()) - 1; i >= 0; i--)
    {
        wxMenuItem* item = m_labelsMenu->FindItemByPosition(i);
        if (item->GetId() <= ptID_EVT_LABELS_USER) { continue; }
        m_labelsMenu->Delete(item);
    }

    for (auto const& label : m_cfg->GetLabels())
    {
        m_labelsMenu->AppendRadioItem(ptID_EVT_LABELS_USER + label.id, Utils::toStdWString(label.name));
    }
}

wxMenuBar* MainFrame::CreateMainMenu()
{
    auto fileMenu = new wxMenu();
    fileMenu->Append(ptID_EVT_ADD_TORRENT, i18n("amp_add_torrent"));
    fileMenu->Append(ptID_EVT_ADD_MAGNET_LINK, i18n("amp_add_magnet_link_s"));
    fileMenu->AppendSeparator();
    fileMenu->Append(ptID_EVT_CREATE_TORRENT, i18n("amp_create_torrent"));
    fileMenu->AppendSeparator();
    fileMenu->Append(ptID_EVT_EXIT, i18n("amp_exit"));

    m_viewMenu = new wxMenu();

    m_filtersMenu = new wxMenu();
    m_filtersMenu->Append(ptID_EVT_FILTERS_NONE, i18n("none"));
    m_filtersMenu->Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& evt)
        {
            if (evt.GetId() > ptID_EVT_FILTERS_USER)
            {
                int filterId = evt.GetId() - ptID_EVT_FILTERS_USER;
                if (auto filter = m_cfg->GetFilterById(filterId))
                {
                    m_console->SetText(filter.value().filter);
                }
            }
            else if (evt.GetId() == ptID_EVT_FILTERS_NONE)
            {
                m_console->SetText("");
            }
        });

    m_labelsMenu = new wxMenu();
    m_labelsMenu->AppendRadioItem(ptID_EVT_LABELS_NONE, i18n("none"));
    m_labelsMenu->Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& evt)
        {
            if (evt.GetId() > ptID_EVT_LABELS_USER)
            {
                int labelId = evt.GetId() - ptID_EVT_LABELS_USER;
                m_torrentListModel->SetLabelFilter(labelId);
            }
            else if (evt.GetId() == ptID_EVT_LABELS_NONE)
            {
                m_torrentListModel->ClearLabelFilter();
            }
        });

    m_menuItemFilters = m_viewMenu->AppendSubMenu(m_filtersMenu, i18n("amp_filter"));
    m_viewMenu->AppendSeparator();
    m_menuItemLabels = m_viewMenu->AppendSubMenu(m_labelsMenu, i18n("labels"));
    m_viewMenu->AppendSeparator();

    m_menuItemConsoleInput = m_viewMenu->Append(ptID_EVT_SHOW_CONSOLE, i18n("amp_console"));
    m_menuItemDetailsPanel = m_viewMenu->Append(ptID_EVT_SHOW_DETAILS, i18n("amp_details_panel"));
    m_menuItemStatusBar = m_viewMenu->Append(ptID_EVT_SHOW_STATUS_BAR, i18n("amp_status_bar"));
    m_viewMenu->AppendSeparator();
#ifdef __WXOSX__
    // With the standard id, macOS shows it in the application menu (HNTorrent), with Cmd+,
    m_viewMenu->Append(wxID_PREFERENCES, i18n("amp_preferences") + L"\tCtrl+,");
#else
    m_viewMenu->Append(ptID_EVT_VIEW_PREFERENCES, i18n("amp_preferences"));
#endif

    auto helpMenu = new wxMenu();
#ifdef _WIN32
    // The updater only exists on Windows
    helpMenu->Append(ptID_EVT_CHECK_FOR_UPDATE, i18n("amp_check_for_update"));
    helpMenu->AppendSeparator();
#endif
    helpMenu->Append(ptID_EVT_VIEW_HELP, i18n("view_help"));
#ifdef __WXOSX__
    // It goes to the application menu of macOS, where it says "About HNTorrent"
    helpMenu->Append(wxID_ABOUT, i18n("about_picotorrent"));
#else
    helpMenu->Append(ptID_EVT_ABOUT, i18n("amp_about"));
#endif

    auto mainMenu = new wxMenuBar();
    mainMenu->Append(fileMenu, i18n("amp_file"));
    mainMenu->Append(m_viewMenu, i18n("amp_view"));
    mainMenu->Append(helpMenu, i18n("amp_help"));

    return mainMenu;
}

void MainFrame::CreateMainToolBar()
{
    auto icon = [](const char* svg) { return wxBitmapBundle::FromSVG(svg, wxSize(24, 24)); };

    auto toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT | wxTB_NODIVIDER);
    toolBar->SetBackgroundColour(Theme::Colors().window);
    toolBar->AddTool(ptID_EVT_ADD_TORRENT, i18n("toolbar_add_torrent"), icon(ToolbarIcons::AddTorrent), i18n("toolbar_add_torrent_help"));
    toolBar->AddTool(ptID_EVT_ADD_MAGNET_LINK, i18n("toolbar_add_magnet_link"), icon(ToolbarIcons::AddMagnetLink), i18n("toolbar_add_magnet_link_help"));
    toolBar->AddSeparator();
    toolBar->AddTool(ptID_TOOLBAR_RESUME, i18n("resume"), icon(ToolbarIcons::Resume), i18n("toolbar_resume_help"));
    toolBar->AddTool(ptID_TOOLBAR_PAUSE, i18n("pause"), icon(ToolbarIcons::Pause), i18n("toolbar_pause_help"));
    toolBar->AddSeparator();
    toolBar->AddTool(ptID_TOOLBAR_REMOVE, i18n("toolbar_remove"), icon(ToolbarIcons::Remove), i18n("toolbar_remove_help"));

    // Search box on the right: shows only the torrents whose name contains the text
    auto search = new wxSearchCtrl(toolBar, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(220, -1)));
    search->SetDescriptiveText(i18n("search_torrents"));
    search->Bind(wxEVT_TEXT, [this](wxCommandEvent& evt)
        {
            if (evt.GetString().IsEmpty()) { m_torrentListModel->ClearFilter(); }
            else { m_torrentListModel->SetFilter(std::make_unique<NameFilter>(evt.GetString())); }
        });

    toolBar->AddStretchableSpace();
    toolBar->AddControl(search);
    toolBar->Realize();
}

void MainFrame::OnToolBarRemove(wxCommandEvent&)
{
    if (m_selection.empty()) { return; }

    if (!ConfirmRemove(this, m_selection.size(), false, "the toolbar"))
    {
        return;
    }

    // Copy first: removing torrents changes the selection
    std::vector<BitTorrent::TorrentHandle*> torrents;
    for (auto const& sel : m_selection) { torrents.push_back(sel.second); }
    for (auto torrent : torrents) { torrent->Remove(); }
}

void MainFrame::OnToolBarUpdateUI(wxUpdateUIEvent& evt)
{
    bool anyPaused = false;
    bool anyRunning = false;

    for (auto const& sel : m_selection)
    {
        if (sel.second->Status().paused) { anyPaused = true; }
        else { anyRunning = true; }
    }

    switch (evt.GetId())
    {
    case ptID_TOOLBAR_RESUME: evt.Enable(anyPaused); break;
    case ptID_TOOLBAR_PAUSE: evt.Enable(anyRunning); break;
    case ptID_TOOLBAR_REMOVE: evt.Enable(!m_selection.empty()); break;
    }
}

void MainFrame::OnClose(wxCloseEvent& evt)
{
    if (evt.CanVeto()
        && m_cfg->Get<bool>("show_in_notification_area").value()
        && m_cfg->Get<bool>("close_to_notification_area").value())
    {
        Hide();
#ifdef __WXMSW__
        MSWGetTaskBarButton()->Hide();
#endif
    }
    else
    {
        // We hide early while closing to not occupy the
        // screen more than necessary. Otherwise the window
        // would be visible (and unresponsive) for a few seconds
        // before being destroyed.
        Hide();

        evt.Skip();
    }
}

void MainFrame::OnFileAddMagnetLink(wxCommandEvent&)
{
    Dialogs::AddMagnetLinkDialog dlg(this, wxID_ANY);

    if (dlg.ShowModal() == wxID_OK)
    {
        auto params = dlg.GetParams();
        this->AddTorrents(params, false);
    }
}

void MainFrame::OnFileAddTorrent(wxCommandEvent&)
{
#ifdef _WIN32
    Win32::OpenFileDialog ofd;

    ofd.SetFileTypes({
        std::make_tuple(L"Torrent files", L"*.torrent"),
        std::make_tuple(L"All files (*.*)", L"*.*")
    });

    ofd.SetOption(Win32::OpenFileDialog::Option::Multi);
    ofd.SetTitle(i18n("add_torrent_s"));
    ofd.Show(this);

    std::vector<std::wstring> files;
    ofd.GetFiles(files);
#else
    // macOS: the system dialog, only for .torrent files (with a single type it shows no list
    // of types, which would be in English)
    wxFileDialog ofd(
        this,
        i18n("add_torrent_s"),
        wxEmptyString,
        wxEmptyString,
        "*.torrent",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

    std::vector<std::wstring> files;

    if (ofd.ShowModal() == wxID_OK)
    {
        wxArrayString paths;
        ofd.GetPaths(paths);

        for (wxString const& path : paths)
        {
            files.push_back(path.ToStdWstring());
        }
    }
#endif

    if (files.empty())
    {
        return;
    }

    std::vector<lt::add_torrent_params> params;
    this->ParseTorrentFiles(params, files);
    this->AddTorrents(params, false);
}

void MainFrame::OnFileCreateTorrent(wxCommandEvent&)
{
    auto dlg = new Dialogs::CreateTorrentDialog(this, wxID_ANY, m_session);
    dlg->Show();
    dlg->Bind(wxEVT_CLOSE_WINDOW,
        [dlg](wxCloseEvent&)
        {
            dlg->Destroy();
        });
}

void MainFrame::OnViewHelp(wxCommandEvent&)
{
    wxLaunchDefaultBrowser("https://docs.picotorrent.org");
}

void MainFrame::OnHelpAbout(wxCommandEvent&)
{
    Dialogs::AboutDialog dlg(this, wxID_ANY);
    dlg.ShowModal();
}

void MainFrame::OnHelpCheckForUpdate(wxCommandEvent&)
{
    m_updateChecker->Check(true);
}

void MainFrame::OnIconize(wxIconizeEvent& ev)
{
    if (ev.IsIconized()
        && m_cfg->Get<bool>("show_in_notification_area").value()
        && m_cfg->Get<bool>("minimize_to_notification_area").value())
    {
#ifdef __WXMSW__
        MSWGetTaskBarButton()->Hide();
#endif
    }
}

#ifdef __WXOSX__
void MainFrame::ShowAndActivate()
{
    bool wasHidden = !IsShown();

    if (wasHidden)
    {
        Show(true);
    }

    if (IsIconized())
    {
        Iconize(false);
        wasHidden = true;
    }

    // A window that was visible comes forward with the application; raising it while a dialog
    // of HNTorrent is open would put it over the dialog
    if (wasHidden)
    {
        Raise();
    }

    Mac::ActivateApplication();
}
#endif

void MainFrame::OnTaskBarLeftDown(wxTaskBarIconEvent&)
{
#ifdef __WXMSW__
    this->MSWGetTaskBarButton()->Show();
#endif

    if (this->IsIconized())
    {
        this->Restore();
    }

    this->Raise();
    this->Show();
    this->SendSizeEvent();
}

void MainFrame::OnViewPreferences(wxCommandEvent&)
{
    Dialogs::PreferencesDialog dlg(this, m_cfg);
    
    if (dlg.ShowModal() == wxID_OK)
    {
        if (dlg.WantsRestart())
        {
#ifdef _WIN32
            TCHAR path[MAX_PATH];
            GetModuleFileName(NULL, path, ARRAYSIZE(path));

            std::wstringstream proc;
            proc << path << L" --wait-for-pid=" << std::to_wstring(GetCurrentProcessId());

            wxExecute(proc.str(), wxEXEC_ASYNC);
#else
            // The same program again (HNTorrent.app/Contents/MacOS/HNTorrent), which waits
            // for this one to exit
            std::string exe = wxStandardPaths::Get().GetExecutablePath().utf8_string();
            std::string wait = "--wait-for-pid=" + std::to_string(wxGetProcessId());
            const char* const argv[] = { exe.c_str(), wait.c_str(), nullptr };

            wxExecute(argv, wxEXEC_ASYNC);
#endif
            Close(true);

            return;
        }

        // Reload settings
        m_session->ReloadSettings();

        if (m_cfg->Get<bool>("show_in_notification_area").value() && !m_taskBarIcon->IsIconInstalled())
        {
            m_taskBarIcon->Show();
        }
        else if (!m_cfg->Get<bool>("show_in_notification_area").value() && m_taskBarIcon->IsIconInstalled())
        {
            m_taskBarIcon->Hide();
        }

        m_torrentDetails->ReloadConfiguration();
        m_torrentListModel->SetBackgroundColorEnabled(
            m_cfg->Get<bool>("use_label_as_list_bgcolor").value());

        this->CreateLabelMenuItems();
        this->UpdateLabels();
    }
}

void MainFrame::ParseTorrentFiles(std::vector<lt::add_torrent_params>& params, std::vector<std::wstring> const& paths)
{
    for (std::wstring const& path : paths)
    {
        lt::error_code ec;
        lt::add_torrent_params param;

#ifdef _WIN32
        std::ifstream in(path, std::ios::binary);
#else
        // Only the Windows library opens a file from a wide string
        std::ifstream in(fs::path(path), std::ios::binary);
#endif
        std::stringstream ss;
        ss << in.rdbuf();

        param.ti = std::make_shared<lt::torrent_info>(
            ss.str().data(),
            static_cast<int>(ss.str().size()),
            ec);

        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Failed to parse torrent file: " << ec.message();
            continue;
        }

        params.push_back(param);
    }
}

void MainFrame::ShowTorrentContextMenu(wxCommandEvent&)
{
    wxDataViewItemArray items;
    m_torrentList->GetSelections(items);

    if (items.IsEmpty())
    {
        return;
    }

    std::vector<BitTorrent::TorrentHandle*> selectedTorrents;

    for (wxDataViewItem& item : items)
    {
        selectedTorrents.push_back(
            m_torrentListModel->GetTorrentFromItem(item));
    }

    TorrentContextMenu menu(this, m_cfg, selectedTorrents);
    PopupMenu(&menu);
}

void MainFrame::UpdateLabels()
{
    std::map<int, std::tuple<std::string, std::string>> labels;

    for (auto const& label : m_cfg->GetLabels())
    {
        labels.insert({ label.id, { label.name, label.color } });
    }

    m_torrentListModel->UpdateLabels(labels, FromDIP(LABEL_ICON_SIZE));
}
