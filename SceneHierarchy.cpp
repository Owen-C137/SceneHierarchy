// SceneHierarchy: Radiance's Hierarchy pane as a plain Radiant dock pane (no Radiance needed).
//
// Reads the map's entities through the MTPlugins host (a time-sliced sweep, like Radiance's native side), builds the
// tree with HierarchyModel (Radiance's rules), and shows it in a Qt tree made with the host UI API plus QtLite for
// what that API does not cover (icons, the eye check box column, drag and drop, the context menu, prompts).
// Custom groups are stored where Radiance stores them, so both see the same groups.
#define MODTOOLS_PLUGIN_IMPL
#include "modtools_plugin.hpp"
#include "QtLite.h"
#include "HierarchyModel.h"
#include "RadiantDock.h"
#include "Version.h"
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace
{
    // ---- logging ------------------------------------------------------------------------------------------------
    void LogSink(const char* text) { if (rp::Bound()) rp::host->Log(rp::self, "%s", text); }

    std::string Utf8(const std::wstring& w)
    {
        if (w.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(size_t(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), &s[0], n, nullptr, nullptr);
        return s;
    }
    std::string FileStem(const std::wstring& path)
    {
        size_t slash = path.find_last_of(L"\\/");
        std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
        size_t dot = name.find_last_of(L'.');
        return Utf8(dot == std::wstring::npos ? name : name.substr(0, dot));
    }
    double ElapsedMs(const LARGE_INTEGER& start)
    {
        LARGE_INTEGER now, freq; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&freq);
        return double(now.QuadPart - start.QuadPart) * 1000.0 / double(freq.QuadPart);
    }
    uint64_t Fnv(uint64_t h, const std::string& s) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } h ^= 0xFF; h *= 1099511628211ULL; return h; }

