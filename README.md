# Hierarchy (SceneHierarchy)

Radiance's Hierarchy pane as a plain Radiant dock pane. It works without Radiance: it is its own MTPlugins plugin
(`bin\plugins\SceneHierarchy\SceneHierarchy.dll`) and uses Radiant's Qt docking and the MTPlugins host UI API.

Open it from **Hierarchy > Hierarchy** (it docks beside Entity Info the first time) or with the Plugins window's Settings
button.

## What was ported (1:1 with Radiance)

- **The tree.** The map at the root. Its settings entities (worldspawn, volume_sun, umbra_volume, volume_fpstool) are
  pinned at the top in that order. Custom groups follow (sorted), then the built-in categories.
  - Entities are named by targetname. Otherwise they use the key that says what they are, taken from Radiant's
    entity definitions:

    | Class | Key |
    |---|---|
    | fx | `fxdef` (the effect) |
    | light, build_light | `PRIMARY_TYPE` (PRIMARY_OMNI when unset) |
    | sound_* | `script_sound` |
    | pbg_* | `color_lvi`, else `bloom_lvi` |
    | vcolor, vbloom boxes | `lvi` |
    | vlut_box, lut_volume | `index` ("LUT index N") |
    | world / lit fog volumes | `fsi` |
    | rope | `ropemodel`, else `material` |
    | misc_volume_decal | `modeloverridematerial` |
    | use triggers | `hintstring` |

    After that comes the model, then `script_noteworthy`, then the class name.
  - Rows are colour-coded by category. A group shows how many direct children it has. An entity shows its class beside
    its name when the two differ.
  - Empty built-in groups are hidden. Empty custom groups stay visible.
  - Custom groups start open. Built-in groups start open when they hold 1 to 40 rows.
- **Moving anything into anything** by drag and drop:
  - An entity dropped on a group goes into that group, whether custom or built-in.
  - An entity dropped on another entity goes into that entity's group.
  - An entity dropped on another entity *of the same group* makes a new group of the two ("<target> Group"), placed
    where they were.
  - Dropping on the map root, or on the entity's own category, removes the custom assignment.
  - Groups, custom or built-in, go inside any other group, or back under the map.
  - Invalid targets show the no-drop cursor: a group into itself or into its own descendants, the map root, or a
    pinned settings entity being dragged.
- **Groups.** You can create a group, remove a custom group (its entities go back to their categories and the groups
  inside it move up a level), and group the current selection (see below).
- **Search.** It filters by name, class or any key value as you type. Groups stay visible while they hold a match, and
  matching groups open.
- **Show / hide (the eye).** The eye hides or shows a row in Radiant. For a group it acts on everything in the group;
  the row's own selection is left alone.
  - A group whose entities are partly hidden shows a partial eye. Hidden rows are dimmed.
  - "Show everything hidden" runs Radiant's Show Hidden (Shift+H).
  - It works like Radiance: hiding selects the entities, runs View > Hide Entities > Hide Selected and restores the
    selection. Showing runs Show Hidden, then hides again whatever should stay hidden.
  - H and Shift+H pressed in Radiant (outside text fields) update the eyes too.
- **Selection in sync both ways.**
  - Selecting an entity row selects it in Radiant. Selecting a group row selects every entity in the group.
  - Radiant's selection selects the matching row and scrolls to it. A selected group stays selected while Radiant's
    selection is exactly its entities.
- **Context menu.** Select All in Group, Hide / Show, Delete All in Group, Delete Object and Remove Custom Group.
  Delete uses Radiant's own Edit/Delete: one undo step, and worldspawn is never deleted. Delete or Backspace in the
  tree deletes the selected row's entities.
- **Footer.** "N entities | N selected | N hidden".
- **Stored the way Radiance stores them.** Groups, assignments and parents are saved in
  `%LOCALAPPDATA%\Radiance\HierarchyGroups\<SHA-256 of the upper-cased full map path>.json`. That is the same file,
  key and JSON layout as Radiance (checked byte for byte against a file Radiance wrote), so both tools show the same
  groups.

## Additions (asked for in the brief, not in Radiance)

- **Multi-selection.** Ctrl+click adds or removes rows, Shift+click selects a range.
  - Radiant gets every entity of every selected row; a group counts as all of its entities.
  - Selecting several entities in Radiant selects each of their rows.
  - Hide / Show and Delete (context menu or Del) act on the whole selection when the clicked row is part of it.
  - Dragging part of the selection moves every selected row into the target's group.

