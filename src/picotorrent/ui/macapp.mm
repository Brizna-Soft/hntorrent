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
