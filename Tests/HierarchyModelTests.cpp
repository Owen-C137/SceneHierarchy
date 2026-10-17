// Unit checks of HierarchyModel against Radiance's behaviour (MainWindowViewModel / HierarchyGroupingStore).
#include "../HierarchyModel.h"
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

using namespace hm;

static Entity E(const char* guid, const char* cls, KeyValues extra = {})
{
    Entity e; e.guid = guid; e.className = cls;
    e.props.push_back({ "classname", cls });
    for (auto& kv : extra) e.props.push_back(kv);
    e.name = EntityName(e.props);
    return e;
}
static std::string Key(const Tree& t, int n) { return n < 0 ? std::string("<none>") : t.nodes[n].kind == Kind::Map ? std::string("<map>") : t.nodes[n].IsGroup() ? t.nodes[n].key : t.nodes[n].guid; }
static std::string ParentKey(const Tree& t, int n) { return Key(t, t.nodes[n].parent); }

int main(int argc, char** argv)
{
    // ---- classification ----
    CHECK(Classify(E("a", "worldspawn")) == kPinnedGroup);
    CHECK(Classify(E("a", "volume_sun")) == kPinnedGroup);
    CHECK(Classify(E("a", "trigger_multiple")) == "Triggers");
    CHECK(Classify(E("a", "misc_prefab")) == "Prefabs");
    CHECK(Classify(E("a", "script_struct", { { "model", "prefabs/x.MAP" } })) == "Prefabs");
    CHECK(Classify(E("a", "light")) == "Lights");
    CHECK(Classify(E("a", "pbg_box")) == "Color Grading");
    CHECK(Classify(E("a", "vcolor_spot")) == "Color Grading");
    CHECK(Classify(E("a", "reflection_probe")) == "Reflection Probes");
    CHECK(Classify(E("a", "build_probe")) == "Reflection Probes");
    CHECK(Classify(E("a", "volume_worldfog")) == "Lighting & Fog Volumes");
    CHECK(Classify(E("a", "sun_shadow_bias")) == "Lighting & Fog Volumes");
    CHECK(Classify(E("a", "umbra_tome")) == "Visibility & Performance");
    CHECK(Classify(E("a", "func_cullgroup")) == "Visibility & Performance");
    CHECK(Classify(E("a", "info_player_start")) == "Player Spawns");
    CHECK(Classify(E("a", "zm_uspawn_point")) == "Player Spawns");
    CHECK(Classify(E("a", "mp_tdm_spawn")) == "Player Spawns");
    CHECK(Classify(E("a", "zm_global_intermission")) == "Player Spawns");
    CHECK(Classify(E("a", "spawn_manager")) == "AI Spawners");
    CHECK(Classify(E("a", "actor_spawner_zm_factory_zombie")) == "AI Spawners");
    CHECK(Classify(E("a", "zm_spawner_riserLocation")) == "AI Spawners");
    CHECK(Classify(E("a", "script_struct", { { "script_noteworthy", "riser_location" } })) == "AI Spawners");
    CHECK(Classify(E("a", "info_volume", { { "targetname", "zone_start" } })) == "Zones");
    CHECK(Classify(E("a", "info_volume")) == "Volumes");
    CHECK(Classify(E("a", "nav_volume")) == "AI & Navigation");
    CHECK(Classify(E("a", "trigger_navmesh")) == "AI & Navigation");
    CHECK(Classify(E("a", "vehicle_t7_mil_tank")) == "Vehicles");
    CHECK(Classify(E("a", "volume_decal")) == "Volumes");
    CHECK(Classify(E("a", "fx_something")) == "Effects");
    CHECK(Classify(E("a", "sound_emitter")) == "Audio");
    CHECK(Classify(E("a", "wave_water")) == "Water & Physics");
    CHECK(Classify(E("a", "dyn_hingeconstraint")) == "Water & Physics");
    CHECK(Classify(E("a", "script_model")) == "Script Models");
    CHECK(Classify(E("a", "misc_volume_decal")) == "Decals");
    CHECK(Classify(E("a", "export_volume")) == "Editor & Tools");
    CHECK(Classify(E("a", "node_pathnode")) == "AI & Navigation");
    CHECK(Classify(E("a", "func_door")) == "Brush Entities");
    CHECK(Classify(E("a", "script_brushmodel")) == "Brush Entities");
    CHECK(Classify(E("a", "misc_model")) == "Static Models");
    CHECK(Classify(E("a", "info_something", { { "model", "p7_crate" } })) == "Static Models");
    CHECK(Classify(E("a", "script_struct")) == "Script");
    CHECK(Classify(E("a", "script_origin")) == "Script");
    CHECK(Classify(E("a", "weird_thing")) == "Other");
    CHECK(EntityName({ { "classname", "script_model" }, { "model", "m" }, { "targetname", "t" } }) == "t");
    CHECK(EntityName({ { "classname", "script_model" }, { "model", "m" } }) == "m");
    CHECK(EntityName({ { "classname", "light" } }) == "PRIMARY_OMNI");    // Radiant's default light type
    CHECK(EntityName({ { "classname", "light" }, { "PRIMARY_TYPE", "PRIMARY_SPOT" }, { "name", "light_001" } }) == "PRIMARY_SPOT");
    CHECK(EntityName({ { "classname", "light" }, { "PRIMARY_TYPE", "PRIMARY_SPOT" }, { "targetname", "lamp" } }) == "lamp");
    CHECK(EntityName({ { "classname", "fx" }, { "fxdef", "dlc3/stalingrad/fx_water_splash_burst_lrg" } }) == "dlc3/stalingrad/fx_water_splash_burst_lrg");
    CHECK(EntityName({ { "classname", "fx" } }) == "fx");
    CHECK(EntityName({ { "classname", "script_origin" } }) == "script_origin");
    CHECK(EntityName({ { "classname", "sound_looper" }, { "script_sound", "amb_wind_lp" } }) == "amb_wind_lp");
    CHECK(EntityName({ { "classname", "pbg_box" }, { "color_lvi", "zm_warm" } }) == "zm_warm");
    CHECK(EntityName({ { "classname", "vlut_box" }, { "index", "3" } }) == "LUT index 3");
    CHECK(EntityName({ { "classname", "volume_worldfog" }, { "fsi", "zm_fog_night" } }) == "zm_fog_night");
    CHECK(EntityName({ { "classname", "trigger_use" }, { "hintstring", "Press &&1 to open" } }) == "Press &&1 to open");
    CHECK(EntityName({ { "classname", "script_struct" }, { "script_noteworthy", "riser_location" } }) == "riser_location");
    CHECK(EntityName({ { "classname", "trigger_use" }, { "model", "" } }) == "trigger_use");
    CHECK(EntityName({}) == "Entity");
    CHECK(CompareCi("_a", "Ab") > 0);   // OrdinalIgnoreCase folds to upper case: '_' sorts after letters

    // every class Radiant defines (plugins\Radiance\Inventory\EntityDefinitionCatalog.json) has a real category
    static const char* radiantClasses[] = {
        "build_light", "build_probe", "cp_coop_respawn", "cp_coop_spawn", "cp_global_intermission", "cp_mobile_spawn", "cp_uspawn_influencer", "cp_uspawn_point", "dyn_brushmodel", "dyn_hingeconstraint", "dyn_model", "dyn_pointconstraint", "export_volume", "fpstool_camera", "func_cullgroup", "func_group", "fx", "heli_height_lock", "info_corona", "info_grenade_hint", "info_intermission", "info_notnull", "info_notnull_big", "info_null", "info_player_respawn", "info_player_start", "info_vehicle_node", "info_vehicle_node_avoidance", "info_vehicle_node_rotate", "info_volume", "klf", "led_reference", "lensflare", "light", "lightgrid_sky", "lod_info", "lut_volume", "misc_model", "misc_prefab", "misc_prop", "misc_volume_decal", "mp_ctf_spawn_allies", "mp_ctf_spawn_allies_start", "mp_ctf_spawn_axis", "mp_ctf_spawn_axis_start", "mp_dem_spawn_attacker", "mp_dem_spawn_attacker_a", "mp_dem_spawn_attacker_b", "mp_dem_spawn_attacker_remove_a", "mp_dem_spawn_attacker_remove_b", "mp_dem_spawn_attacker_start", "mp_dem_spawn_attackerOT_start", "mp_dem_spawn_defender", "mp_dem_spawn_defender_a", "mp_dem_spawn_defender_b", "mp_dem_spawn_defender_start", "mp_dem_spawn_defenderOT_start", "mp_dm_spawn", "mp_dm_spawn_start", "mp_dom_spawn", "mp_dom_spawn_allies_start", "mp_dom_spawn_axis_start", "mp_dom_spawn_flag_a", "mp_dom_spawn_flag_b", "mp_dom_spawn_flag_c", "mp_escort_spawn_attacker", "mp_escort_spawn_attacker_start", "mp_escort_spawn_defender", "mp_escort_spawn_defender_start", "mp_global_intermission", "mp_mobile_spawn", "mp_multi_team_spawn", "mp_res_spawn_allies", "mp_res_spawn_allies_a", "mp_res_spawn_allies_b", "mp_res_spawn_allies_start", "mp_res_spawn_axis", "mp_res_spawn_axis_a", "mp_res_spawn_axis_start", "mp_sab_spawn_allies", "mp_sab_spawn_allies_start", "mp_sab_spawn_axis", "mp_sab_spawn_axis_start", "mp_sd_spawn_attacker", "mp_sd_spawn_defender", "mp_tdm_spawn", "mp_tdm_spawn_allies_start", "mp_tdm_spawn_axis_start", "mp_tdm_spawn_team1_start", "mp_tdm_spawn_team2_start", "mp_tdm_spawn_team3_start", "mp_tdm_spawn_team4_start", "mp_tdm_spawn_team5_start", "mp_tdm_spawn_team6_start", "mp_uspawn_influencer", "mp_uspawn_point", "mp_wager_spawn", "named_occluder", "nav_volume", "nav_volume_passable", "node_ambush", "node_concealment_crouch", "node_concealment_prone", "node_concealment_stand", "node_cover_crouch", "node_cover_crouch_window", "node_cover_left", "node_cover_pillar", "node_cover_prone", "node_cover_right", "node_cover_stand", "node_exposed", "node_guard", "node_negotiation_begin", "node_negotiation_end", "node_pathnode", "node_scripted", "node_turret", "occlusion_override", "pbg_box", "pbg_omni", "pbg_spot", "radiant_fx_physics", "reflection_probe", "rope", "script_brushmodel", "script_model", "script_origin", "script_struct", "sound_line_emitter", "sound_looper", "sound_random", "spawn_manager", "sun_shadow_bias", "sun_shadow_quality_hi", "sun_shadow_quality_low", "template_trigger_flags", "trigger_box", "trigger_damage", "trigger_hurt", "trigger_ik_disable_terrain", "trigger_ik_playerclip_terrain", "trigger_lookat", "trigger_multiple", "trigger_navmesh", "trigger_once", "trigger_out_of_bounds", "trigger_radius", "trigger_use", "trigger_use_touch", "umbra_parameter_volume", "umbra_tome", "umbra_tome_trigger", "umbra_volume", "undertow_volume", "vbloom_box", "vcolor_box", "vcolor_omni", "vcolor_spot", "vlut_box", "volume_attenuation", "volume_exposure", "volume_fpstool", "volume_lightclip", "volume_litfog", "volume_outdoor", "volume_performance", "volume_reflection", "volume_sun", "volume_sun_extension", "volume_vista", "volume_weathergrime", "volume_worldfog", "volume_worldfogmodifier", "wave_clamp", "wave_generator", "wave_water", "worldspawn", "zm_global_intermission", "zm_mobile_spawn", "zm_spawner_riserLocation", "zm_spawner_spawnLocation", "zm_uspawn_influencer", "zm_uspawn_point",
    };
    bool table = argc > 1 && std::string(argv[1]) == "--table";
    for (const char* cls : radiantClasses)
    {
        std::string cat = Classify(E("x", cls));
        if (table) std::printf("%-34s %s\n", cls, cat.c_str());
        if (cat == "Other") { ++g_fail; std::printf("FAIL unclassified Radiant class %s\n", cls); }
    }

    // ---- build ----
    std::vector<Entity> ents = {
        E("{W}", "worldspawn"), E("{S}", "volume_sun"), E("{M1}", "misc_model", { { "model", "crate_b" } }), E("{M2}", "misc_model", { { "model", "crate_a" } }),
        E("{L1}", "light"), E("{T1}", "trigger_multiple", { { "targetname", "door_trig" } }), E("{P1}", "misc_prefab", { { "model", "prefabs/a.map" } }),
    };
    GroupState st;
    Tree t = Build("mymap", ents, st);
    CHECK(t.nodes[0].kind == Kind::Map && t.nodes[0].name == "mymap" && t.nodes[0].expanded);
    CHECK(t.nodes[0].children.size() >= 2);
    CHECK(Key(t, t.nodes[0].children[0]) == "{W}");                // pinned, worldspawn first
    CHECK(Key(t, t.nodes[0].children[1]) == "{S}");
    CHECK(Key(t, t.nodes[0].children[2]) == "Prefabs");            // then the categories in order
    int models = t.FindGroup("static models");
    CHECK(models >= 0 && t.nodes[models].children.size() == 2);
    CHECK(Key(t, t.nodes[models].children[0]) == "{M2}");          // sorted by name: crate_a, crate_b
    CHECK(t.nodes[models].expanded);                               // 1..40 entities: open
    CHECK(!t.Shown(t.FindGroup("Player Spawns")));                        // empty built-in groups are hidden
    CHECK(t.Shown(models));
    CHECK(t.nodes[t.FindGuid("{T1}")].Detail() == "trigger_multiple");
    CHECK(t.nodes[t.FindGuid("{L1}")].name == "PRIMARY_OMNI" && t.nodes[t.FindGuid("{L1}")].Detail() == "light");

    // ---- custom groups ----
    std::string g = CreateGroup(st, "");
    CHECK(g == "New Group");
    CHECK(CreateGroup(st, "new group") == "new group 2");
    CHECK(CreateGroup(st, " Lights ") == "Lights 2");               // a built-in name is taken
    st = GroupState{};
    CreateGroup(st, "Zeta"); CreateGroup(st, "alpha");
    t = Build("mymap", ents, st);
    CHECK(Key(t, t.nodes[0].children[2]) == "alpha");              // custom groups sorted, above the categories
    CHECK(Key(t, t.nodes[0].children[3]) == "Zeta");
    CHECK(t.Shown(t.FindGroup("alpha")));                          // empty custom groups stay visible
    CHECK(t.nodes[t.FindGroup("alpha")].expanded);

    // entity into a custom group, then back by dropping on the map, then onto its own category
    int m1 = t.FindGuid("{M1}"), alpha = t.FindGroup("alpha");
    CHECK(CanMove(t, m1, alpha));
    CHECK(MoveToGroup(st, t, ents, m1, alpha));
    CHECK(st.assignments["{M1}"] == "alpha");
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGuid("{M1}")) == "alpha");
    CHECK(MoveToGroup(st, t, ents, t.FindGuid("{M1}"), 0));
    CHECK(st.assignments.count("{M1}") == 0);
    t = Build("mymap", ents, st);
    CHECK(MoveToGroup(st, t, ents, t.FindGuid("{M1}"), t.FindGroup("Lights")));   // into another built-in group
    CHECK(st.assignments["{m1}"] == "Lights");
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGuid("{M1}")) == "Lights");
    CHECK(MoveToGroup(st, t, ents, t.FindGuid("{M1}"), t.FindGroup("Static Models")));   // its own category: assignment removed
    CHECK(st.assignments.empty());
    t = Build("mymap", ents, st);
    // dropped on an entity of another group: into that entity's group
    CHECK(MoveToGroup(st, t, ents, t.FindGuid("{M1}"), t.FindGuid("{L1}")));
    CHECK(st.assignments["{M1}"] == "Lights");
    st.assignments.clear();
    t = Build("mymap", ents, st);
    // pinned entities and the map never move
    CHECK(!CanMove(t, t.FindGuid("{W}"), alpha));
    CHECK(!CanMove(t, 0, alpha));

    // one entity dropped on another of the same group: a new group of the two, where they were
    size_t before = st.groups.size();
    CHECK(MoveToGroup(st, t, ents, t.FindGuid("{M1}"), t.FindGuid("{M2}")));
    CHECK(st.groups.size() == before + 1);
    CHECK(st.groups.back() == "crate_a Group");
    CHECK(st.parents["crate_a Group"] == "Static Models");
    CHECK(st.assignments["{M1}"] == "crate_a Group" && st.assignments["{M2}"] == "crate_a Group");
    t = Build("mymap", ents, st);
    int pair = t.FindGroup("crate_a Group");
    CHECK(ParentKey(t, pair) == "Static Models");
    CHECK(t.nodes[pair].children.size() == 2);
    CHECK(t.Shown(t.FindGroup("Static Models")));                         // holds the sub-group

    // groups into groups (built-in into custom too), never into themselves
    CHECK(MoveToGroup(st, t, ents, t.FindGroup("Lights"), t.FindGroup("alpha")));
    CHECK(st.parents["Lights"] == "alpha");
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGroup("Lights")) == "alpha");
    CHECK(!CanMove(t, t.FindGroup("alpha"), t.FindGroup("Lights")));        // alpha into its own child
    CHECK(!CanMove(t, t.FindGroup("alpha"), t.FindGuid("{L1}")));           // ... or onto an entity in it
    CHECK(CanMove(t, t.FindGroup("alpha"), t.FindGroup("Zeta")));
    CHECK(MoveToGroup(st, t, ents, t.FindGroup("alpha"), t.FindGroup("Zeta")));
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGroup("alpha")) == "Zeta");
    CHECK(ParentKey(t, t.FindGroup("Lights")) == "alpha");
    CHECK(!MoveToGroup(st, t, ents, t.FindGroup("alpha"), t.FindGroup("Zeta")));   // already there
    CHECK(MoveToGroup(st, t, ents, t.FindGroup("Lights"), 0));                      // back under the map
    CHECK(st.parents.count("Lights") == 0);
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGroup("Lights")) == "<map>");

    // removing a group: its entities go home, groups inside move up a level
    st.parents["Lights"] = "alpha";
    st.assignments["{T1}"] = "alpha";
    CHECK(RemoveGroup(st, "ALPHA"));
    CHECK(!st.HasGroup("alpha"));
    CHECK(st.parents["Lights"] == "Zeta");
    CHECK(st.assignments.count("{T1}") == 0);
    CHECK(!RemoveGroup(st, "Lights"));                             // built-in groups cannot be removed
    t = Build("mymap", ents, st);
    CHECK(ParentKey(t, t.FindGroup("Lights")) == "Zeta");

    // rename (an addition): every reference follows
    std::string err;
    CHECK(RenameGroup(st, "zeta", "Omega", err));
    CHECK(st.HasGroup("Omega") && !st.HasGroup("Zeta"));
    CHECK(st.parents["Lights"] == "Omega");
    CHECK(!RenameGroup(st, "Omega", "Static Models", err) && !err.empty());
    CHECK(!RenameGroup(st, "Lights", "Foo", err));
    CHECK(RenameGroup(st, "Omega", "omega", err));                 // a case-only rename is allowed

    // a stale loop in Parents shows the groups under the map
    GroupState loop; loop.groups = { "A", "B" }; loop.parents["A"] = "B"; loop.parents["B"] = "A";
    Tree lt = Build("m", ents, loop);
    CHECK(ParentKey(lt, lt.FindGroup("A")) == "<map>");
    CHECK(ParentKey(lt, lt.FindGroup("B")) == "<map>");
    // an assignment to a group that no longer exists falls back to the category
    GroupState stale; stale.assignments["{M1}"] = "Gone";
    Tree stt = Build("m", ents, stale);
    CHECK(ParentKey(stt, stt.FindGuid("{M1}")) == "Static Models");

    // group several entities (an addition)
    GroupState gs;
    Tree gt = Build("m", ents, gs);
    std::string made = GroupEntities(gs, gt, { "{M1}", "{M2}", "{W}" }, "Crates");
    CHECK(made == "Crates");
    CHECK(gs.parents["Crates"] == "Static Models");
    CHECK(gs.assignments.size() == 2);                             // worldspawn stays pinned

    // ---- search ----
    GroupState ss;
    Tree sr = Build("m", ents, ss);
    ApplySearch(sr, ents, "door");
    CHECK(sr.nodes[sr.FindGuid("{T1}")].searchMatch);              // targetname
    CHECK(!sr.nodes[sr.FindGuid("{L1}")].searchMatch);
    CHECK(sr.nodes[sr.FindGroup("Triggers")].searchMatch);
    CHECK(!sr.nodes[sr.FindGroup("Lights")].searchMatch);
    ApplySearch(sr, ents, "CRATE_B");                              // a key value (model), ignoring case
    CHECK(sr.nodes[sr.FindGuid("{M1}")].searchMatch && !sr.nodes[sr.FindGuid("{M2}")].searchMatch);
    ApplySearch(sr, ents, "  ");
    CHECK(sr.nodes[sr.FindGuid("{L1}")].searchMatch);

    // ---- store ----
    GroupState js;
    js.groups = { "New Group", "Caf\xC3\xA9 & <stuff>" };
    js.assignments["{A}"] = "New Group";
    js.parents["Lights"] = "Caf\xC3\xA9 & <stuff>";
    std::string json = ToJson(js);
    CHECK(json.find("\"Caf\\u00E9 \\u0026 \\u003Cstuff\\u003E\"") != std::string::npos);
    GroupState back;
    CHECK(FromJson(json, back));
    CHECK(back.groups == js.groups && back.assignments == js.assignments && back.parents == js.parents);
    GroupState empty;
    CHECK(ToJson(empty) == "{\r\n  \"Groups\": [],\r\n  \"Assignments\": {},\r\n  \"Parents\": {}\r\n}");
    const char* radiance = "{\r\n  \"Groups\": [\r\n    \"New Group\"\r\n  ],\r\n  \"Assignments\": {},\r\n  \"Parents\": {}\r\n}";
    GroupState fromRadiance;
    CHECK(FromJson(radiance, fromRadiance) && fromRadiance.groups.size() == 1 && fromRadiance.groups[0] == "New Group");
    CHECK(ToJson(fromRadiance) == radiance);                       // byte for byte what Radiance wrote
    GroupState nulls;
    CHECK(FromJson("{\"Groups\":[\"a\",\" \"],\"Assignments\":{\"x\":\"a\",\"y\":\"\"},\"Parents\":null}", nulls));
    CHECK(nulls.groups.size() == 1 && nulls.assignments.size() == 1 && nulls.parents.empty());
    CHECK(StateKey(L"D:\\SteamLibrary\\steamapps\\common\\Call of Duty Black Ops III\\map_source\\zm\\zm_weapon_ports.map")
          == "3ECD09E119A83EBB8CBBD4C4F1ED9A489B4C42475F70A0EDF691EC904BF5BA99");   // the file Radiance wrote for that map
    CHECK(StateKey(L"d:/steamlibrary/steamapps/common/call of duty black ops iii/map_source/zm/zm_weapon_ports.map")
          == "3ECD09E119A83EBB8CBBD4C4F1ED9A489B4C42475F70A0EDF691EC904BF5BA99");   // same file for any spelling

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
