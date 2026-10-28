#pragma once

// Icons for the main toolbar, drawn in the style of the Brizna app icon: a rounded square
// with a white symbol. They are SVG so they stay sharp at every DPI (rendered by NanoSVG).
namespace pt::UI::ToolbarIcons
{
    inline const char* AddTorrent = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<rect x="1.5" y="1.5" width="21" height="21" rx="5" fill="#2E7D32"/>
<path d="M12 6.5v11M6.5 12h11" fill="none" stroke="#FFFFFF" stroke-width="2.6" stroke-linecap="round"/>
</svg>)";

    inline const char* AddMagnetLink = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<rect x="1.5" y="1.5" width="21" height="21" rx="5" fill="#2E7D32"/>
<path d="M7.5 6v6.5a4.5 4.5 0 0 0 9 0V6" fill="none" stroke="#FFFFFF" stroke-width="2.6"/>
<path d="M7.5 6v2.6M16.5 6v2.6" fill="none" stroke="#A5D6A7" stroke-width="2.6"/>
</svg>)";

    inline const char* Resume = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<rect x="1.5" y="1.5" width="21" height="21" rx="5" fill="#388E3C"/>
<path d="M9.5 7.2v9.6l7.4-4.8z" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="1" stroke-linejoin="round"/>
</svg>)";

    inline const char* Pause = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<rect x="1.5" y="1.5" width="21" height="21" rx="5" fill="#EF6C00"/>
<path d="M9.5 7.5v9M14.5 7.5v9" fill="none" stroke="#FFFFFF" stroke-width="2.6" stroke-linecap="round"/>
</svg>)";

    inline const char* Remove = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
<rect x="1.5" y="1.5" width="21" height="21" rx="5" fill="#D32F2F"/>
<path d="M8.2 8.2l7.6 7.6M15.8 8.2l-7.6 7.6" fill="none" stroke="#FFFFFF" stroke-width="2.6" stroke-linecap="round"/>
</svg>)";
}
