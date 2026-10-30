#pragma once

// macOS only. On Windows these icons are .ico files in resources.rc (AppIcon, ICO_TERMINAL and
// ICO_TERMINAL_DARK_THEME), which macOS cannot load, so here they are SVG and drawn by NanoSVG
// like the toolbar icons. packaging/macos/icono.cpp also draws HNTorrent.icns from AppLogo.
namespace pt::UI::MacIcons
{
    // The HNTorrent logo: blue square, download arrow and the five stars of the flag. The text
    // has a parenthesis followed by a quote, so this raw string uses the delimiter "svg".
    inline const char* AppLogo = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 120 120">
<rect x="0" y="0" width="120" height="120" rx="28" fill="#1A8FD6"/>
<g fill="#FFFFFF">
<path transform="translate(60,28)" d="M0,-6 L1.47,-2.02 L5.71,-1.85 L2.38,0.77 L3.53,4.85 L0,2.5 L-3.53,4.85 L-2.38,0.77 L-5.71,-1.85 L-1.47,-2.02 Z"/>
<path transform="translate(42,18)" d="M0,-6 L1.47,-2.02 L5.71,-1.85 L2.38,0.77 L3.53,4.85 L0,2.5 L-3.53,4.85 L-2.38,0.77 L-5.71,-1.85 L-1.47,-2.02 Z"/>
<path transform="translate(78,18)" d="M0,-6 L1.47,-2.02 L5.71,-1.85 L2.38,0.77 L3.53,4.85 L0,2.5 L-3.53,4.85 L-2.38,0.77 L-5.71,-1.85 L-1.47,-2.02 Z"/>
<path transform="translate(42,38)" d="M0,-6 L1.47,-2.02 L5.71,-1.85 L2.38,0.77 L3.53,4.85 L0,2.5 L-3.53,4.85 L-2.38,0.77 L-5.71,-1.85 L-1.47,-2.02 Z"/>
<path transform="translate(78,38)" d="M0,-6 L1.47,-2.02 L5.71,-1.85 L2.38,0.77 L3.53,4.85 L0,2.5 L-3.53,4.85 L-2.38,0.77 L-5.71,-1.85 L-1.47,-2.02 Z"/>
<rect x="52" y="50" width="16" height="26"/>
<polygon points="36,74 84,74 60,98"/>
<rect x="32" y="102" width="56" height="6" rx="3"/>
</g>
</svg>)svg";

    // The prompt next to the filter box (the console), for the light and the dark theme
    inline const char* Terminal = R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M3 4.5l3.5 3.5L3 11.5M8.5 11.5H13" fill="none" stroke="#5F6B7A" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)";

    inline const char* TerminalDark = R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M3 4.5l3.5 3.5L3 11.5M8.5 11.5H13" fill="none" stroke="#A0A7B0" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)";
}