#define HOST(fn) (rp::Has(&rp::host->fn) && rp::host->fn)

    // ---- scene cache ----------------------------------------------------------------------------------------------
    struct SceneEntity { hm::Entity e; uint64_t hash = 0; };
    std::vector<SceneEntity> g_scene;                       // map order
    std::unordered_map<std::string, size_t> g_sceneIndex;  // GUID -> g_scene index
    std::vector<hm::Entity> g_entities;                     // the model's view of g_scene (rebuilt when it changes)
    size_t g_sweepCursor = 0;
    bool g_worldRead = false;
    bool g_scopeChanging = false;
    bool g_sceneChanged = false;
    bool g_unreadPending = false;
    ULONGLONG g_nextSweep = 0;

    // ---- grouping state -------------------------------------------------------------------------------------------
    std::wstring g_mapPath, g_stateFile;
    hm::GroupState g_state;

    // ---- pane and widgets -----------------------------------------------------------------------------------------
    ModToolsPane g_pane = nullptr;
    ModToolsWidget g_root = nullptr, g_tree = nullptr, g_viewport = nullptr, g_search = nullptr, g_showHidden = nullptr, g_footer = nullptr;
    ModToolsWidget g_addButton = nullptr;
    void* g_filter = nullptr;
    bool g_paneWasVisible = false;

    // ---- tree state -----------------------------------------------------------------------------------------------
    hm::Tree g_tree_;                                       // the model shown
    std::vector<ModToolsTreeItem> g_items;                  // node -> item (null when not shown)
    std::vector<int> g_eye;                                 // node -> the check state we set in the eye column
    std::map<std::string, bool, hm::LessCi> g_expanded;    // group key (or "@root") -> open
    std::string g_searchText, g_searchPending;
    ULONGLONG g_searchChangedAt = 0;
    bool g_rebuildPending = false;
    ULONGLONG g_rebuildAt = 0;
    int g_suppress = 0;                                     // >0 while we change the tree ourselves
    struct Ident { std::string guid, group; bool root = false; bool Empty() const { return guid.empty() && group.empty() && !root; } };
    Ident g_current;                                        // the tree's current row
    std::vector<Ident> g_selection;                         // every selected row (multi-selection)
    std::string g_mapName;

    // Pushing the tree's selection to Radiant waits for the mouse button: a click on the eye also selects its row
    // first, and then the selection is put back instead (Radiance: the eye leaves the row's selection alone).
    bool g_pushPending = false, g_eyeDuringPush = false;
    std::vector<Ident> g_pushPrevious;
    ULONGLONG g_buttonUpSince = 0;

    // A drop waits for the next tick (the tree is still inside Qt's drag loop when it arrives).
    bool g_dropPending = false;
    Ident g_dropSource, g_dropTarget;
    std::vector<Ident> g_dropSources;                       // every row being dragged (the selection, when the drag began on it)
    Ident g_dragSource;

    // ---- Radiant selection and hiding -----------------------------------------------------------------------------
    std::vector<std::string> g_radiantSel;                 // entity GUIDs of Radiant's selection, in order
    std::vector<std::string> g_prevSel;                    // the poll before (what a fresh H press hid)
    std::string g_selSignature = "\x01";
    ULONGLONG g_nextSelPoll = 0;
    std::set<std::string, hm::LessCi> g_hidden;            // what we know is hidden in Radiant
    std::string g_hidePath, g_showPath;
    bool g_hidePathsResolved = false;
    bool g_hDown = false;

    // ---- icons ------------------------------------------------------------------------------------------------------
    std::map<std::pair<uint32_t, int>, ql::Icon*> g_icons;
    ql::Icon g_glyphPlus, g_glyphEye, g_glyphExpand, g_glyphCollapse;
    const ql::Icon* IconFor(uint32_t color, int shape)
    {
        auto key = std::make_pair(color, shape);
        auto it = g_icons.find(key);
        if (it != g_icons.end()) return it->second;
        auto* icon = new ql::Icon();
        ql::MakeIcon(*icon, color, shape);
        g_icons[key] = icon;
        return icon;
    }

    // ===============================================================================================================
    // Radiant
    // ===============================================================================================================
    bool Reloading() { return HOST(IsReloading) && rp::host->IsReloading(rp::self); }

    int TriggerMenu(const std::string& path) { return HOST(TriggerMenuAction) ? rp::host->TriggerMenuAction(rp::self, path.c_str()) : 0; }

    std::vector<std::string> ReadSelection()
    {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;
        for (const ModToolsSelectedBrush& s : rp::GetSelection())
        {
            if (!s.entityGuid[0]) continue;
            std::string g(s.entityGuid);
            if (seen.insert(g).second) out.push_back(g);
        }
        return out;
    }

    // Radiance's SelectEntities: Radiant's selection becomes exactly these entities.
    size_t SelectEntities(const std::vector<std::string>& guids)
    {
        if (!HOST(DeselectAll) || !HOST(SelectEntityByGuid)) return 0;
        rp::host->DeselectAll(rp::self);
        if (guids.empty()) return 0;
        if (guids.size() == 1) rp::host->SelectEntityByGuid(rp::self, guids.front().c_str());
        else if (HOST(FindEntitiesByGuid) && HOST(BrushInstOf) && HOST(SelectBrushInsts))
        {
            const auto entities = rp::FindEntitiesByGuid(guids);
            std::vector<ModToolsBrushInst> instances;
            std::vector<std::string> fallback;
            for (size_t i = 0; i < guids.size(); ++i)
            {
                if (!entities[i]) continue;
                size_t before = instances.size();
                rp::ForEachEntityBrush(entities[i], [&](ModToolsBrush b) { if (ModToolsBrushInst inst = rp::host->BrushInstOf(rp::self, b)) instances.push_back(inst); });
                if (instances.size() == before) fallback.push_back(guids[i]);
            }
            for (const auto& g : fallback)
            {
                if (!rp::host->SelectEntityByGuid(rp::self, g.c_str())) continue;
                auto picked = rp::SelectedBrushInsts();
                instances.insert(instances.end(), picked.begin(), picked.end());
                rp::host->DeselectAll(rp::self);
            }
            if (!instances.empty()) rp::host->SelectBrushInsts(rp::self, instances.data(), instances.size());
        }
        else if (HOST(SelectedBrushInsts) && HOST(SelectBrushInsts))
        {
            std::vector<ModToolsBrushInst> instances;
            for (const auto& g : guids)
            {
                if (!rp::host->SelectEntityByGuid(rp::self, g.c_str())) continue;
                auto picked = rp::SelectedBrushInsts();
                instances.insert(instances.end(), picked.begin(), picked.end());
            }
            rp::host->DeselectAll(rp::self);
            if (!instances.empty()) rp::host->SelectBrushInsts(rp::self, instances.data(), instances.size());
        }
        auto now = ReadSelection();
        std::unordered_set<std::string> set(now.begin(), now.end());
        size_t n = 0;
        for (const auto& g : guids) if (set.count(g)) ++n;
        return n;
    }

    // ---- the sweep: entity list -> g_scene ---------------------------------------------------------------------
    bool ReadEntity(ModToolsEntity e, const std::string& guid, SceneEntity& out)
    {
        out.e = hm::Entity{};
        out.e.guid = guid;
        out.e.props = rp::GetKeys(e);
        uint64_t h = 1469598103934665603ULL;
        for (auto& kv : out.e.props)
        {
            if (kv.second.size() > 16384) kv.second.resize(16384);
            h = Fnv(Fnv(h, kv.first), kv.second);
            if (kv.first == "classname") out.e.className = kv.second;
            else if (kv.first == "origin") out.e.origin = kv.second;
        }
        out.e.name = hm::EntityName(out.e.props);
        out.hash = h;
        return true;
    }

    void ResetScene()
    {
        g_scene.clear(); g_sceneIndex.clear(); g_entities.clear();
        g_sweepCursor = 0; g_worldRead = false; g_sceneChanged = true; g_unreadPending = true; g_nextSweep = 0;
        g_hidden.clear();
    }

    // One slice: structural changes (added / removed) always, key reads within the time budget, continuing from
    // where the last slice stopped, so undo, redo and other plugins' edits are all noticed without stalling Radiant.
    void SweepSlice(double budgetMs)
    {
        if (g_scopeChanging || Reloading() || !HOST(ForEachEntity) || !HOST(EntityGuid)) return;
        LARGE_INTEGER start; QueryPerformanceCounter(&start);
        struct Live { ModToolsEntity e; std::string guid; };
        std::vector<Live> live;
        rp::ForEachEntity([&](ModToolsEntity e) { live.push_back({ e, rp::EntityGuid(e) }); });
        ModToolsEntity world = HOST(WorldEntity) ? rp::host->WorldEntity(rp::self) : nullptr;

        std::unordered_set<std::string> liveSet;
        liveSet.reserve(live.size());
        for (const Live& l : live) liveSet.insert(l.guid);
        bool structural = false;
        // removed
        for (const SceneEntity& s : g_scene) if (!liveSet.count(s.e.guid)) { structural = true; break; }
        // new ones first (they must appear), worldspawn by its class without reading every world brush
        std::vector<SceneEntity> fresh;
        bool budget = true, unread = false;
        for (const Live& l : live)
        {
            if (l.guid.empty() || g_sceneIndex.count(l.guid)) continue;
            if (l.e == world)
            {
                SceneEntity w; w.e.guid = l.guid; w.e.className = "worldspawn"; w.e.name = "worldspawn";
                w.e.props = { { "classname", "worldspawn" } };
                fresh.push_back(w);
                continue;
            }
            if (!budget) { unread = true; continue; }
            SceneEntity s;
            if (ReadEntity(l.e, l.guid, s)) fresh.push_back(std::move(s));
            budget = ElapsedMs(start) < budgetMs;
        }
        if (!fresh.empty() || structural)
        {
            // rebuild g_scene in map order: known records kept, new ones added, gone ones dropped
            std::unordered_map<std::string, SceneEntity> pool;
            for (SceneEntity& s : g_scene) pool.emplace(s.e.guid, std::move(s));
            for (SceneEntity& s : fresh) pool[s.e.guid] = std::move(s);
            g_scene.clear(); g_sceneIndex.clear();
            for (const Live& l : live)
            {
                auto it = pool.find(l.guid);
                if (it == pool.end()) continue;
                g_sceneIndex[l.guid] = g_scene.size();
                g_scene.push_back(std::move(it->second));
                pool.erase(it);
            }
            g_sceneChanged = true;
        }
        g_unreadPending = unread;

        // worldspawn's keys once per map (its text holds every world brush), for the search
        if (budget && !g_worldRead && world)
        {
            std::string guid = rp::EntityGuid(world);
            auto it = g_sceneIndex.find(guid);
            if (it != g_sceneIndex.end())
            {
                SceneEntity s;
                if (ReadEntity(world, guid, s)) { s.e.name = hm::EntityName(s.e.props); g_scene[it->second] = std::move(s); g_sceneChanged = true; }
                g_worldRead = true;
            }
            budget = ElapsedMs(start) < budgetMs;
        }

        // re-read known entities round-robin for key changes (renames, models, classnames)
        if (budget && !unread && !live.empty())
        {
            size_t visited = 0;
            while (visited < live.size() && ElapsedMs(start) < budgetMs)
            {
                if (g_sweepCursor >= live.size()) g_sweepCursor = 0;
                const Live& l = live[g_sweepCursor++];
                ++visited;
                if (l.e == world) continue;
                auto it = g_sceneIndex.find(l.guid);
                if (it == g_sceneIndex.end()) continue;
                SceneEntity s;
                if (!ReadEntity(l.e, l.guid, s)) continue;
                if (s.hash != g_scene[it->second].hash) { g_scene[it->second] = std::move(s); g_sceneChanged = true; }
            }
        }
    }

    // ---- hiding: View > Hide Entities > Hide Selected (H) and Show Hidden (Shift+H), as Radiance does ---------------
    std::string PlainLabel(std::string s)
    {
        s.erase(std::remove(s.begin(), s.end(), '&'), s.end());
        size_t tab = s.find('\t'); if (tab != std::string::npos) s.resize(tab);
        while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
        return s;
    }
    std::string MenuText(const std::string& path)
    {
        if (!HOST(MenuEntries)) return {};
        std::string buf(4096, '\0');
        size_t n = rp::host->MenuEntries(rp::self, path.c_str(), &buf[0], buf.size());
        if (n >= buf.size()) { buf.assign(n + 1, '\0'); n = rp::host->MenuEntries(rp::self, path.c_str(), &buf[0], buf.size()); }
        buf.resize(std::min(n, buf.size()));
        return buf;
    }
    void FindMenu(const std::string& path, int depth, const char* label, std::string& out)
    {
        if (!out.empty() || depth > 4) return;
        std::string text = MenuText(path);
        size_t pos = 0;
        while (pos < text.size() && out.empty())
        {
            size_t nl = text.find('\n', pos);
            std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? text.size() : nl + 1;
            if (line.empty() || line == "---") continue;
            size_t t1 = line.find('\t');
            std::string name = line.substr(0, t1);
            bool sub = !line.empty() && line.back() == '1' && line.size() >= 2 && line[line.size() - 2] == '\t';
            std::string child = path.empty() ? PlainLabel(name) : path + "/" + PlainLabel(name);
            if (sub) FindMenu(child, depth + 1, label, out);
            else if (hm::EqualsCi(PlainLabel(name), label)) out = child;
        }
    }
    bool ResolveHidePaths()
    {
        if (g_hidePathsResolved) return !g_hidePath.empty() && !g_showPath.empty();
        g_hidePathsResolved = true;
        FindMenu("", 0, "Hide Selected", g_hidePath);
        FindMenu("", 0, "Show Hidden", g_showPath);
        if (g_hidePath.empty()) g_hidePath = "View/Hide Entities/Hide Selected";
        if (g_showPath.empty()) g_showPath = "View/Show Hidden";
        rp::Log("hide commands: '%s' / '%s'", g_hidePath.c_str(), g_showPath.c_str());
        return true;
    }

    void RefreshEyes();
    void UpdateFooter();

    // SetEntitiesHiddenAsync: hiding selects the entities, runs Hide Selected and puts the selection back; showing runs
    // Show Hidden, then hides again whatever should stay hidden.
    void SetHidden(const std::vector<std::string>& guids, bool hidden)
    {
        if (!ResolveHidePaths()) return;
        std::vector<std::string> selection = ReadSelection();
        std::set<std::string, hm::LessCi> still(g_hidden.begin(), g_hidden.end());
        std::vector<std::string> toHide;
        if (hidden)
        {
            for (const auto& g : guids) if (!still.count(g)) toHide.push_back(g);
            if (toHide.empty()) return;
        }
        else
        {
            for (const auto& g : guids) still.erase(g);
            if (TriggerMenu(g_showPath) != 1) { rp::Log("Show Hidden ('%s') is unavailable", g_showPath.c_str()); return; }
            for (const auto& g : still) if (g_sceneIndex.count(g)) toHide.push_back(g);
            still.clear();
        }
        if (!toHide.empty())
        {
            if (SelectEntities(toHide) == 0) return;
            if (TriggerMenu(g_hidePath) != 1) { rp::Log("Hide Selected ('%s') is unavailable", g_hidePath.c_str()); return; }
            still.insert(toHide.begin(), toHide.end());
        }
        std::vector<std::string> restore;
        for (const auto& g : selection) if (!still.count(g)) restore.push_back(g);
        if (!restore.empty()) SelectEntities(restore);
        else if (!toHide.empty() && HOST(DeselectAll)) rp::host->DeselectAll(rp::self);
        g_hidden = still;
        g_prevSel = g_radiantSel = ReadSelection();
        RefreshEyes();
        UpdateFooter();
    }

    void ShowAllHidden()
    {
        if (!ResolveHidePaths()) return;
        if (TriggerMenu(g_showPath) == 1) { g_hidden.clear(); RefreshEyes(); UpdateFooter(); }
    }

    // ===============================================================================================================
    // Tree
    // ===============================================================================================================
    struct Suppress { Suppress() { ++g_suppress; } ~Suppress() { --g_suppress; } };

    Ident IdentOf(int node)
    {
        Ident id;
        if (node < 0 || node >= int(g_tree_.nodes.size())) return id;
        const hm::Node& n = g_tree_.nodes[node];
        if (n.kind == hm::Kind::Map) id.root = true;
        else if (n.IsGroup()) id.group = n.key;
        else id.guid = n.guid;
        return id;
    }
    int NodeOf(const Ident& id)
    {
        if (id.root) return g_tree_.Root();
        if (!id.group.empty()) return g_tree_.FindGroup(id.group);
        if (!id.guid.empty()) return g_tree_.FindGuid(id.guid);
        return -1;
    }
    int NodeOfItem(ModToolsTreeItem item)
    {
        if (!item) return -1;
        int64_t id = rp::host->ui->TreeItemId(item);
        int node = int(id) - 1;
        return node >= 0 && node < int(g_items.size()) && g_items[node] == item ? node : -1;
    }
    int CurrentNode() { return g_tree ? NodeOfItem(rp::host->ui->TreeCurrent(g_tree)) : -1; }

    // Eye state of every row: an entity is shown unless hidden; a group while anything in it is shown.
    void RefreshEyes()
    {
        if (g_tree_.nodes.empty()) return;
        for (auto it = g_hidden.begin(); it != g_hidden.end();) it = g_sceneIndex.count(*it) ? std::next(it) : g_hidden.erase(it);
        Suppress s;
        struct Walk
        {
            std::pair<int, int> Run(int node)
            {
                const hm::Node& n = g_tree_.nodes[node];
                int shown = 0, hidden = 0;
                if (n.HasEntity() && n.children.empty()) { (g_hidden.count(n.guid) ? hidden : shown) = 1; }
                else for (int c : n.children) { auto r = Run(c); shown += r.first; hidden += r.second; }
                bool active = shown > 0 || hidden == 0;
                int state = !active ? 0 : (shown > 0 && hidden > 0 ? 1 : 2);
                if (g_items[node] && g_eye[node] != state)
                {
                    ql::TreeItemCheck(g_items[node], 2, state);
                    // a hidden row is dimmed (Radiance: inactive rows at 45%), a shown one takes the theme's colour again
                    if (state == 0) ql::TreeItemColor(g_items[node], 0, 0x707070);
                    else if (g_eye[node] == 0) ql::TreeItemClearRole(g_items[node], 0, ql::ForegroundRole);
                    g_eye[node] = state;
                }
                return { shown, hidden };
            }
        } walk;
        walk.Run(0);
        if (g_showHidden) rp::host->ui->SetEnabled(g_showHidden, !g_hidden.empty());
    }

    // The selected rows (several with Ctrl / Shift). They stay selected while the search hides them.
    std::vector<int> SelectedNodes()
    {
        std::vector<int> out;
        for (const Ident& id : g_selection) { int n = NodeOf(id); if (n >= 0) out.push_back(n); }
        return out;
    }
    // Every entity under the given rows, once each (a group gives all of its entities, the map row none).
    std::vector<std::string> GuidsOf(const std::vector<int>& nodes)
    {
        std::vector<std::string> out;
        std::set<std::string, hm::LessCi> seen;
        for (int n : nodes)
            if (g_tree_.nodes[n].kind != hm::Kind::Map)
                for (const auto& g : g_tree_.EntityGuidsUnder(n)) if (seen.insert(g).second) out.push_back(g);
        return out;
    }
    bool IsSelected(int node) { for (int n : SelectedNodes()) if (n == node) return true; return false; }
    int SelectedEntityCount() { return int(GuidsOf(SelectedNodes()).size()); }

    void UpdateFooter()
    {
        if (!g_footer) return;
        std::string text;
        if (g_mapPath.empty()) text = "No map loaded";
        else
        {
            text = std::to_string(g_entities.size()) + " entities   |   " + std::to_string(SelectedEntityCount()) + " selected";
            if (!g_hidden.empty()) text += "   |   " + std::to_string(g_hidden.size()) + " hidden";
            if (g_unreadPending) text += "   |   reading...";
        }
        rp::host->ui->SetLabelText(g_footer, text.c_str());
    }

    void CaptureExpansion()
    {
        for (size_t i = 0; i < g_items.size(); ++i)
        {
            if (!g_items[i]) continue;
            const hm::Node& n = g_tree_.nodes[i];
            if (n.kind == hm::Kind::Map) g_expanded["@root"] = ql::TreeItemExpanded(g_items[i]);
            else if (n.IsGroup()) g_expanded[n.key] = ql::TreeItemExpanded(g_items[i]);
        }
    }

    std::string RowText(const hm::Node& n)
    {
        std::string second = n.IsGroup() ? std::to_string(n.children.size()) : n.Detail();
        return n.name + "\t" + second + "\t";
    }

    void AddItems(int node, ModToolsTreeItem parent)
    {
        const hm::Node& n = g_tree_.nodes[node];
        if (!g_tree_.Shown(node)) return;
        ModToolsTreeItem item = rp::host->ui->TreeAdd(g_tree, parent, RowText(n).c_str());
        if (!item) return;
        ql::TreeItemText(item, 0, n.name);   // names beyond ASCII (the host's text is Latin-1)
        ql::TreeItemText(item, 1, n.Detail());
        g_items[node] = item;
        rp::host->ui->TreeSetItemId(item, int64_t(node) + 1);
        ql::TreeItemIcon(item, 0, *IconFor(n.color, n.shape));
        // flags: selectable 1, drag 4, drop 8, user checkable 16, enabled 32; the map and its settings cannot be dragged
        int flags = 1 | 8 | 16 | 32;
        if (n.kind != hm::Kind::Map && !n.pinned) flags |= 4;
        ql::TreeItemSetFlags(item, flags);
        ql::TreeItemCheck(item, 2, 2);
        g_eye[node] = 2;
        if (n.IsGroup()) ql::TreeItemColor(item, 1, n.color);
        else ql::TreeItemColor(item, 1, 0x8A8A8A);
        if (n.IsGroup() || n.kind == hm::Kind::Map)
            ql::TreeItemToolTip(item, 2, "Hide or show everything in this group in Radiant (H / Shift+H)");
        else
        {
            ql::TreeItemToolTip(item, 2, "Hide or show this in Radiant (H / Shift+H)");
            if (n.pinned) ql::TreeItemToolTip(item, 0, "Map settings: always at the top");
            else ql::TreeItemToolTip(item, 0, n.className + (n.name != n.className ? "  " + n.name : std::string()));
        }
        for (int c : n.children) AddItems(c, item);
        bool open = n.expanded;
        auto it = g_expanded.find(n.kind == hm::Kind::Map ? std::string("@root") : n.key);
        if (n.kind == hm::Kind::Map) open = true;   // the map itself stays open
        else if (n.IsGroup() && it != g_expanded.end()) open = it->second;
        if (n.IsGroup() || n.kind == hm::Kind::Map) rp::host->ui->TreeSetExpanded(item, open ? 1 : 0);
    }

    // Selects exactly these rows (the first becomes the current one), without pushing anything to Radiant.
    void SetSelection(const std::vector<Ident>& ids, bool scroll)
    {
        Suppress s;
        ql::ClearSelection(g_tree);
        std::vector<Ident> kept;
        ModToolsTreeItem first = nullptr;
        for (const Ident& id : ids)
        {
            int node = NodeOf(id);
            if (node < 0) continue;
            kept.push_back(id);
            ModToolsTreeItem item = g_items[node];
            if (!item) continue;   // hidden by the search: still selected, shown again when it matches
            if (!first) { first = item; rp::host->ui->TreeSetCurrent(g_tree, item); }
            ql::TreeItemSelect(item, true);
        }
        if (!first) rp::host->ui->TreeSetCurrent(g_tree, nullptr);
        if (first && scroll) ql::TreeScrollTo(g_tree, first);
        g_selection = kept;
        g_current = kept.empty() ? Ident{} : kept.front();
    }
    void SetCurrent(const Ident& id, bool scroll) { SetSelection(id.Empty() ? std::vector<Ident>{} : std::vector<Ident>{ id }, scroll); }
    // The tree's selection as the user left it.
    void ReadTreeSelection()
    {
        g_selection.clear();
        for (ModToolsTreeItem item : ql::TreeSelectedItems(g_tree)) { int n = NodeOfItem(item); if (n >= 0) g_selection.push_back(IdentOf(n)); }
        int cur = CurrentNode();
        if (cur >= 0) g_current = IdentOf(cur);
    }

    // RebuildHierarchy: the tree from the current scene and groups, keeping what is open, selected and scrolled.
    void Rebuild()
    {
        g_rebuildPending = false;
        if (!g_tree) return;
        if (g_sceneChanged)
        {
            g_entities.clear();
            g_entities.reserve(g_scene.size());
            for (const SceneEntity& s : g_scene) g_entities.push_back(s.e);
            g_sceneChanged = false;
        }
        CaptureExpansion();
        ModToolsWidget bar = ql::VerticalScrollBar(g_tree);
        int scroll = ql::SliderValue(bar);
        std::vector<Ident> keep = g_selection;

        if (g_mapPath.empty() && g_entities.empty()) g_tree_ = hm::Tree{};
        else g_tree_ = hm::Build(g_mapName.empty() ? "Map" : g_mapName, g_entities, g_state);
        if (!g_searchText.empty())
            for (int opened : hm::ApplySearch(g_tree_, g_entities, g_searchText))
                if (g_tree_.nodes[opened].IsGroup()) g_expanded[g_tree_.nodes[opened].key] = true;   // matching groups open up

        Suppress s;
        ql::SetUpdates(g_tree, false);
        rp::host->ui->TreeClear(g_tree);
        g_items.assign(g_tree_.nodes.size(), nullptr);
        g_eye.assign(g_tree_.nodes.size(), -1);
        if (!g_tree_.nodes.empty()) AddItems(0, nullptr);
        SetSelection(keep, false);
        RefreshEyes();
        ql::SetUpdates(g_tree, true);
        ql::SliderSetValue(bar, scroll);
        UpdateFooter();
    }
    void RebuildSoon(ULONGLONG delay = 0) { if (!g_rebuildPending || GetTickCount64() + delay < g_rebuildAt) g_rebuildAt = GetTickCount64() + delay; g_rebuildPending = true; }

    void SaveGroups()
    {
        if (g_stateFile.empty()) return;
        if (!hm::SaveState(g_stateFile, g_state)) rp::Log("could not save custom hierarchy groups to %s", Utf8(g_stateFile).c_str());
    }

    // The tree's selection -> Radiant: every entity of every selected row (a group: all of its entities). Nothing
    // selected deselects in Radiant; only the map row selected leaves Radiant alone.
    void PushSelection(const std::vector<int>& nodes)
    {
        std::vector<std::string> guids = GuidsOf(nodes);
        if (guids.empty())
        {
            if (!nodes.empty() || !HOST(DeselectAll)) return;
            rp::host->DeselectAll(rp::self);
        }
        else SelectEntities(guids);
        g_radiantSel = ReadSelection();
        g_selSignature.clear();
        for (const auto& g : g_radiantSel) g_selSignature += g + "|";
    }
    void PushSelection(int node) { if (node >= 0) PushSelection(std::vector<int>{ node }); }

    // Radiant's selection -> the tree. Selected groups stay selected while Radiant's selection is exactly their
    // entities; otherwise every selected entity's row is selected.
    void ApplyRadiantSelection(bool force)
    {
        std::string signature;
        for (const auto& g : g_radiantSel) signature += g + "|";
        std::vector<int> selected = SelectedNodes();
        bool lost = !g_radiantSel.empty() && selected.empty();
        if (!force && !lost && signature == g_selSignature) return;
        g_selSignature = signature;
        auto mine = GuidsOf(selected);
        std::set<std::string, hm::LessCi> a(mine.begin(), mine.end()), b(g_radiantSel.begin(), g_radiantSel.end());
        if (!g_radiantSel.empty() && a == b) { SetSelection(g_selection, false); UpdateFooter(); return; }   // already what Radiant has
        std::vector<Ident> rows;
        for (const auto& g : g_radiantSel) { int node = g_tree_.FindGuid(g); if (node >= 0) rows.push_back(IdentOf(node)); }
        SetSelection(rows, true);
        UpdateFooter();
    }

    // ---- actions --------------------------------------------------------------------------------------------------
    void* Parent() { return g_root; }

    void CreateGroupPrompt(const std::vector<std::string>& members)
    {
        std::string name;
        std::string label = members.empty()
            ? "This group organizes objects in the Hierarchy and keeps the map's native entity structure unchanged.\n\nGroup name:"
            : "A new group with the " + std::to_string(members.size()) + " selected entit" + (members.size() == 1 ? "y" : "ies") + ".\n\nGroup name:";
        if (!ql::AskText(Parent(), "Create Hierarchy Group", label, "New Group", name)) return;
        if (name.find_first_not_of(" \t") == std::string::npos) return;
        std::string created = members.empty() ? hm::CreateGroup(g_state, name) : hm::GroupEntities(g_state, g_tree_, members, name);
        if (created.empty()) return;
        g_expanded[created] = true;
        SaveGroups();
        Rebuild();
        SetCurrent(Ident{ "", created, false }, true);
        UpdateFooter();
    }

    void RenameGroupPrompt(int node)
    {
        if (node < 0 || !g_tree_.nodes[node].custom) return;
        std::string key = g_tree_.nodes[node].key, name, error;
        if (!ql::AskText(Parent(), "Rename Hierarchy Group", "Group name:", key, name)) return;
        bool wasOpen = g_expanded.count(key) ? g_expanded[key] : true;
        if (!hm::RenameGroup(g_state, key, name, error)) { if (!error.empty()) ql::Warn(Parent(), "Rename Hierarchy Group", error); return; }
        std::string renamed;
        for (const auto& g : g_state.groups) if (hm::EqualsCi(g, name.substr(name.find_first_not_of(" \t")))) renamed = g;
        if (renamed.empty()) renamed = name;
        g_expanded[renamed] = wasOpen;
        SaveGroups();
        if (hm::EqualsCi(g_current.group, key)) g_current.group = renamed;
        for (Ident& id : g_selection) if (hm::EqualsCi(id.group, key)) id.group = renamed;
        Rebuild();
    }

    void RemoveGroup(int node)
    {
        if (node < 0 || !g_tree_.nodes[node].custom) return;
        if (!hm::RemoveGroup(g_state, g_tree_.nodes[node].key)) return;
        SaveGroups();
        Rebuild();
    }

    // Radiance's Delete: select exactly the targets, then Radiant's own Edit/Delete (one undo step).
    void DeleteEntities(const std::vector<std::string>& guids, const std::string& what)
    {
        if (guids.empty()) return;
        if (SelectEntities(guids) != guids.size()) { rp::Log("delete of %s: Radiant selected fewer than %zu entities, nothing was deleted", what.c_str(), guids.size()); return; }
        int r = TriggerMenu("Edit/Delete");
        if (r != 1) rp::Log("delete of %s: Radiant's Edit/Delete %s", what.c_str(), r == 0 ? "was not found" : "is disabled");
        else rp::Log("deleted %zu entit%s (%s) with Edit/Delete", guids.size(), guids.size() == 1 ? "y" : "ies", what.c_str());
        g_nextSweep = 0;
    }
    // Deletes every deletable entity under the rows (worldspawn never), as one Radiant Edit/Delete.
    void DeleteNodes(const std::vector<int>& nodes)
    {
        std::vector<std::string> guids;
        for (const auto& g : GuidsOf(nodes)) { int n = g_tree_.FindGuid(g); if (n >= 0 && g_tree_.nodes[n].CanDelete()) guids.push_back(g); }
        if (guids.empty()) return;
        std::string what = nodes.size() == 1 ? (g_tree_.nodes[nodes[0]].IsGroup() ? "group '" + g_tree_.nodes[nodes[0]].name + "'" : g_tree_.nodes[nodes[0]].name)
                                             : std::to_string(nodes.size()) + " selected rows";
        DeleteEntities(guids, what);
    }
    void DeleteNode(int node) { if (node >= 0) DeleteNodes({ node }); }
    // The rows an action on `node` applies to: the whole selection when the node is part of it, else just the node.
    std::vector<int> ActionNodes(int node) { return IsSelected(node) ? SelectedNodes() : std::vector<int>{ node }; }

    // The eye: rows that are shown (a group: anything in it shown) get everything in them hidden, hidden ones shown.
    void ToggleVisibility(const std::vector<int>& nodes)
    {
        auto targets = GuidsOf(nodes);
        if (targets.empty()) return;
        int shown = 0;
        for (const auto& g : targets) if (!g_hidden.count(g)) ++shown;
        SetHidden(targets, shown > 0);
    }
    void ToggleVisibility(int node) { if (node >= 0) ToggleVisibility(std::vector<int>{ node }); }

    void SetAllExpanded(bool open)
    {
        Suppress s;
        for (size_t i = 0; i < g_items.size(); ++i)
        {
            if (!g_items[i]) continue;
            const hm::Node& n = g_tree_.nodes[i];
            if (n.kind == hm::Kind::Map) rp::host->ui->TreeSetExpanded(g_items[i], 1);
            else if (n.IsGroup()) { rp::host->ui->TreeSetExpanded(g_items[i], open ? 1 : 0); g_expanded[n.key] = open; }
        }
    }

    void ShowContextMenu(int node)
    {
        if (node < 0) return;
        const hm::Node& n = g_tree_.nodes[node];
        std::vector<int> targets = ActionNodes(node);
        bool many = targets.size() > 1;
        int shownCount = 0;
        for (const auto& g : GuidsOf(targets)) if (!g_hidden.count(g)) ++shownCount;
        bool active = shownCount > 0;
        ql::Menu m(g_tree);
        if (!m.Ok()) return;
        enum Act { SelectGroup, Hide, Show, DeleteGroup, DeleteObject, NewGroup, GroupSelection, Rename, Remove, Count };
        int ids[Count]; for (int& i : ids) i = -2;
        if (n.IsGroup() && !many) ids[SelectGroup] = m.Add("Select All in Group");
        std::string suffix = many ? " (" + std::to_string(targets.size()) + " rows)" : std::string();
        if (active) ids[Hide] = m.Add("Hide" + suffix + "\tH"); else ids[Show] = m.Add("Show" + suffix);
        if (many) ids[DeleteObject] = m.Add("Delete Selected" + suffix + "\tDel");
        else
        {
            if (g_tree_.CanDeleteGroup(node)) ids[DeleteGroup] = m.Add("Delete All in Group");
            if (n.CanDelete()) ids[DeleteObject] = m.Add("Delete Object\tDel");
        }
        m.Separator();
        ids[NewGroup] = m.Add("New Group...");
        ids[GroupSelection] = m.Add("Group Selection...", !g_radiantSel.empty());
        if (n.custom)
        {
            ids[Rename] = m.Add("Rename Group...\tF2");
            ids[Remove] = m.Add("Remove Custom Group");
        }
        int chosen = m.Exec();
        if (chosen < 0) return;
        Ident id = IdentOf(node);
        if (chosen == ids[SelectGroup]) { SetCurrent(id, false); PushSelection(node); UpdateFooter(); }
        else if (chosen == ids[Hide] || chosen == ids[Show]) ToggleVisibility(targets);
        else if (chosen == ids[DeleteGroup] || chosen == ids[DeleteObject]) DeleteNodes(targets);
        else if (chosen == ids[NewGroup]) CreateGroupPrompt({});
        else if (chosen == ids[GroupSelection]) CreateGroupPrompt(g_radiantSel);
        else if (chosen == ids[Rename]) RenameGroupPrompt(node);
        else if (chosen == ids[Remove]) RemoveGroup(node);
    }

    // One row dropped: Radiance's rules (MoveToGroup, including a new group of two entities of the same group).
    // Several selected rows dropped together: each goes into the target's group (or back home on the map).
    void ApplyDrop()
    {
        g_dropPending = false;
        int target = NodeOf(g_dropTarget);
        std::vector<Ident> sources = g_dropSources.empty() ? std::vector<Ident>{ g_dropSource } : g_dropSources;
        g_dropSources.clear();
        if (target < 0 || sources.empty()) return;
        bool changed = false;
        if (sources.size() == 1)
        {
            int source = NodeOf(sources[0]);
            changed = source >= 0 && hm::MoveToGroup(g_state, g_tree_, g_entities, source, target);
        }
        else
            for (const Ident& id : sources)
            {
                int source = NodeOf(id);
                if (source < 0 || source == target || !hm::CanMove(g_tree_, source, target)) continue;
                changed |= g_tree_.nodes[source].IsGroup() ? hm::MoveGroup(g_state, g_tree_, source, target) : hm::MoveEntity(g_state, g_tree_, g_entities, source, target);
            }
        if (!changed) return;
        int destination = hm::DestinationGroup(g_tree_, target);
        if (destination >= 0) g_expanded[g_tree_.nodes[destination].key] = true;
        SaveGroups();
        Rebuild();
        SetSelection(sources, true);
    }

    // ---- Qt events on the tree and its viewport --------------------------------------------------------------------
    bool TreeEvent(void*, void* watched, int type, void* ev)
    {
        using namespace ql;
        if (type == EvShortcutOverride && watched == g_tree)
        {
            int key = KeyCode(ev), mods = KeyMods(ev) & (CtrlMod | AltMod | ShiftMod);
            if ((key == Key_Delete || key == Key_Backspace || key == Key_F2) && mods == 0) AcceptEvent(ev);   // ours, not Radiant's shortcut
            return false;
        }
        if (type == EvKeyPress && watched == g_tree)
        {
            int key = KeyCode(ev), mods = KeyMods(ev) & (CtrlMod | AltMod | ShiftMod);
            if ((key == Key_Delete || key == Key_Backspace) && mods == 0) { DeleteNodes(SelectedNodes()); return true; }
            if (key == Key_F2 && mods == 0) { RenameGroupPrompt(CurrentNode()); return true; }
            return false;
        }
        if (watched != g_viewport) return false;
        if (type == EvMouseButtonPress)
        {
            g_dragSource = IdentOf(NodeOfItem(TreeItemUnderCursor(g_tree)));
            return false;
        }
        if (type == EvContextMenu)
        {
            int node = NodeOfItem(TreeItemUnderCursor(g_tree));
            if (node < 0) return true;
            if (!IsSelected(node))
            {
                SetCurrent(IdentOf(node), false);   // a right-click outside the selection selects that row (and its entities in Radiant)
                PushSelection(node);
                UpdateFooter();
            }
            ShowContextMenu(node);
            return true;
        }
        if (type == EvDragMove)
        {
            // anything into anything; an invalid target shows the no-drop cursor
            int source = NodeOf(g_dragSource), target = NodeOfItem(TreeItemUnderCursor(g_tree));
            return !(source >= 0 && target >= 0 && hm::CanMove(g_tree_, source, target));
        }
        if (type == EvDrop)
        {
            // what Qt's own drop handling would have done for the view: stop scrolling at the edges
            if (A().QAbstractItemView_stopAutoScroll) A().QAbstractItemView_stopAutoScroll(g_tree);
            int target = NodeOfItem(TreeItemUnderCursor(g_tree));
            int source = NodeOf(g_dragSource);
            if (target >= 0 && source >= 0)
            {
                g_dropSource = g_dragSource;
                g_dropSources = IsSelected(source) ? g_selection : std::vector<Ident>{ g_dragSource };
                g_dropTarget = IdentOf(target);
                g_dropPending = true;
            }
            return true;   // Qt never moves rows itself: the model decides and the tree is rebuilt
        }
        return false;
    }

    // ---- host widget callbacks -------------------------------------------------------------------------------------
    // The user changed the selection (click, Ctrl / Shift click, keys): pushed to Radiant once the mouse is up.
    void SelectionChanged()
    {
        if (g_suppress > 0) return;
        if (!g_pushPending) g_pushPrevious = g_selection;
        ReadTreeSelection();
        g_pushPending = true;
        g_eyeDuringPush = false;
        UpdateFooter();
    }
    void OnSelectionSignal(void*, void**) { SelectionChanged(); }

    void OnTreeChanged(void*, int what)
    {
        if (g_suppress > 0) return;
        if (what == 0) SelectionChanged();
        else if (what == 1)
        {
            // which eye was clicked: the one whose check state no longer matches ours
            for (size_t i = 0; i < g_items.size(); ++i)
            {
                if (!g_items[i]) continue;
                int state = ql::TreeItemCheckState(g_items[i], 2);
                if (state >= 0 && state != g_eye[i])
                {
                    if (g_pushPending) g_eyeDuringPush = true;
                    { Suppress s; ql::TreeItemCheck(g_items[i], 2, g_eye[i]); }
                    ToggleVisibility(int(i));
                    break;
                }
            }
        }
    }

    void OnAddClicked(void*)
    {
        ql::Menu m(g_addButton);
        if (!m.Ok()) { CreateGroupPrompt({}); return; }
        int empty = m.Add("New Group...");
        int fromSel = m.Add("Group Selection...", !g_radiantSel.empty());
        int chosen = m.Exec();
        if (chosen == empty) CreateGroupPrompt({});
        else if (chosen == fromSel) CreateGroupPrompt(g_radiantSel);
    }
    void OnShowHiddenClicked(void*) { ShowAllHidden(); }
    void OnExpandClicked(void*) { SetAllExpanded(true); }
    void OnCollapseClicked(void*) { SetAllExpanded(false); }

    // ===============================================================================================================
    // Pane
    // ===============================================================================================================
    ModToolsWidget ToolButton(const char* fallbackText, ql::Icon& icon, ql::Glyph glyph, const char* tip, ModToolsCallback fn, ModToolsWidget box)
    {
        const ModToolsUiApi* ui = rp::host->ui;
        ModToolsWidget b = ui->PushButton("", g_root);
        if (ql::MakeGlyph(icon, glyph) && ql::SetButtonIcon(b, icon)) { if (ql::A().QWidget_setFixedWidth) ql::A().QWidget_setFixedWidth(b, 26); }
        else ui->SetButtonText(b, fallbackText);
        ui->SetToolTip(b, tip);
        rp::host->OnClicked(rp::self, b, fn, nullptr);
        ui->BoxAddWidget(box, b, 0);
        return b;
    }

    bool BuildPane()
    {
        const ModToolsUiApi* ui = rp::host->ui;
        if (!ui || !rp::HasUi(&ui->TreeSetDragDrop) || !HOST(OnChanged)) { rp::Log("this MTPlugins host lacks the tree widget API; update MTPlugins"); return false; }
        g_root = ui->Widget(nullptr);
        ModToolsWidget v = ui->VBox(g_root);
        ui->LayoutMargins(v, 4, 4, 4, 2);
        ui->LayoutSpacing(v, 3);

        // toolbar: create group, search, show everything hidden, expand all, collapse all
        ModToolsWidget bar = ui->HBox();
        ui->LayoutSpacing(bar, 3);
        g_addButton = ToolButton("+", g_glyphPlus, ql::Glyph::Plus, "Create a custom hierarchy group", OnAddClicked, bar);
        g_search = ui->LineEdit("", g_root);
        ui->SetPlaceholder(g_search, "Search name, class or key value...");
        ui->BoxAddWidget(bar, g_search, 1);
        g_showHidden = ToolButton("Unhide", g_glyphEye, ql::Glyph::EyeOff, "Show everything hidden (Radiant's Show Hidden, Shift+H)", OnShowHiddenClicked, bar);
        ui->SetEnabled(g_showHidden, 0);
        ToolButton("Expand", g_glyphExpand, ql::Glyph::ExpandAll, "Expand all groups", OnExpandClicked, bar);
        ToolButton("Collapse", g_glyphCollapse, ql::Glyph::CollapseAll, "Collapse all groups", OnCollapseClicked, bar);
        ui->BoxAddLayout(v, bar, 0);

        // the tree: name (with icon) | class or count | eye
        g_tree = ui->TreeWidget(3, g_root);
        ui->TreeSetDragDrop(g_tree, 1);
        // multi-selection: click, Ctrl+click to add / remove, Shift+click for a range
        if (ql::A().QAbstractItemView_setSelectionMode) ql::A().QAbstractItemView_setSelectionMode(g_tree, 3 /* ExtendedSelection */);
        ql::Connect(g_tree, ql::A().sig_QTreeWidget_itemSelectionChanged, ql::A().meta_QTreeWidget, OnSelectionSignal, nullptr);
        if (ql::A().QAbstractItemView_setDragEnabled) ql::A().QAbstractItemView_setDragEnabled(g_tree, true);
        if (ql::A().QTreeView_header)
            if (void* header = ql::A().QTreeView_header(g_tree))
            {
                if (ql::A().QHeaderView_setStretchLastSection) ql::A().QHeaderView_setStretchLastSection(header, false);
                if (ql::A().QHeaderView_setSectionResizeMode)
                {
                    ql::A().QHeaderView_setSectionResizeMode(header, 0, 1 /* Stretch */);
                    ql::A().QHeaderView_setSectionResizeMode(header, 1, 3 /* ResizeToContents */);
                    ql::A().QHeaderView_setSectionResizeMode(header, 2, 3);
                }
            }
        if (ql::A().QTreeView_setIndentation) ql::A().QTreeView_setIndentation(g_tree, 14);
        if (ql::A().QAbstractItemView_setTextElideMode) ql::A().QAbstractItemView_setTextElideMode(g_tree, 1 /* ElideRight */);
        ui->BoxAddWidget(v, g_tree, 1);
        rp::host->OnChanged(rp::self, g_tree, OnTreeChanged, nullptr);

        g_viewport = ql::Viewport(g_tree);
        g_filter = ql::MakeFilter(TreeEvent, nullptr);
        if (g_filter) { ql::Install(g_tree, g_filter); if (g_viewport) ql::Install(g_viewport, g_filter); }
        else rp::Log("Qt event filter unavailable: no drag and drop, context menu or Delete key in the tree");

        g_footer = ui->Label("No map loaded", g_root);
        ql::IgnoreWidth(g_footer);
        ui->BoxAddWidget(v, g_footer, 0);

        g_pane = rp::host->CreatePane(rp::self, "Hierarchy", g_root, 320, 620);
        if (!g_pane) return false;
        if (!rp::ConfigGetBool("pane", "placed", false) && HOST(TabPaneWith))
        {
            int r = rp::host->TabPaneWith(rp::self, g_pane, "Entity Info");
            rp::ConfigSet("pane", "placed", "1");
            rp::Log("first placement beside Entity Info: %d", r);
        }
        if (ModToolsWidget menu = rp::host->AddMenu(rp::self, "Hierarchy"))
        {
            rp::host->AddMenuAction(rp::self, menu, rp::host->PaneToggleAction(rp::self, g_pane));
            rdock::AddEdgeItems(menu, g_root, "Hierarchy");
        }
        rp::host->ShowPane(rp::self, g_pane, 1);
        return true;
    }

    // ===============================================================================================================
    // Map and scope
    // ===============================================================================================================
    void LoadMap()
    {
        std::wstring path = HOST(CurrentMapPath) ? rp::CurrentMapPath() : std::wstring();
        if (path != g_mapPath)
        {
            g_mapPath = path;
            g_mapName = FileStem(path);
            g_stateFile = path.empty() ? std::wstring() : hm::StateFile(path);
            g_state = g_stateFile.empty() ? hm::GroupState{} : hm::LoadState(g_stateFile);
            g_expanded.clear();
            rp::Log("map %s: %zu custom groups (%s)", Utf8(path).c_str(), g_state.groups.size(), Utf8(g_stateFile).c_str());
        }
        ResetScene();
        g_current = Ident{};
        g_selection.clear();
        g_selSignature = "\x01";
        RebuildSoon();
    }

    void OnMapLoading(const wchar_t*) { g_scopeChanging = true; }
    void OnMapLoaded(const wchar_t*) { g_scopeChanging = false; g_mapPath.clear(); LoadMap(); }
    void OnBeforeScopeChange() { g_scopeChanging = true; }
    void OnScopeChanged(const wchar_t*, int) { g_scopeChanging = false; LoadMap(); }

    // ===============================================================================================================
    // Tick
    // ===============================================================================================================
    void PollHideKeys()
    {
        // H / Shift+H pressed in Radiant (not in a text field) hide the selection / show everything, as in Radiance.
        DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        bool down = pid == GetCurrentProcessId() && (GetAsyncKeyState('H') & 0x8000) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000) && !(GetAsyncKeyState(VK_MENU) & 0x8000);
        if (down && !g_hDown && !ql::TextInputFocused())
        {
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) g_hidden.clear();
            else for (const auto& g : g_prevSel) g_hidden.insert(g);
            RefreshEyes();
            UpdateFooter();
        }
        g_hDown = down;
    }

    void OnTick()
    {
        if (!g_pane || g_scopeChanging || Reloading()) return;
        ULONGLONG now = GetTickCount64();
        PollHideKeys();
        bool visible = !HOST(PaneVisible) || rp::host->PaneVisible(rp::self, g_pane);

        if (now >= g_nextSelPoll)
        {
            g_nextSelPoll = now + 100;
            g_prevSel = g_radiantSel;
            g_radiantSel = ReadSelection();
            if (visible && !g_pushPending) ApplyRadiantSelection(false);
        }
        if (!visible) { g_paneWasVisible = false; return; }
        if (!g_paneWasVisible) { g_paneWasVisible = true; g_nextSweep = 0; }

        // the tree's selection goes to Radiant once the mouse button is up (see g_pushPending)
        bool button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (button) g_buttonUpSince = 0; else if (!g_buttonUpSince) g_buttonUpSince = now;
        if (g_pushPending && g_buttonUpSince && now - g_buttonUpSince >= 30)
        {
            g_pushPending = false;
            if (g_eyeDuringPush) SetSelection(g_pushPrevious, false);   // the eye leaves the selection alone
            else PushSelection(SelectedNodes());
            UpdateFooter();
        }
        if (g_dropPending) ApplyDrop();

        // search as you type
        if (g_search)
        {
            std::string text = rp::EditText(g_search);
            if (text != g_searchPending) { g_searchPending = text; g_searchChangedAt = now; }
            if (g_searchPending != g_searchText && now - g_searchChangedAt >= 150) { g_searchText = g_searchPending; RebuildSoon(); }
        }

        if (now >= g_nextSweep)
        {
            bool filling = g_unreadPending;
            SweepSlice(filling ? 12.0 : 4.0);
            g_nextSweep = now + (g_unreadPending ? 30 : 150);
            if (g_sceneChanged) RebuildSoon(g_unreadPending ? 400 : 100);
        }
        if (g_rebuildPending && now >= g_rebuildAt && !button && !g_pushPending) { Rebuild(); ApplyRadiantSelection(true); }
    }

    // ===============================================================================================================
    // Lifecycle
    // ===============================================================================================================
    int OnLoad()
    {
        if (!rp::InRadiant()) return 0;
        ql::LogSink() = LogSink;
        if (!ql::Bind()) rp::Log("Qt binding incomplete: the tree works without icons, drag and drop or menus");
        if (!BuildPane()) return 0;
        LoadMap();
        rp::Log("SceneHierarchy %s loaded", SCENEHIERARCHY_VERSION);
        return 1;
    }

    void OnUnload()
    {
        ql::Shutdown();     // filters, slots and Qt objects that point into this DLL
        for (auto& kv : g_icons) { kv.second->Reset(); delete kv.second; }
        g_icons.clear();
        for (ql::Icon* i : { &g_glyphPlus, &g_glyphEye, &g_glyphExpand, &g_glyphCollapse }) i->Reset();
        g_pane = nullptr; g_tree = nullptr; g_viewport = nullptr; g_filter = nullptr;
    }

    void OnSettings() { if (g_pane) rp::host->ShowPane(rp::self, g_pane, 1); }

    const char* OnCommand(const char* cmd)
    {
        static std::string reply;
        if (!cmd) return nullptr;
        std::string c = cmd;
        if (c == "status")
        {
            reply = "entities=" + std::to_string(g_entities.size()) + " nodes=" + std::to_string(g_tree_.nodes.size()) + " groups=" + std::to_string(g_state.groups.size())
                + " hidden=" + std::to_string(g_hidden.size()) + " selected=" + std::to_string(g_radiantSel.size()) + " state=" + Utf8(g_stateFile);
            return reply.c_str();
        }
        if (c == "rebuild") { g_nextSweep = 0; RebuildSoon(); return "ok"; }
        // automation: "move <from> <to>" (a drop), "group <name>" (the Radiant selection), "rename <old>|<new>", "remove <name>";
        // a row is "{guid}", "group:<key>" or "root"
        auto ident = [](const std::string& s) { Ident id; if (s == "root") id.root = true; else if (s.rfind("group:", 0) == 0) id.group = s.substr(6); else id.guid = s; return id; };
        if (c.rfind("move ", 0) == 0)
        {
            std::string rest = c.substr(5);
            size_t sp = rest.find(rest[0] == '{' ? "} " : " ");
            if (sp == std::string::npos) return "usage: move <from> <to>";
            if (rest[0] == '{') ++sp;
            g_dropSource = ident(rest.substr(0, sp)); g_dropTarget = ident(rest.substr(sp + 1));
            size_t before = g_state.assignments.size() + g_state.parents.size() + g_state.groups.size();
            ApplyDrop();
            reply = "moved, groups=" + std::to_string(g_state.groups.size()) + " assignments=" + std::to_string(g_state.assignments.size())
                + " parents=" + std::to_string(g_state.parents.size()) + (before == g_state.assignments.size() + g_state.parents.size() + g_state.groups.size() ? " (no count change)" : "");
            return reply.c_str();
        }
        if (c.rfind("group ", 0) == 0)
        {
            std::string made = hm::GroupEntities(g_state, g_tree_, g_radiantSel, c.substr(6));
            if (made.empty()) return "nothing selected";
            SaveGroups(); Rebuild();
            reply = "created " + made; return reply.c_str();
        }
        if (c.rfind("rename ", 0) == 0)
        {
            std::string rest = c.substr(7), error;
            size_t bar = rest.find('|');
            if (bar == std::string::npos) return "usage: rename <old>|<new>";
            if (!hm::RenameGroup(g_state, rest.substr(0, bar), rest.substr(bar + 1), error)) { reply = "refused: " + error; return reply.c_str(); }
            SaveGroups(); Rebuild(); return "ok";
        }
        if (c.rfind("remove ", 0) == 0)
        {
            if (!hm::RemoveGroup(g_state, c.substr(7))) return "no such custom group";
            SaveGroups(); Rebuild(); return "ok";
        }
        if (c.rfind("parent ", 0) == 0)
        {
            Ident id = ident(c.substr(7));
            int node = NodeOf(id);
            if (node < 0) return "unknown";
            int p = g_tree_.nodes[node].parent;
            reply = p < 0 ? "-" : g_tree_.nodes[p].kind == hm::Kind::Map ? "root" : g_tree_.nodes[p].IsGroup() ? "group:" + g_tree_.nodes[p].key : g_tree_.nodes[p].guid;
            return reply.c_str();
        }
        return nullptr;
    }
}

extern "C" MODTOOLS_PLUGIN_EXPORT const ModToolsPluginApi* ModToolsPluginEntry(ModToolsPlugin* self, const ModToolsHostApi* host)
{
    rp::Bind(self, host);
    static ModToolsPluginApi a{};
    a.size = sizeof a;
    a.apiVersion = MODTOOLS_PLUGIN_API_VERSION;
    a.name = "SceneHierarchy";
    a.displayName = "Hierarchy";
    a.version = SCENEHIERARCHY_VERSION;
    a.author = "Owen";
    a.description = "Radiance's Hierarchy pane in Radiant: entities by category, custom groups with drag and drop, search, hide / show and selection sync.";
    a.apps = MODTOOLS_APP_RADIANT;
    a.OnLoad = OnLoad;
    a.OnUnload = OnUnload;
    a.OnTick = OnTick;
    a.OnMapLoaded = OnMapLoaded;
    a.OnBeforeScopeChange = OnBeforeScopeChange;
    a.OnScopeChanged = OnScopeChanged;
    a.OnCommand = OnCommand;
    a.OnSettings = OnSettings;
    a.OnMapLoading = OnMapLoading;
    return &a;
}
