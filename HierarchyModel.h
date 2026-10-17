// HierarchyModel: the Hierarchy pane's rules, ported 1:1 from Radiance (Editor\Services\HierarchyGroupingStore.cs and
// the hierarchy part of Editor\ViewModels\MainWindowViewModel.cs). No UI and no Radiant calls, so it is unit tested.
//
// The tree: the map at the root, its settings entities pinned at the top, then custom groups (sorted) and the
// built-in categories. A group (custom or built-in) can sit inside any other group (Parents); an entity can be
// assigned to any group other than its own category (Assignments). Radiant's map is never changed by any of this.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace hm
{
    // String.Compare(a, b, StringComparison.OrdinalIgnoreCase) for UTF-8 text (ASCII letters folded to upper case).
    int CompareCi(const std::string& a, const std::string& b);
    bool EqualsCi(const std::string& a, const std::string& b);
    bool ContainsCi(const std::string& text, const std::string& term);
    std::string Lower(const std::string& s);
    struct LessCi { bool operator()(const std::string& a, const std::string& b) const { return CompareCi(a, b) < 0; } };
    using CiMap = std::map<std::string, std::string, LessCi>;

    using KeyValues = std::vector<std::pair<std::string, std::string>>;

    struct Entity
    {
        std::string guid, className, name, origin;
        KeyValues props;
    };
    // Radiance's entity name: targetname, else model, else the class name ("Entity" without one).
    std::string EntityName(const KeyValues& keys);
    std::string FindKey(const KeyValues& keys, const char* key);

    struct Category { const char* key; uint32_t color; int shape; const char* icon; };   // shape: 0 dot, 1 square, 2 folder; icon: a Radiance glyph name (RadianceIcons.h)
    extern const Category kCategories[];
    extern const size_t kCategoryCount;   // the last category is "Other"
    extern const char* const kPinnedClasses[4];
    extern const char* const kPinnedGroup;            // "@map"
    const Category& CategoryOf(const std::string& key);
    uint32_t PinnedColor(const std::string& className);
    const char* PinnedIcon(const std::string& className);   // Radiance's PinnedIcon: the glyph of a map-settings row
    std::string Classify(const Entity& e);            // a category key, or kPinnedGroup
    bool IsBuiltInGroup(const std::string& key);
    constexpr uint32_t kCustomGroupColor = 0xD6B05C, kMapColor = 0x78AEF0;

    // HierarchyGroupState: what Radiance stores per map.
    struct GroupState
    {
        std::vector<std::string> groups;   // custom group names (keys), unique ignoring case
        CiMap assignments;                 // entity GUID -> group key
        CiMap parents;                     // group key -> the group it is inside (absent = under the map)
        bool HasGroup(const std::string& key) const;
        bool IsKnownGroup(const std::string& key) const { return HasGroup(key) || IsBuiltInGroup(key); }
    };

    enum class Kind { Map, Group, Entity };
    struct Node
    {
        Kind kind = Kind::Entity;
        std::string name, key, guid, className;   // key: group key; guid: entity GUID
        bool custom = false, pinned = false;
        uint32_t color = 0; int shape = 0; const char* icon = "tag";
        int parent = -1;
        std::vector<int> children;
        int entity = -1;                          // index into the entity list
        bool expanded = false;                    // the default open state (Radiance's IsExpanded at build time)
        bool searchMatch = true;                  // IsSearchMatch
        bool IsGroup() const { return kind == Kind::Group; }
        bool HasEntity() const { return kind == Kind::Entity; }
        bool CanDelete() const { return kind == Kind::Entity && !EqualsCi(className, "worldspawn"); }
        // Detail: the class name beside an entity whose name says something else.
        std::string Detail() const { return kind == Kind::Entity && !EqualsCi(name, className) ? className : std::string(); }
    };

    struct Tree
    {
        std::vector<Node> nodes;                  // nodes[0] is the map
        std::unordered_map<std::string, int> byGuid;   // exact GUID text -> entity node
        std::map<std::string, int, LessCi> byGroup;    // group key -> group node
        int Root() const { return nodes.empty() ? -1 : 0; }
        int FindGuid(const std::string& guid) const;
        int FindGroup(const std::string& key) const;
        void Descendants(int node, std::vector<int>& out) const;
        std::vector<std::string> EntityGuidsUnder(int node) const;   // node itself when it is an entity
        bool IsDescendant(int ancestor, int node) const;
        bool CanDeleteGroup(int node) const;
        // IsShownInTree: matches the search, and built-in groups only when they hold something.
        bool Shown(int node) const;
    };

    // RebuildHierarchy.
    Tree Build(const std::string& mapName, const std::vector<Entity>& entities, const GroupState& state);
    // ApplyHierarchySearch: sets searchMatch (and opens groups holding a match) for `term`. Returns the groups opened.
    std::vector<int> ApplySearch(Tree& tree, const std::vector<Entity>& entities, const std::string& term);

    // ---- moving things: anything into anything --------------------------------------------------------------
    bool CanMove(const Tree& tree, int source, int target);
    int DestinationGroup(const Tree& tree, int target);    // -1 = the map
    // MoveHierarchyNodeToGroup: groups move into groups, an entity dropped on another of its own group makes a new
    // group of the two, anything else goes into the target's group. Returns true when the state changed.
    bool MoveToGroup(GroupState& state, const Tree& tree, const std::vector<Entity>& entities, int source, int target);
    bool MoveEntity(GroupState& state, const Tree& tree, const std::vector<Entity>& entities, int source, int target);
    bool GroupTwo(GroupState& state, const Tree& tree, int source, int target);
    bool MoveGroup(GroupState& state, const Tree& tree, int source, int target);

    // CreateCustomGroup: a unique name (" 2", " 3"... added) among custom groups and every built-in category.
    std::string CreateGroup(GroupState& state, const std::string& requested);
    // RemoveCustomGroup: its entities go back to their categories, groups inside it move up a level.
    bool RemoveGroup(GroupState& state, const std::string& key);
    // Additions (not in Radiance): rename a custom group everywhere it is referenced; group several entities.
    bool RenameGroup(GroupState& state, const std::string& key, const std::string& newName, std::string& error);
    std::string GroupEntities(GroupState& state, const Tree& tree, const std::vector<std::string>& guids, const std::string& name);

    // ---- HierarchyGroupingStore: %LOCALAPPDATA%\Radiance\HierarchyGroups\<SHA-256 of the upper-cased full map path>.json
    std::wstring StateFile(const std::wstring& mapPath);
    std::string StateKey(const std::wstring& mapPath);     // the hex file name without ".json"
    GroupState LoadState(const std::wstring& file);
    bool SaveState(const std::wstring& file, const GroupState& state);
    std::string ToJson(const GroupState& state);           // System.Text.Json's indented layout
    bool FromJson(const std::string& text, GroupState& state);
}
