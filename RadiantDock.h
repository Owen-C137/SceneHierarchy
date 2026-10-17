// RadiantDock.h: dock a pane along an outer edge of Radiant's whole dock area (full width or full height), which
// Radiant's drag and drop cannot do (it only places a pane beside, below or over one other pane).
//
// Radiant's TADockContainer::add(container, dock, fromRestore, orientation, side, relativeTo, tabify) with no
// relativeTo anchors at the root of the layout tree; when the root runs the other way it wraps the whole layout in
// a new split of the requested orientation. Orientation 1 = side by side, 2 = stacked; side 1 = after (bottom /
// right), 0 = before (top / left). Read from the disassembly of 0x140E7E760; the call sequence (remove, add,
// relayout, toggleView) is the host's own dock::TabWith. Addresses are the thunks in the host's radiant_addrs.h.
#pragma once
#include "modtools_plugin.hpp"
#include "QtLite.h"

namespace rdock
{
    enum Edge { Bottom, Top, Left, Right };

    // The Radiant dock widget (TADockWidget) that holds `content`, found through the QObject parents.
    inline void* DockOf(void* content)
    {
        for (void* o = content; o; )
        {
            if (ql::Inherits(o, "TADockWidget")) return o;
            void* d = *reinterpret_cast<void**>(static_cast<char*>(o) + 8);      // QObject::d_ptr
            o = d ? *reinterpret_cast<void**>(static_cast<char*>(d) + 16) : nullptr;   // QObjectData::parent
        }
        return nullptr;
    }

    inline bool DockToEdge(void* content, Edge edge)
    {
        if (!rp::Bound() || !rp::Has(&rp::host->RadiantAddress) || !rp::host->RadiantAddress || !rp::host->MainWindow) return false;
        auto addr = [](uintptr_t ida) { return rp::host->RadiantAddress(rp::self, ida); };
        auto rootOf = reinterpret_cast<void* (*)(void*)>(addr(0x14002F879));                 // RadiantMainWin dock container
        auto containerOf = reinterpret_cast<void* (*)(void*)>(addr(0x140044F62));            // TADockWidget -> nearest container
        auto remove = reinterpret_cast<void (*)(void*, void*)>(addr(0x14000763A));
        auto add = reinterpret_cast<void (*)(void*, void*, int, int, int, void*, int)>(addr(0x14000860C));
        auto relayout = reinterpret_cast<void (*)(void*)>(addr(0x1400A44D0));
        auto toggleView = reinterpret_cast<void (*)(void*, bool)>(addr(0x14003BCB9));
        if (!rootOf || !containerOf || !remove || !add || !relayout || !toggleView) { rp::Log("dock to edge: this Radiant build is not the one the addresses were read from"); return false; }
        void* main = ql::WidgetForHwnd(rp::host->MainWindow(rp::self));
        void* root = main ? rootOf(main) : nullptr;
        void* dock = DockOf(content);
        if (!root || !dock) { rp::Log("dock to edge: Radiant's dock area or this pane's dock was not found"); return false; }
        void* from = containerOf(dock);
        int orientation = edge == Left || edge == Right ? 1 : 2;
        int side = edge == Bottom || edge == Right ? 1 : 0;
        if (from) remove(from, dock);
        add(root, dock, 0, orientation, side, nullptr, 0);
        if (from && from != root) relayout(from);
        relayout(root);
        toggleView(dock, true);
        rp::Log("docked along the %s of Radiant's dock area", edge == Bottom ? "bottom" : edge == Top ? "top" : edge == Left ? "left" : "right");
        return true;
    }

    // Adds "Dock Along Bottom / Top / Left / Right" entries for a pane to a menu.
    struct MenuTarget { void* content; Edge edge; };
    inline void AddEdgeItems(ModToolsWidget menu, void* content, const char* paneName)
    {
        if (!menu || !rp::Has(&rp::host->AddMenuItem) || !rp::host->AddMenuItem) return;
        static const char* labels[4] = { "Bottom (full width)", "Top (full width)", "Left (full height)", "Right (full height)" };
        rp::host->AddMenuSeparator(rp::self, menu);
        for (int e = 0; e < 4; ++e)
        {
            auto* t = new MenuTarget{ content, Edge(e) };   // lives as long as the menu entry (plugin lifetime)
            std::string text = std::string("Dock ") + paneName + " Along " + labels[e];
            rp::host->AddMenuItem(rp::self, menu, text.c_str(), [](void* u) { auto* m = static_cast<MenuTarget*>(u); DockToEdge(m->content, m->edge); }, t);
        }
    }
}
