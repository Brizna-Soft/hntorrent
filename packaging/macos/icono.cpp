// Draws the icon of HNTorrent.app: the PNG files of an .iconset folder, which iconutil turns
// into HNTorrent.icns. The logo is the same SVG that the program shows (ui/macicons.hpp),
// drawn with NanoSVG, the SVG library that comes with wxWidgets.
//
// Usage: HNTorrent-icono <folder>.iconset

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "ui/macicons.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <folder>.iconset\n", argv[0]);
        return 1;
    }

    std::filesystem::path folder = argv[1];
    std::filesystem::create_directories(folder);

    // NanoSVG changes the text while it reads it, so it gets a copy
    const char* logo = pt::UI::MacIcons::AppLogo;
    std::vector<char> svg(logo, logo + std::strlen(logo) + 1);
    NSVGimage* image = nsvgParse(svg.data(), "px", 96.0f);

    if (image == nullptr || image->width <= 0)
    {
        std::fprintf(stderr, "could not read the logo\n");
        return 1;
    }

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();

    // The sizes that iconutil expects: 16, 32, 128, 256 and 512 points, each one also at double size
    const int sizes[] = { 16, 32, 128, 256, 512 };

    for (int points : sizes)
    {
        for (int scale = 1; scale <= 2; scale++)
        {
            int size = points * scale;

            // macOS icons leave a margin around the square: 824 of 1024 pixels, centred
            float side = size * 824.0f / 1024.0f;
            float offset = (size - side) / 2.0f;

            std::vector<unsigned char> pixels(static_cast<size_t>(size) * size * 4, 0);
            nsvgRasterize(rasterizer, image, offset, offset, side / image->width, pixels.data(), size, size, size * 4);

            std::string name = "icon_" + std::to_string(points) + "x" + std::to_string(points) + (scale == 2 ? "@2x" : "") + ".png";
            std::string file = (folder / name).string();

            if (!stbi_write_png(file.c_str(), size, size, 4, pixels.data(), size * 4))
            {
                std::fprintf(stderr, "could not write %s\n", file.c_str());
                return 1;
            }
        }
    }

    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);

    return 0;
}
