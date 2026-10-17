#include "HierarchyModel.h"
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <set>

#pragma comment(lib, "bcrypt.lib")

namespace hm
{
    // ---- text helpers --------------------------------------------------------------------------------------------
    namespace
    {
        inline unsigned char Up(unsigned char c) { return c >= 'a' && c <= 'z' ? static_cast<unsigned char>(c - 32) : c; }
        bool StartsWith(const std::string& s, const char* p) { size_t n = strlen(p); return s.size() >= n && s.compare(0, n, p) == 0; }
        bool EndsWith(const std::string& s, const char* p) { size_t n = strlen(p); return s.size() >= n && s.compare(s.size() - n, n, p) == 0; }
        bool Contains(const std::string& s, const char* p) { return s.find(p) != std::string::npos; }
        std::string Trim(const std::string& s)
        {
            size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        }
        bool IsBlank(const std::string& s) { return Trim(s).empty(); }
    }

    int CompareCi(const std::string& a, const std::string& b)
    {
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i)
        {
            unsigned char x = Up(static_cast<unsigned char>(a[i])), y = Up(static_cast<unsigned char>(b[i]));
            if (x != y) return x < y ? -1 : 1;
        }
        return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
    }
    bool EqualsCi(const std::string& a, const std::string& b) { return a.size() == b.size() && CompareCi(a, b) == 0; }
    bool ContainsCi(const std::string& text, const std::string& term)
    {
        if (term.empty()) return true;
        if (term.size() > text.size()) return false;
        for (size_t i = 0; i + term.size() <= text.size(); ++i)
        {
            size_t j = 0;
            while (j < term.size() && Up(static_cast<unsigned char>(text[i + j])) == Up(static_cast<unsigned char>(term[j]))) ++j;
            if (j == term.size()) return true;
        }
        return false;
    }
    std::string Lower(const std::string& s)
    {
        std::string r = s;
        for (char& c : r) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return r;
    }

    std::string FindKey(const KeyValues& keys, const char* key)
    {
        for (const auto& kv : keys) if (EqualsCi(kv.first, key)) return kv.second;
        return std::string();
    }

    // targetname first; then the key that says what this entity is (from Radiant's entity definitions): an fx's
    // effect, a light's PRIMARY_TYPE, a sound's alias, a grading box's LVI, a fog volume's settings, a rope's model, a
    // decal's material, a use trigger's hint; then the model; then script_noteworthy; else the class.
    std::string EntityName(const KeyValues& keys)
    {
        std::string cls = FindKey(keys, "classname");
        auto key = [&](const char* k) { std::string v = FindKey(keys, k); return IsBlank(v) ? std::string() : v; };
        std::string v = key("targetname");
        if (!v.empty()) return v;
        std::string c = Lower(cls);
        if (c == "fx") v = key("fxdef");
        else if (c == "light" || c == "build_light") { v = key("PRIMARY_TYPE"); if (v.empty()) v = "PRIMARY_OMNI"; }   // Radiant's default type
        else if (StartsWith(c, "sound_")) v = key("script_sound");
        else if (StartsWith(c, "pbg_")) { v = key("color_lvi"); if (v.empty()) v = key("bloom_lvi"); }
        else if (c == "vcolor_box" || c == "vbloom_box" || c == "vcolor_omni" || c == "vcolor_spot") v = key("lvi");
        else if (c == "vlut_box" || c == "lut_volume") { v = key("index"); if (!v.empty()) v = "LUT index " + v; }
        else if (c == "volume_worldfog" || c == "volume_litfog") v = key("fsi");
        else if (c == "rope") { v = key("ropemodel"); if (v.empty()) v = key("material"); }
        else if (c == "misc_volume_decal") v = key("modeloverridematerial");
        else if (c == "trigger_use" || c == "trigger_use_touch") v = key("hintstring");
        if (v.empty()) v = key("model");
        if (v.empty()) v = key("script_noteworthy");
        if (!v.empty()) return v;
        return cls.empty() ? "Entity" : cls;
    }

    // ---- categories ----------------------------------------------------------------------------------------------
    // Built from Radiant's own entity definitions (every class it defines, plus the actor_ / vehicle_ classes it
    // generates from asset types). Keys shared with Radiance's categories keep their names, so groups and
    // assignments stored by Radiance still find them.
    const Category kCategories[] = {
        { "Prefabs", 0x8DCB3F, 1 },
        { "Static Models", 0x6FA8F0, 1 },
        { "Script Models", 0x4FC1FF, 1 },
        { "Brush Entities", 0xA8B4C0, 1 },
        { "Lights", 0xF0D25A, 1 },
        { "Reflection Probes", 0x9CDCFE, 1 },
        { "Color Grading", 0xE08AE0, 1 },
        { "Lighting & Fog Volumes", 0xD7BA7D, 1 },
        { "Visibility & Performance", 0x7FC8A9, 1 },
        { "Zones", 0x5FD3A0, 1 },
        { "Player Spawns", 0xE07A9A, 1 },
        { "AI Spawners", 0xE05A5A, 1 },
        { "Vehicles", 0xC0A060, 1 },
        { "AI & Navigation", 0x9AB8E0, 1 },
        { "Triggers", 0xE0AA3C, 1 },
        { "Effects", 0xF09A5A, 1 },
        { "Audio", 0x4EC9B0, 1 },
        { "Water & Physics", 0x569CD6, 1 },
        { "Script", 0xC58AE0, 1 },
        { "Decals", 0xB5CEA8, 1 },
        { "Editor & Tools", 0x9A9A9A, 1 },
        { "Volumes", 0x6FB8A0, 1 },
        { "Other", 0x8A8A8A, 1 },
    };
    const size_t kCategoryCount = sizeof kCategories / sizeof kCategories[0];
    const char* const kPinnedClasses[4] = { "worldspawn", "volume_sun", "umbra_volume", "volume_fpstool" };
    const char* const kPinnedGroup = "@map";

    const Category& CategoryOf(const std::string& key)
    {
        for (size_t i = 0; i < kCategoryCount; ++i) if (EqualsCi(key, kCategories[i].key)) return kCategories[i];
        return kCategories[kCategoryCount - 1];
    }
    bool IsBuiltInGroup(const std::string& key)
    {
        for (size_t i = 0; i < kCategoryCount; ++i) if (EqualsCi(key, kCategories[i].key)) return true;
        return false;
    }
    uint32_t PinnedColor(const std::string& className)
    {
        std::string c = Lower(className);
        if (c == "worldspawn") return 0x78AEF0;
        if (c == "volume_sun") return 0xF0C040;
        if (c == "umbra_volume") return 0x7FC8A9;
        return 0x9AB8E0;
    }
    static int PinnedRank(const std::string& className)
    {
        std::string c = Lower(className);
        for (int i = 0; i < 4; ++i) if (c == kPinnedClasses[i]) return i;
        return -1;
    }

    std::string Classify(const Entity& e)
    {
        std::string c = Lower(e.className);
        std::string model = Lower(FindKey(e.props, "model"));
        std::string noteworthy = Lower(FindKey(e.props, "script_noteworthy"));
        std::string targetname = Lower(FindKey(e.props, "targetname"));
        auto in = [&](std::initializer_list<const char*> l) { for (const char* x : l) if (c == x) return true; return false; };
        if (PinnedRank(c) >= 0) return kPinnedGroup;
        if (c == "misc_prefab" || EndsWith(model, ".map")) return "Prefabs";

        // rendering and lighting
        if (c == "reflection_probe" || c == "build_probe" || c == "volume_reflection") return "Reflection Probes";
        if (StartsWith(c, "pbg_") || StartsWith(c, "vcolor_") || StartsWith(c, "vbloom_") || StartsWith(c, "vlut_") || c == "lut_volume") return "Color Grading";
        if (in({ "light", "build_light", "info_corona", "lensflare", "klf" })) return "Lights";
        if (StartsWith(c, "sun_shadow") || c == "lightgrid_sky" || in({ "volume_exposure", "volume_worldfog", "volume_worldfogmodifier", "volume_litfog",
            "volume_attenuation", "volume_lightclip", "volume_outdoor", "volume_sun_extension", "volume_vista", "volume_weathergrime" }))
            return "Lighting & Fog Volumes";
        if (StartsWith(c, "umbra_") || in({ "occlusion_override", "named_occluder", "func_cullgroup", "volume_performance", "lod_info" })) return "Visibility & Performance";

        // gameplay
        if (c == "info_volume" && (noteworthy == "player_volume" || StartsWith(targetname, "zone_"))) return "Zones";
        if (StartsWith(c, "actor_") || StartsWith(c, "zm_spawner_") || c == "spawn_manager"
            || (c == "script_struct" && (noteworthy == "spawn_location" || noteworthy == "riser_location" || noteworthy == "faller_location"
                                         || noteworthy == "dog_location" || noteworthy == "wasp_location" || noteworthy == "raps_location"
                                         || EndsWith(noteworthy, "_spawn_location"))))
            return "AI Spawners";
        if (StartsWith(c, "info_player") || Contains(c, "intermission") || Contains(c, "influencer") || StartsWith(c, "cp_coop")
            || ((StartsWith(c, "mp_") || StartsWith(c, "cp_") || StartsWith(c, "zm_")) && Contains(c, "spawn"))) return "Player Spawns";
        if (StartsWith(c, "vehicle")) return "Vehicles";
        if (StartsWith(c, "node_") || StartsWith(c, "info_vehicle_node") || StartsWith(c, "nav_volume") || c == "trigger_navmesh"
            || c == "heli_height_lock" || c == "info_grenade_hint") return "AI & Navigation";
        if (StartsWith(c, "trigger") || c == "template_trigger_flags") return "Triggers";

        // effects, sound, simulation
        if (c == "fx" || StartsWith(c, "fx_") || c == "radiant_fx") return "Effects";
        if (StartsWith(c, "sound_")) return "Audio";
        if (StartsWith(c, "wave_") || c == "rope" || c == "dyn_hingeconstraint" || c == "dyn_pointconstraint" || c == "undertow_volume"
            || StartsWith(c, "radiant_fx_physics")) return "Water & Physics";

        // geometry
        if (c == "script_brushmodel" || c == "dyn_brushmodel" || StartsWith(c, "func_")) return "Brush Entities";
        if (c == "misc_volume_decal") return "Decals";
        if (c == "script_model" || c == "dyn_model") return "Script Models";
        if (StartsWith(c, "misc_model") || c == "misc_prop") return "Static Models";

        // markers and tools
        if (c == "script_struct" || c == "script_origin" || c == "info_null" || c == "info_notnull" || c == "info_notnull_big" || Contains(c, "struct"))
            return "Script";
        if (in({ "export_volume", "fpstool_camera", "led_reference" })) return "Editor & Tools";
        if (StartsWith(c, "volume_") || EndsWith(c, "_volume") || c == "info_volume") return "Volumes";
        if (!IsBlank(model)) return "Static Models";
        return "Other";
    }

    bool GroupState::HasGroup(const std::string& key) const
    {
        for (const std::string& g : groups) if (EqualsCi(g, key)) return true;
        return false;
    }

    // ---- tree ----------------------------------------------------------------------------------------------------
    int Tree::FindGuid(const std::string& guid) const { auto it = byGuid.find(guid); return it == byGuid.end() ? -1 : it->second; }
    int Tree::FindGroup(const std::string& key) const { auto it = byGroup.find(key); return it == byGroup.end() ? -1 : it->second; }
    void Tree::Descendants(int node, std::vector<int>& out) const
    {
        if (node < 0) return;
        for (int c : nodes[node].children) { out.push_back(c); Descendants(c, out); }
    }
    std::vector<std::string> Tree::EntityGuidsUnder(int node) const
    {
        std::vector<std::string> out;
        if (node < 0) return out;
        if (nodes[node].HasEntity()) { out.push_back(nodes[node].guid); return out; }
        std::vector<int> all; Descendants(node, all);
        std::set<std::string, LessCi> seen;
        for (int d : all) if (nodes[d].HasEntity() && seen.insert(nodes[d].guid).second) out.push_back(nodes[d].guid);
        return out;
    }
    bool Tree::IsDescendant(int ancestor, int node) const
    {
        for (int at = node >= 0 ? nodes[node].parent : -1; at >= 0; at = nodes[at].parent) if (at == ancestor) return true;
        return false;
    }
    bool Tree::CanDeleteGroup(int node) const
    {
        if (node < 0 || !nodes[node].IsGroup()) return false;
        std::vector<int> all; Descendants(node, all);
        for (int d : all) if (nodes[d].CanDelete()) return true;
        return false;
    }
    bool Tree::Shown(int node) const
    {
        const Node& n = nodes[node];
        return n.searchMatch && !(n.IsGroup() && !n.custom && n.children.empty());
    }

    namespace
    {
        // The group this group sits inside when that group still exists and no loop would form; "" = under the map.
        std::string ParentGroupOf(const GroupState& state, const std::string& key)
        {
            auto it = state.parents.find(key);
            if (it == state.parents.end() || !state.IsKnownGroup(it->second)) return std::string();
            std::set<std::string, LessCi> seen{ key };
            for (std::string at = it->second; !at.empty();)
            {
                if (!seen.insert(at).second) return std::string();   // a loop (stale saved state): under the map instead
                auto next = state.parents.find(at);
                at = next != state.parents.end() && state.IsKnownGroup(next->second) ? next->second : std::string();
            }
            return it->second;
        }

        // Pinned entities go to the top of the map in their fixed order; group members after the group's sub-groups,
        // sorted by name.
        void AddToGroup(Tree& t, int group, int node)
        {
            auto& kids = t.nodes[group].children;
            size_t at = 0;
            if (t.nodes[group].kind == Kind::Map)
            {
                int rank = PinnedRank(t.nodes[node].className);
                while (at < kids.size() && t.nodes[kids[at]].pinned && PinnedRank(t.nodes[kids[at]].className) <= rank) ++at;
            }
            else
            {
                while (at < kids.size() && t.nodes[kids[at]].IsGroup()) ++at;
                while (at < kids.size() && CompareCi(t.nodes[kids[at]].name, t.nodes[node].name) <= 0) ++at;
            }
            kids.insert(kids.begin() + static_cast<ptrdiff_t>(at), node);
            t.nodes[node].parent = group;
        }
    }

    Tree Build(const std::string& mapName, const std::vector<Entity>& entities, const GroupState& state)
    {
        Tree t;
        Node root; root.kind = Kind::Map; root.name = mapName; root.className = "map"; root.color = kMapColor; root.shape = 1; root.expanded = true;
        t.nodes.push_back(root);

        // Custom groups first (sorted), then the built-in categories; each inside the group the user put it in.
        std::vector<std::string> custom;
        for (const std::string& g : state.groups) if (!IsBlank(g)) custom.push_back(g);
        std::stable_sort(custom.begin(), custom.end(), [](const std::string& a, const std::string& b) { return CompareCi(a, b) < 0; });
        std::vector<int> groups;
        for (const std::string& g : custom)
        {
            Node n; n.kind = Kind::Group; n.name = g; n.key = g; n.custom = true; n.color = kCustomGroupColor; n.shape = 2; n.expanded = true;
            groups.push_back(static_cast<int>(t.nodes.size())); t.nodes.push_back(n);
        }
        for (const Category& c : kCategories)
        {
            Node n; n.kind = Kind::Group; n.name = c.key; n.key = c.key; n.color = c.color; n.shape = c.shape;
            groups.push_back(static_cast<int>(t.nodes.size())); t.nodes.push_back(n);
        }
        for (int g : groups) t.byGroup[t.nodes[g].key] = g;
        for (int g : groups)
        {
            std::string parentKey = ParentGroupOf(state, t.nodes[g].key);
            int parent = parentKey.empty() ? -1 : t.FindGroup(parentKey);
            if (parent < 0) parent = 0;
            t.nodes[parent].children.push_back(g);
            t.nodes[g].parent = parent;
        }

        std::vector<size_t> order(entities.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return CompareCi(entities[a].name, entities[b].name) < 0; });
        for (size_t i : order)
        {
            const Entity& e = entities[i];
            std::string category = Classify(e);
            bool pinned = category == kPinnedGroup;
            Node n; n.kind = Kind::Entity; n.name = e.name; n.guid = e.guid; n.className = e.className; n.entity = static_cast<int>(i);
            n.pinned = pinned; n.shape = 0;
            n.color = pinned ? PinnedColor(e.className) : CategoryOf(category).color;
            int node = static_cast<int>(t.nodes.size());
            t.nodes.push_back(n);
            int group = 0;   // map settings stay at the top
            if (!pinned)
            {
                auto a = state.assignments.find(e.guid);
                std::string destination = a != state.assignments.end() && t.FindGroup(a->second) >= 0 ? a->second : category;
                group = t.FindGroup(destination);
                if (group < 0) group = t.FindGroup(kCategories[kCategoryCount - 1].key);
                if (group < 0) group = 0;
            }
            AddToGroup(t, group, node);
            t.byGuid[e.guid] = node;
        }
        for (int g : groups)
            if (!t.nodes[g].custom) t.nodes[g].expanded = !t.nodes[g].children.empty() && t.nodes[g].children.size() <= 40;
        return t;
    }

    std::vector<int> ApplySearch(Tree& tree, const std::vector<Entity>& entities, const std::string& rawTerm)
    {
        std::vector<int> opened;
        std::string term = Trim(rawTerm);
        struct Walk
        {
            Tree& t; const std::vector<Entity>& ents; const std::string& term; std::vector<int>& opened;
            bool Run(int node)
            {
                Node& n = t.nodes[node];
                bool anyChild = false;
                for (int c : n.children) anyChild |= Run(c);
                bool self = term.empty() || ContainsCi(n.name, term) || ContainsCi(n.className, term);
                if (!self && n.HasEntity() && n.entity >= 0)
                    for (const auto& kv : ents[n.entity].props) if (ContainsCi(kv.second, term)) { self = true; break; }
                bool visible = term.empty() || n.kind == Kind::Map || (n.HasEntity() ? self : anyChild || self);
                n.searchMatch = visible;
                if (!term.empty() && anyChild && (n.IsGroup() || n.kind == Kind::Map)) opened.push_back(node);
                return visible && (n.HasEntity() ? self : anyChild || self);
            }
        } walk{ tree, entities, term, opened };
        if (!tree.nodes.empty()) walk.Run(0);
        return opened;
    }

    // ---- moves -----------------------------------------------------------------------------------------------------
    int DestinationGroup(const Tree& tree, int target)
    {
        const Node& n = tree.nodes[target];
        if (n.kind == Kind::Map) return -1;
        if (n.IsGroup()) return target;
        if (n.HasEntity() && n.parent >= 0 && tree.nodes[n.parent].IsGroup()) return n.parent;
        return -1;
    }

    bool CanMove(const Tree& tree, int source, int target)
    {
        if (source < 0 || target < 0 || source == target) return false;
        const Node& s = tree.nodes[source];
        const Node& t = tree.nodes[target];
        if (s.kind == Kind::Map || s.pinned) return false;
        if (s.IsGroup())
        {
            int destination = DestinationGroup(tree, target);
            return destination < 0 || (destination != source && !tree.IsDescendant(source, destination));
        }
        return s.HasEntity() && (t.IsGroup() || t.kind == Kind::Map || t.HasEntity());
    }

    bool MoveEntity(GroupState& state, const Tree& tree, const std::vector<Entity>& entities, int source, int target)
    {
        const Node& s = tree.nodes[source];
        if (!s.HasEntity() || s.pinned || s.entity < 0) return false;
        int destination = DestinationGroup(tree, target);
        std::string natural = Classify(entities[s.entity]);
        if (destination < 0 || EqualsCi(tree.nodes[destination].key, natural))
            return state.assignments.erase(s.guid) > 0;   // back where it belongs by default
        auto it = state.assignments.find(s.guid);
        if (it != state.assignments.end() && EqualsCi(it->second, tree.nodes[destination].key)) return false;
        state.assignments[s.guid] = tree.nodes[destination].key;
        return true;
    }

    bool GroupTwo(GroupState& state, const Tree& tree, int source, int target)
    {
        const Node& s = tree.nodes[source];
        const Node& t = tree.nodes[target];
        if (source == target || !s.HasEntity() || !t.HasEntity()) return false;
        std::string group = CreateGroup(state, t.name + " Group");
        if (t.parent >= 0 && tree.nodes[t.parent].IsGroup()) state.parents[group] = tree.nodes[t.parent].key;   // where the two were
        state.assignments[s.guid] = group;
        state.assignments[t.guid] = group;
        return true;
    }

    bool MoveGroup(GroupState& state, const Tree& tree, int source, int target)
    {
        const std::string& key = tree.nodes[source].key;
        int destination = DestinationGroup(tree, target);
        auto current = state.parents.find(key);
        if (destination < 0)
        {
            if (current == state.parents.end()) return false;
            state.parents.erase(current);
        }
        else
        {
            if (current != state.parents.end() && EqualsCi(current->second, tree.nodes[destination].key)) return false;
            state.parents[key] = tree.nodes[destination].key;
        }
        return true;
    }

    bool MoveToGroup(GroupState& state, const Tree& tree, const std::vector<Entity>& entities, int source, int target)
    {
        if (!CanMove(tree, source, target)) return false;
        if (tree.nodes[source].IsGroup()) return MoveGroup(state, tree, source, target);
        // One entity dropped on another in the same group: a new group of the two. Anywhere else: the target's group.
        const Node& t = tree.nodes[target];
        if (t.HasEntity() && tree.nodes[source].parent == t.parent && !t.pinned) return GroupTwo(state, tree, source, target);
        return MoveEntity(state, tree, entities, source, target);
    }

    std::string CreateGroup(GroupState& state, const std::string& requested)
    {
        std::string name = IsBlank(requested) ? std::string("New Group") : Trim(requested);
        auto exists = [&](const std::string& n) { return state.HasGroup(n) || IsBuiltInGroup(n); };
        std::string base = name;
        for (int suffix = 2; exists(name); ++suffix) name = base + " " + std::to_string(suffix);
        state.groups.push_back(name);
        return name;
    }

    bool RemoveGroup(GroupState& state, const std::string& key)
    {
        auto g = std::find_if(state.groups.begin(), state.groups.end(), [&](const std::string& s) { return EqualsCi(s, key); });
        if (g == state.groups.end()) return false;
        std::string removed = *g;
        state.groups.erase(g);
        for (auto it = state.assignments.begin(); it != state.assignments.end();)
            it = EqualsCi(it->second, removed) ? state.assignments.erase(it) : std::next(it);
        // groups inside it move up to where it was
        std::string parent;
        auto p = state.parents.find(removed);
        if (p != state.parents.end()) { parent = p->second; state.parents.erase(p); }
        for (auto it = state.parents.begin(); it != state.parents.end();)
        {
            if (EqualsCi(it->second, removed))
            {
                if (parent.empty()) { it = state.parents.erase(it); continue; }
                it->second = parent;
            }
            ++it;
        }
        return true;
    }

    bool RenameGroup(GroupState& state, const std::string& key, const std::string& newName, std::string& error)
    {
        std::string name = Trim(newName);
        auto g = std::find_if(state.groups.begin(), state.groups.end(), [&](const std::string& s) { return EqualsCi(s, key); });
        if (g == state.groups.end()) { error = "Only custom groups can be renamed."; return false; }
        if (name.empty()) { error = "Enter a group name."; return false; }
        if (name == *g) return false;
        if (!EqualsCi(name, *g) && (state.HasGroup(name) || IsBuiltInGroup(name))) { error = "A group named '" + name + "' already exists."; return false; }
        std::string old = *g;
        *g = name;
        for (auto& a : state.assignments) if (EqualsCi(a.second, old)) a.second = name;
        CiMap parents;
        for (auto& p : state.parents) parents[EqualsCi(p.first, old) ? name : p.first] = EqualsCi(p.second, old) ? name : p.second;
        state.parents.swap(parents);
        return true;
    }

    std::string GroupEntities(GroupState& state, const Tree& tree, const std::vector<std::string>& guids, const std::string& requested)
    {
        std::vector<int> nodes;
        for (const std::string& guid : guids)
        {
            int n = tree.FindGuid(guid);
            if (n >= 0 && !tree.nodes[n].pinned) nodes.push_back(n);
        }
        if (nodes.empty()) return std::string();
        std::string group = CreateGroup(state, requested);
        // The new group sits where its members were when they were all in one group.
        int common = tree.nodes[nodes[0]].parent;
        for (int n : nodes) if (tree.nodes[n].parent != common) common = -1;
        if (common >= 0 && tree.nodes[common].IsGroup()) state.parents[group] = tree.nodes[common].key;
        for (int n : nodes) state.assignments[tree.nodes[n].guid] = group;
        return group;
    }

    // ---- store -------------------------------------------------------------------------------------------------------
    namespace
    {
        std::string Utf8(const std::wstring& w)
        {
            if (w.empty()) return {};
            int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
            std::string s(static_cast<size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
            return s;
        }
        std::wstring Wide(const std::string& s)
        {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
            return w;
        }
        bool Sha256(const std::string& data, unsigned char out[32])
        {
            BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
            bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0
                && BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0
                && BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), 0) == 0
                && BCryptFinishHash(hash, out, 32, 0) == 0;
            if (hash) BCryptDestroyHash(hash);
            if (alg) BCryptCloseAlgorithmProvider(alg, 0);
            return ok;
        }
        std::wstring LocalAppData()
        {
            PWSTR path = nullptr;
            std::wstring out;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)) && path) out = path;
            if (path) CoTaskMemFree(path);
            return out;
        }
    }

    std::string StateKey(const std::wstring& mapPath)
    {
        // Path.GetFullPath(mapPath).ToUpperInvariant(), UTF-8, SHA-256, upper-case hex (Convert.ToHexString).
        wchar_t full[4096];
        DWORD n = GetFullPathNameW(mapPath.c_str(), 4096, full, nullptr);
        std::wstring path = n > 0 && n < 4096 ? std::wstring(full, n) : mapPath;
        std::wstring upper(path.size(), L'\0');
        int m = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, path.c_str(), static_cast<int>(path.size()), &upper[0], static_cast<int>(upper.size()), nullptr, nullptr, 0);
        if (m <= 0) { upper = path; for (wchar_t& c : upper) if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - 32); }
        unsigned char digest[32];
        if (!Sha256(Utf8(upper), digest)) return std::string();
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        for (unsigned char b : digest) { out += hex[b >> 4]; out += hex[b & 15]; }
        return out;
    }

    std::wstring StateFile(const std::wstring& mapPath)
    {
        std::string key = StateKey(mapPath);
        std::wstring root = LocalAppData();
        if (key.empty() || root.empty()) return std::wstring();
        return root + L"\\Radiance\\HierarchyGroups\\" + Wide(key) + L".json";
    }

    // ---- JSON ----------------------------------------------------------------------------------------------------
    namespace
    {
        // System.Text.Json's default encoder: quotes, backslash and controls escaped, and everything outside printable
        // ASCII plus the HTML-sensitive characters as \uXXXX.
        std::string Quote(const std::string& s)
        {
            std::wstring w = Wide(s);
            std::string out = "\"";
            char buf[8];
            for (wchar_t c : w)
            {
                if (c == L'"') out += "\\\"";
                else if (c == L'\\') out += "\\\\";
                else if (c == L'\n') out += "\\n";
                else if (c == L'\r') out += "\\r";
                else if (c == L'\t') out += "\\t";
                else if (c == L'\b') out += "\\b";
                else if (c == L'\f') out += "\\f";
                else if (c < 0x20 || c >= 0x7F || c == L'<' || c == L'>' || c == L'&' || c == L'\'' || c == L'+' || c == L'`')
                { snprintf(buf, sizeof buf, "\\u%04X", static_cast<unsigned>(c)); out += buf; }
                else out += static_cast<char>(c);
            }
            return out + "\"";
        }

        struct Parser
        {
            const std::string& s; size_t i = 0;
            void Ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i; }
            bool Eat(char c) { Ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
            bool Str(std::string& out)
            {
                Ws();
                if (i >= s.size() || s[i] != '"') return false;
                ++i;
                std::wstring w;
                while (i < s.size() && s[i] != '"')
                {
                    char c = s[i++];
                    if (c != '\\')
                    {
                        // copy a UTF-8 run as is
                        size_t start = i - 1;
                        while (i < s.size() && s[i] != '"' && s[i] != '\\') ++i;
                        w += Wide(s.substr(start, i - start));
                        continue;
                    }
                    if (i >= s.size()) return false;
                    char e = s[i++];
                    switch (e)
                    {
                    case '"': w += L'"'; break; case '\\': w += L'\\'; break; case '/': w += L'/'; break;
                    case 'b': w += L'\b'; break; case 'f': w += L'\f'; break; case 'n': w += L'\n'; break;
                    case 'r': w += L'\r'; break; case 't': w += L'\t'; break;
                    case 'u':
                    {
                        if (i + 4 > s.size()) return false;
                        unsigned v = 0;
                        for (int k = 0; k < 4; ++k)
                        {
                            char h = s[i++]; v <<= 4;
                            if (h >= '0' && h <= '9') v |= unsigned(h - '0');
                            else if (h >= 'a' && h <= 'f') v |= unsigned(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') v |= unsigned(h - 'A' + 10);
                            else return false;
                        }
                        w += static_cast<wchar_t>(v);   // surrogate pairs arrive as two escapes and stay pairs in UTF-16
                        break;
                    }
                    default: return false;
                    }
                }
                if (i >= s.size()) return false;
                ++i;
                out = Utf8(w);
                return true;
            }
            bool Skip()   // any value
            {
                Ws();
                if (i >= s.size()) return false;
                char c = s[i];
                if (c == '"') { std::string t; return Str(t); }
                if (c == '{') { ++i; if (Eat('}')) return true; do { std::string k; if (!Str(k) || !Eat(':') || !Skip()) return false; } while (Eat(',')); return Eat('}'); }
                if (c == '[') { ++i; if (Eat(']')) return true; do { if (!Skip()) return false; } while (Eat(',')); return Eat(']'); }
                while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' ' && s[i] != '\r' && s[i] != '\n' && s[i] != '\t') ++i;
                return true;
            }
            bool Null() { Ws(); if (s.compare(i, 4, "null") == 0) { i += 4; return true; } return false; }
            bool StringArray(std::vector<std::string>& out)
            {
                if (Null()) return true;
                if (!Eat('[')) return false;
                if (Eat(']')) return true;
                do { std::string v; if (Null()) continue; if (!Str(v)) return false; out.push_back(v); } while (Eat(','));
                return Eat(']');
            }
            bool StringMap(CiMap& out)
            {
                if (Null()) return true;
                if (!Eat('{')) return false;
                if (Eat('}')) return true;
                do
                {
                    std::string k, v;
                    if (!Str(k) || !Eat(':')) return false;
                    if (Null()) continue;
                    if (!Str(v)) return false;
                    out[k] = v;
                } while (Eat(','));
                return Eat('}');
            }
        };
    }

    std::string ToJson(const GroupState& state)
    {
        const char* nl = "\r\n";
        std::string j = "{"; j += nl;
        j += "  \"Groups\": ";
        if (state.groups.empty()) j += "[]";
        else
        {
            j += "["; j += nl;
            for (size_t i = 0; i < state.groups.size(); ++i) { j += "    " + Quote(state.groups[i]); if (i + 1 < state.groups.size()) j += ","; j += nl; }
            j += "  ]";
        }
        j += ","; j += nl;
        auto map = [&](const char* name, const CiMap& m, bool last)
        {
            j += std::string("  \"") + name + "\": ";
            if (m.empty()) j += "{}";
            else
            {
                j += "{"; j += nl;
                size_t k = 0;
                for (const auto& kv : m) { j += "    " + Quote(kv.first) + ": " + Quote(kv.second); if (++k < m.size()) j += ","; j += nl; }
                j += "  }";
            }
            if (!last) j += ",";
            j += nl;
        };
        map("Assignments", state.assignments, false);
        map("Parents", state.parents, true);
        j += "}";
        return j;
    }

    bool FromJson(const std::string& text, GroupState& state)
    {
        state = GroupState{};
        size_t start = text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF ? 3 : 0;   // UTF-8 BOM
        std::string body = text.substr(start);
        Parser p{ body };
        if (!p.Eat('{')) return false;
        if (p.Eat('}')) return true;
        do
        {
            std::string key;
            if (!p.Str(key) || !p.Eat(':')) return false;
            bool ok;
            if (EqualsCi(key, "Groups")) ok = p.StringArray(state.groups);
            else if (EqualsCi(key, "Assignments")) ok = p.StringMap(state.assignments);
            else if (EqualsCi(key, "Parents")) ok = p.StringMap(state.parents);
            else ok = p.Skip();
            if (!ok) return false;
        } while (p.Eat(','));
        if (!p.Eat('}')) return false;
        // Radiance's loader drops blank names, assignments and parents.
        state.groups.erase(std::remove_if(state.groups.begin(), state.groups.end(), IsBlank), state.groups.end());
        std::vector<std::string> unique;
        for (const std::string& g : state.groups)
            if (std::none_of(unique.begin(), unique.end(), [&](const std::string& u) { return EqualsCi(u, g); })) unique.push_back(g);
        state.groups.swap(unique);
        for (auto it = state.assignments.begin(); it != state.assignments.end();) it = IsBlank(it->first) || IsBlank(it->second) ? state.assignments.erase(it) : std::next(it);
        for (auto it = state.parents.begin(); it != state.parents.end();) it = IsBlank(it->first) || IsBlank(it->second) ? state.parents.erase(it) : std::next(it);
        return true;
    }

    GroupState LoadState(const std::wstring& file)
    {
        GroupState state;
        if (file.empty()) return state;
        HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return state;
        std::string text;
        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (64LL << 20))
        {
            text.resize(static_cast<size_t>(size.QuadPart));
            DWORD read = 0;
            if (!ReadFile(h, &text[0], static_cast<DWORD>(text.size()), &read, nullptr)) read = 0;
            text.resize(read);
        }
        CloseHandle(h);
        if (!FromJson(text, state)) state = GroupState{};   // like Radiance: an unreadable file is an empty state
        return state;
    }

    bool SaveState(const std::wstring& file, const GroupState& state)
    {
        if (file.empty()) return false;
        std::wstring dir = file.substr(0, file.find_last_of(L'\\'));
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        std::wstring temp = file + L".tmp";
        std::string json = ToJson(state);
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        bool ok = WriteFile(h, json.data(), static_cast<DWORD>(json.size()), &written, nullptr) && written == json.size();
        CloseHandle(h);
        return ok && MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
}
