#pragma once

// macOS only (macapp.mm): what wxWidgets does not offer
namespace pt::UI::Mac
{
    // Brings HNTorrent in front of the other applications, as a click on its Dock icon does
    void ActivateApplication();
}
