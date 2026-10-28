#pragma once

// Icons for the main toolbar: thin grey line drawings, as in the HNTorrent design. They are
// SVG so they stay sharp at every DPI (rendered by NanoSVG).
namespace pt::UI::ToolbarIcons
{
    inline const char* AddTorrent = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<path d="M12 5v14M5 12h14" fill="none" stroke="#5F6B7A" stroke-width="2" stroke-linecap="round"/>
</svg>)";

    inline const char* AddMagnetLink = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<path d="M6.5 4.5v8a5.5 5.5 0 0 0 11 0v-8" fill="none" stroke="#5F6B7A" stroke-width="2" stroke-linejoin="round"/>
<path d="M6.5 8.5h3M14.5 8.5h3" fill="none" stroke="#5F6B7A" stroke-width="2"/>
</svg>)";

    inline const char* Resume = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<path d="M8 5.5v13l10.5-6.5z" fill="none" stroke="#5F6B7A" stroke-width="2" stroke-linejoin="round"/>
</svg>)";

    inline const char* Pause = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<path d="M9 6v12M15 6v12" fill="none" stroke="#5F6B7A" stroke-width="2.4" stroke-linecap="round"/>
</svg>)";

    inline const char* Remove = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<path d="M7 7l10 10M17 7L7 17" fill="none" stroke="#5F6B7A" stroke-width="2" stroke-linecap="round"/>
</svg>)";
}