- **Finer categories, built from Radiant's own entity definitions.** All 184 classes Radiant defines, plus the
  actor_ / vehicle_ classes it generates, map to a real category; a test checks that none falls into Other.
  - Prefabs, Static Models, Script Models, Brush Entities, Lights, Reflection Probes.
  - Color Grading (pbg_, vcolor_, vbloom_, vlut_, lut_volume), Lighting & Fog Volumes, Visibility & Performance
    (umbra, occluders, cull groups).
  - Zones (info_volume used as a zombies zone), Player Spawns (all MP / CP / ZM spawn points, influencers,
    intermissions), AI Spawners (actor_ spawners, zm_spawner_, spawn_manager, zombie spawn-location structs),
    Vehicles, AI & Navigation, Triggers.
  - Effects, Audio, Water & Physics, Script, Decals, Editor & Tools, Volumes, Other.
  - Radiance's own categories were coarser: probes and pbg boxes sat in Lights. The names Prefabs, Lights, Triggers,
    Script, Brush Entities, AI & Navigation, Volumes and Other are unchanged, so groups stored by Radiance that use
    them still apply. An assignment to one of Radiance's renamed categories (Models, Spawns, Effects & Audio) falls
    back to the entity's own category.

- **Rename Group** (context menu or F2) on custom groups. Every assignment and parent that names the group follows.
- **Group Selection**: from the **+** button or the context menu, it makes a new custom group of the entities
  selected in Radiant. Radiance only groups two rows by dropping one on the other, which still works too.

- **Dock along an outer edge.** The Hierarchy menu has "Dock Hierarchy Along Left (full height)" (and Right, Top,
  Bottom). This splits Radiant's whole dock area, which its drag and drop cannot do. See `RadiantDock.h`.

## What could not be matched 1:1, and why

- **The eye is a check box in the right-hand column.** Radiance draws an eye icon button. A Qt item check box is
  what Radiant's Qt tree offers without custom painting. It behaves the same: checked means shown, partial means
  partly hidden, and clicking it toggles.
- **Icons are coloured dots and squares** (custom groups get a gold folder) in each category's colour, instead of
  Radiance's vector icons. Radiant's Qt has no access to Radiance's icon set.
- **While you drag, Qt's own drop indicator shows the target** (a line between rows or a box around a row) instead of
  Radiance's blue row highlight. The drop always lands in the row under the cursor, as in Radiance, and invalid
  targets show the no-drop cursor.
- **Groups keep their open or closed state across rebuilds.** Radiance re-applies its defaults after every move.
  Keeping the state is friendlier and does not change any result.
- **Hiding done from Radiant's View menu with the mouse is not seen.** Radiant does not report what is hidden. As in
  Radiance, the pane tracks its own eye toggles plus the H and Shift+H keys. Show everything hidden resets the state.
- **Entity keys are read in time slices** (4 ms per slice, faster while a map is first filling in), the same approach
  as Radiance's native side. On a very large map the tree fills in over a few seconds, and the footer shows
  "reading..." until it is done. worldspawn's keys are read once per map, because its text holds every world brush.

## Building, testing, deploying

- Build: open `SceneHierarchy.vcxproj` in Visual Studio, or run
  `MSBuild SceneHierarchy.vcxproj /p:Configuration=Release /p:Platform=x64`.
  The build does not deploy.
- Test: `Tests\RunTests.ps1` runs 114 checks of the hierarchy rules, the moves, the store format and the state-file
  key.
- Deploy: `Deploy.ps1`. It refuses while Radiant, APE or the launcher is running. It backs up the deployed folder to
  `deployment-backups\<stamp>` and verifies the copied files by hash. Plugins in `bin\plugins` load automatically
  unless disabled in the Plugins window.
- Radiance (if it is enabled) replaces Radiant's window with its own editor. Switch to Radiant, or disable Radiance,
  to see this pane.
- Automation through MTPlugins' command channel (`OnCommand`): `status`, `rebuild`, `move <from> <to>`,
  `group <name>`, `rename <old>|<new>`, `remove <name>` and `parent <row>`. A row is `{guid}`, `group:<key>` or
  `root`.

`QtLite.h` holds the Qt calls the host API does not wrap: icons, the eye column, drag and drop, menus and prompts.
They are bound by export name from Radiant's own Qt DLLs, using the same technique as the host's `src\qt.cpp`. Each
call is optional. Everything that points into this DLL (event filters, signal slots) is torn down in `OnUnload`.
