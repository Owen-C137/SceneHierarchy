// Draws Radiance's vector glyphs (RadianceIcons.h) into a pixel buffer: no Qt, so it can be unit tested.
// A glyph is SVG-style path data on a 16x16 grid. Filled parts use the even-odd rule (Avalonia's default); stroked
// parts are 1.3 wide with round caps and joins, exactly as Radiance's Icon control draws them.
#pragma once
#include "RadianceIcons.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace glyph
{
    struct Pt { double x, y; };
    struct SubPath { std::vector<Pt> pts; bool closed = false; };

    namespace detail
    {
        constexpr double kPi = 3.14159265358979323846;

        struct Lexer
        {
            const char* p;
            void Skip() { while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') ++p; }
            bool Number(double& out)
            {
                Skip();
                char* end = nullptr;
                out = std::strtod(p, &end);
                if (end == p) return false;
                p = end;
                return true;
            }
            // an arc flag is a single 0 or 1 that may touch the next number
            bool Flag(bool& out)
            {
                Skip();
                if (*p != '0' && *p != '1') return false;
                out = *p == '1'; ++p;
                return true;
            }
        };

        inline void Cubic(std::vector<Pt>& o, Pt a, Pt b, Pt c, Pt d)
        {
            const int n = 16;
            for (int i = 1; i <= n; ++i)
            {
                double t = double(i) / n, u = 1 - t;
                o.push_back({ u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                              u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y });
            }
        }

        // SVG endpoint arc -> centre parameterisation (SVG 1.1 appendix F.6), flattened.
        inline void Arc(std::vector<Pt>& o, Pt from, double rx, double ry, double rotDeg, bool large, bool sweep, Pt to)
        {
            if ((from.x == to.x && from.y == to.y)) return;
            rx = std::fabs(rx); ry = std::fabs(ry);
            if (rx == 0 || ry == 0) { o.push_back(to); return; }
            double phi = rotDeg * kPi / 180, cp = std::cos(phi), sp = std::sin(phi);
            double dx = (from.x - to.x) / 2, dy = (from.y - to.y) / 2;
            double x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
            double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
            if (lambda > 1) { double s = std::sqrt(lambda); rx *= s; ry *= s; }
            double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
            double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
            double co = den == 0 ? 0 : std::sqrt(std::max(0.0, num / den));
            if (large == sweep) co = -co;
            double cxp = co * rx * y1 / ry, cyp = -co * ry * x1 / rx;
            double cx = cp * cxp - sp * cyp + (from.x + to.x) / 2, cy = sp * cxp + cp * cyp + (from.y + to.y) / 2;
            auto ang = [](double ux, double uy, double vx, double vy)
            {
                double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
                return a;
            };
            double th1 = ang(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry);
            double dth = ang((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
            if (!sweep && dth > 0) dth -= 2 * kPi;
            else if (sweep && dth < 0) dth += 2 * kPi;
            int n = std::max(6, int(std::ceil(std::fabs(dth) / (kPi / 12))));
            for (int i = 1; i <= n; ++i)
            {
                double t = th1 + dth * i / n, ex = rx * std::cos(t), ey = ry * std::sin(t);
                o.push_back({ cp * ex - sp * ey + cx, sp * ex + cp * ey + cy });
            }
            o.back() = to;
        }
    }

    // Parses absolute M/L/C/Q/A/Z path data (all that Radiance's icons use) into polylines.
    inline std::vector<SubPath> ParsePath(const char* d)
    {
        std::vector<SubPath> out;
        detail::Lexer lx{ d };
        Pt cur{ 0, 0 }, start{ 0, 0 };
        char cmd = 0;
        for (;;)
        {
            lx.Skip();
            if (!*lx.p) break;
            if (std::isalpha(static_cast<unsigned char>(*lx.p))) cmd = *lx.p++;
            else if (!cmd) break;
            switch (cmd)
            {
            case 'M':
            {
                double x, y;
                if (!lx.Number(x) || !lx.Number(y)) return out;
                out.emplace_back(); out.back().pts.push_back({ x, y });
                cur = start = { x, y }; cmd = 'L';   // further pairs are line-to
                break;
            }
            case 'L':
            {
                double x, y;
                if (!lx.Number(x) || !lx.Number(y) || out.empty()) return out;
                out.back().pts.push_back({ x, y }); cur = { x, y };
                break;
            }
            case 'C':
            {
                double v[6];
                for (double& n : v) if (!lx.Number(n)) return out;
                if (out.empty()) return out;
                detail::Cubic(out.back().pts, cur, { v[0], v[1] }, { v[2], v[3] }, { v[4], v[5] });
                cur = { v[4], v[5] };
                break;
            }
            case 'Q':
            {
                double v[4];
                for (double& n : v) if (!lx.Number(n)) return out;
                if (out.empty()) return out;
                Pt c1{ cur.x + 2.0 / 3 * (v[0] - cur.x), cur.y + 2.0 / 3 * (v[1] - cur.y) };
                Pt c2{ v[2] + 2.0 / 3 * (v[0] - v[2]), v[3] + 2.0 / 3 * (v[1] - v[3]) };
                detail::Cubic(out.back().pts, cur, c1, c2, { v[2], v[3] });
                cur = { v[2], v[3] };
                break;
            }
            case 'A':
            {
                double rx, ry, rot, x, y; bool large, sweep;
                if (!lx.Number(rx) || !lx.Number(ry) || !lx.Number(rot) || !lx.Flag(large) || !lx.Flag(sweep) || !lx.Number(x) || !lx.Number(y)) return out;
                if (out.empty()) return out;
                detail::Arc(out.back().pts, cur, rx, ry, rot, large, sweep, { x, y });
                cur = { x, y };
                break;
            }
            case 'Z': case 'z':
                if (!out.empty()) { out.back().closed = true; cur = start; }
                cmd = 0;
                break;
            default:
                return out;   // a command we do not draw
            }
        }
        return out;
    }

    inline const radiance_icons::Glyph* Find(const char* name)
    {
        if (!name) return nullptr;
        for (int i = 0; i < radiance_icons::kGlyphCount; ++i)
            if (std::strcmp(radiance_icons::kGlyphs[i].name, name) == 0) return &radiance_icons::kGlyphs[i];
        return nullptr;
    }

    namespace detail
    {
        inline double DistSq(Pt p, Pt a, Pt b)
        {
            double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
            double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0;
            t = std::max(0.0, std::min(1.0, t));
            double ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
            return ex * ex + ey * ey;
        }
        struct Seg { Pt a, b; };
    }

    // Renders `g` at size x size pixels in colour 0xRRGGBB into premultiplied BGRA (QImage::Format_ARGB32_Premultiplied
    // on little-endian). 4x4 samples per pixel.
    inline void Render(const radiance_icons::Glyph& g, int size, uint32_t rgb, std::vector<uint8_t>& bgra, double strokeWidth = 1.3)
    {
        const int ss = 4;
        const double scale = size / 16.0;
        std::vector<uint8_t> hit(size_t(size) * size * ss * ss, 0);   // sub-samples covered by any part
        for (int part = 0; part < g.count; ++part)
        {
            std::vector<SubPath> paths = ParsePath(g.parts[part].path);
            const bool fill = g.parts[part].mode == 'F' || g.parts[part].mode == 'B';
            const bool stroke = g.parts[part].mode == 'S' || g.parts[part].mode == 'B';
            std::vector<detail::Seg> segs, edges;
            for (const SubPath& sp : paths)
            {
                for (size_t i = 0; i + 1 < sp.pts.size(); ++i) segs.push_back({ sp.pts[i], sp.pts[i + 1] });
                if (sp.closed && sp.pts.size() > 1) segs.push_back({ sp.pts.back(), sp.pts.front() });
                if (fill && sp.pts.size() > 2) // fills close implicitly
                {
                    for (size_t i = 0; i + 1 < sp.pts.size(); ++i) edges.push_back({ sp.pts[i], sp.pts[i + 1] });
                    edges.push_back({ sp.pts.back(), sp.pts.front() });
                }
            }
            const double r2 = (strokeWidth / 2) * (strokeWidth / 2);
            for (int py = 0; py < size; ++py)
                for (int px = 0; px < size; ++px)
                    for (int sy = 0; sy < ss; ++sy)
                        for (int sx = 0; sx < ss; ++sx)
                        {
                            Pt p{ (px + (sx + 0.5) / ss) / scale, (py + (sy + 0.5) / ss) / scale };
                            bool in = false;
                            if (fill)
                            {
                                int crossings = 0;
                                for (const detail::Seg& e : edges)
                                {
                                    if ((e.a.y > p.y) == (e.b.y > p.y)) continue;
                                    double x = e.a.x + (p.y - e.a.y) * (e.b.x - e.a.x) / (e.b.y - e.a.y);
                                    if (x > p.x) ++crossings;
                                }
                                in = (crossings & 1) != 0;
                            }
                            if (!in && stroke)
                                for (const detail::Seg& s : segs)
                                    if (detail::DistSq(p, s.a, s.b) <= r2) { in = true; break; }
                            if (in) hit[((size_t(py) * size + px) * ss + sy) * ss + sx] = 1;
                        }
        }
        bgra.assign(size_t(size) * size * 4, 0);
        std::vector<float> cover(size_t(size) * size, 0.f);
        for (size_t i = 0; i < cover.size(); ++i)
        {
            int n = 0;
            for (int k = 0; k < ss * ss; ++k) n += hit[i * ss * ss + k];
            cover[i] = float(n) / (ss * ss);
        }
        const double cr = (rgb >> 16 & 0xFF), cg = (rgb >> 8 & 0xFF), cb = (rgb & 0xFF);
        for (size_t i = 0; i < cover.size(); ++i)
        {
            double a = std::min(1.0, double(cover[i]));
            bgra[i * 4 + 0] = uint8_t(std::lround(cb * a));
            bgra[i * 4 + 1] = uint8_t(std::lround(cg * a));
            bgra[i * 4 + 2] = uint8_t(std::lround(cr * a));
            bgra[i * 4 + 3] = uint8_t(std::lround(255 * a));
        }
    }
}
