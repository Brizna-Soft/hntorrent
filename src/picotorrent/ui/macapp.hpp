#pragma once

// macOS only (macapp.mm): what wxWidgets does not offer
namespace pt::UI::Mac
{
    // Brings HNTorrent in front of the other applications, as a click on its Dock icon does
    void ActivateApplication();

    // Moves a column of the native table inside a wxDataViewCtrl (GetHandle()) from one shown
    // position to another, as dragging its header does. wxWidgets can only add and delete
    // columns there, and on macOS deleting one destroys it.
    void MoveTableColumn(void* dataViewHandle, int from, int to);
}
