#include "macapp.hpp"

#import <AppKit/AppKit.h>

void pt::UI::Mac::ActivateApplication()
{
    // macOS 14 replaced activateIgnoringOtherApps: with activate
    if (@available(macOS 14.0, *))
    {
        [NSApp activate];
    }
    else
    {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void pt::UI::Mac::MoveTableColumn(void* dataViewHandle, int from, int to)
{
    // The handle of a wxDataViewCtrl is the scroll view around its table
    NSView* view = (NSView*)dataViewHandle;
    NSTableView* table = nil;

    if ([view isKindOfClass:[NSScrollView class]])
    {
        NSView* document = [(NSScrollView*)view documentView];

        if ([document isKindOfClass:[NSTableView class]])
        {
            table = (NSTableView*)document;
        }
    }
    else if ([view isKindOfClass:[NSTableView class]])
    {
        table = (NSTableView*)view;
    }

    if (table == nil || from == to
        || from < 0 || from >= [table numberOfColumns]
        || to < 0 || to >= [table numberOfColumns])
    {
        return;
    }

    [table moveColumn:from toColumn:to];
}
