// Checks of the glyph rasterizer (GlyphRaster.h) against every glyph in RadianceIcons.h.
#include "../GlyphRaster.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void Dump(const char* name, int size)
{
    const radiance_icons::Glyph* g = glyph::Find(name);
    std::vector<uint8_t> px; glyph::Render(*g, size, 0xFFFFFF, px);
    std::printf("-- %s %dpx\n", name, size);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x) { int a = px[(size_t(y) * size + x) * 4 + 3]; std::putchar(a > 190 ? '#' : a > 100 ? '+' : a > 30 ? '.' : ' '); }
        std::putchar('\n');
    }
}

int main(int argc, char** argv)
{
    CHECK(radiance_icons::kGlyphCount >= 60, "only %d glyphs", radiance_icons::kGlyphCount);
    for (int i = 0; i < radiance_icons::kGlyphCount; ++i)
    {
        const radiance_icons::Glyph& g = radiance_icons::kGlyphs[i];
        for (int p = 0; p < g.count; ++p)
        {
            auto paths = glyph::ParsePath(g.parts[p].path);
            CHECK(!paths.empty(), "%s part %d did not parse", g.name, p);
            for (auto& sp : paths) CHECK(sp.pts.size() >= 2, "%s part %d has a one-point subpath", g.name, p);
            for (auto& sp : paths) for (auto& pt : sp.pts) CHECK(pt.x > -0.1 && pt.x < 16.1 && pt.y > -0.1 && pt.y < 16.1, "%s point out of the grid (%g,%g)", g.name, pt.x, pt.y);
        }
        for (int size : { 12, 16, 24 })
        {
            std::vector<uint8_t> px; glyph::Render(g, size, 0x6FA8F0, px);
            CHECK(px.size() == size_t(size) * size * 4, "%s size", g.name);
            int lit = 0, maxA = 0;
            for (size_t k = 0; k < px.size(); k += 4) { if (px[k + 3]) ++lit; maxA = std::max<int>(maxA, px[k + 3]); }
            CHECK(lit >= size / 2, "%s at %dpx is nearly empty (%d px)", g.name, size, lit);
            CHECK(maxA >= 100, "%s at %dpx is too faint (%d)", g.name, size, maxA);
            for (size_t k = 0; k < px.size(); k += 4) CHECK(px[k] <= px[k + 3] && px[k + 1] <= px[k + 3] && px[k + 2] <= px[k + 3], "%s not premultiplied", g.name);
        }
    }
    // colour order is BGRA premultiplied
    {
        std::vector<uint8_t> px; glyph::Render(*glyph::Find("stop"), 16, 0x102030, px);
        size_t c = (8 * 16 + 8) * 4;
        CHECK(px[c] == 0x30 && px[c + 1] == 0x20 && px[c + 2] == 0x10 && px[c + 3] == 255, "stop centre %02x %02x %02x %02x", px[c], px[c + 1], px[c + 2], px[c + 3]);
        CHECK(px[0 * 4 + 3] == 0, "stop corner should be clear");
    }
    // a filled square is half covered on a half-pixel edge: stop spans 3.5..12.5 on a 16 grid
    {
        std::vector<uint8_t> px; glyph::Render(*glyph::Find("stop"), 16, 0xFFFFFF, px);
        CHECK(px[(8 * 16 + 3) * 4 + 3] > 100 && px[(8 * 16 + 3) * 4 + 3] < 160, "left edge alpha %d", px[(8 * 16 + 3) * 4 + 3]);
    }
    CHECK(glyph::Find("nope") == nullptr && glyph::Find(nullptr) == nullptr, "unknown glyph name");
    if (argc > 1) for (int i = 1; i < argc; ++i) Dump(argv[i], 16);
    if (g_fail) { std::printf("%d glyph checks failed\n", g_fail); return 1; }
    std::printf("All glyph checks passed (%d glyphs).\n", radiance_icons::kGlyphCount);
    return 0;
}
