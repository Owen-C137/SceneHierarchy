# Changelog

## 1.0.0 (2026-10-17)

First release.

- **The pane.** Radiance's Hierarchy as a Radiant dock pane (Hierarchy menu); Radiance is not needed. The map is at
  the root, with its settings entities pinned at the top, then custom groups and the built-in categories.
- **Categories** built from Radiant's own entity definitions: all 184 defined classes plus actor_ / vehicle_
  spawners. Prefabs, Static / Script Models, Brush Entities, Lights, Reflection Probes, Color Grading, Lighting & Fog
  Volumes, Visibility & Performance, Zones, Player Spawns, AI Spawners, Vehicles, AI & Navigation, Triggers, Effects,
  Audio, Water & Physics, Script, Decals, Editor & Tools, Volumes and Other.
- **Names that say what an entity is:** fx show their effect, lights their PRIMARY_TYPE, sounds their alias, grading
  boxes their LVI, fog volumes their settings, and use triggers their hint string.
- **Custom groups** nest inside any group, including the built-in ones.
  - Drag and drop moves anything into anything; dropping on the map or on an entity's own category removes the
    custom assignment.
  - You can create, rename and remove groups, group the current selection, and drop one entity on another to group
    the two.
- **Multi-selection** (Ctrl / Shift) is kept in sync with Radiant's selection both ways.
- **Search** by name, class or any key value. **Hide / show** per row or group (Radiant's Hide Selected / Show
  Hidden), and Show everything hidden.
- **Delete** an entity, a whole group or the selection with Radiant's Edit/Delete (one undo step).
- **Dock Along Bottom / Top / Left / Right** (full width or full height) from the Hierarchy menu.
- **Groups are stored in Radiance's file** for each map, so both tools share them.
