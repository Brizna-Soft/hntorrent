#include "server.hpp"

#include "applicationoptionsconnection.hpp"
#include "../ui/mainframe.hpp"

#ifndef __WXMSW__
#include <wx/filename.h>
#endif

using pt::IPC::Server;

Server::Server(pt::UI::MainFrame* mainFrame)
    : m_frame(mainFrame)
{
#ifdef __WXMSW__
    Create("HNTorrent");
#else
    Create(SocketPath());
#endif
}

#ifndef __WXMSW__
wxString Server::SocketPath()
{
    // wxWidgets takes a name with a "/" as a Unix socket; a plain name would be a TCP port
    return wxFileName(wxFileName::GetTempDir(), "HNTorrent.ipc").GetFullPath();
}
#endif

Server::~Server()
{
}

wxConnectionBase* Server::OnAcceptConnection(const wxString& topic)
{
    if (topic == "ApplicationOptions")
    {
        return new ApplicationOptionsConnection(m_frame);
    }

    return nullptr;
}
