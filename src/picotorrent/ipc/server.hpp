#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/ipc.h>

namespace pt
{
namespace UI
{
    class MainFrame;
}
namespace IPC
{
    class Server : public wxServer
    {
    public:
        Server(UI::MainFrame* mainFrame);
        virtual ~Server();

        virtual wxConnectionBase* OnAcceptConnection(const wxString& topic) wxOVERRIDE;

#ifndef __WXMSW__
        // Outside Windows (where the server is the DDE service "HNTorrent") it listens on this
        // Unix socket, in the temporary folder of the user
        static wxString SocketPath();
#endif

    private:
        UI::MainFrame* m_frame;
    };
}
}
