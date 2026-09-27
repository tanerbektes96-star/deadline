# V01 in the warehouse (DEADLINE_Karakter_Pipeline.md 8, step 5: "V01 depoda yurusun").
#
# Adds to LVL_Greybox: a NavMesh bounds volume over the warehouse floor, three
# patrol points (rack aisle -> supplier counter -> buyer counter) and one
# BP_NPC_V01 walking them. The rack-to-supplier leg has the van in the way, so
# the route only works if the NavMesh does.
#
# Safe to run repeatedly: actors are found by label and reused, and existing
# ones are never moved -- nudge the points in the editor, the script keeps them.
#
# Inside the editor: Tools > Execute Python Script. The NavMesh then builds on
# its own and is saved with the level on your next save.
# Headless (editor closed), builds the NavMesh, saves and quits:
#   UnrealEditor-Cmd.exe DEADLINE_.uproject "-ExecCmds=py <this file> --quit"
#       -unattended -nosplash -nop4 -RenderOffscreen
# (-ExecutePythonScript cannot be used: it quits before the NavMesh is built.)
import sys
import unreal

MAP = "/Game/Deadline/Maps/LVL_Greybox"
NPC_BP = "/Game/Deadline/Characters/NPC/V01/BP_NPC_V01"
NPC_LABEL = "NPC_V01_DepoGorevlisi"

# Warehouse floor is 30 x 20 m (x -1500..1500, y -1000..1000), top at z = 0.
NAV_LABEL = "NavMeshBounds_Warehouse"
NAV_CENTER = unreal.Vector(0.0, 0.0, 100.0)
NAV_SCALE = unreal.Vector(15.0, 10.0, 2.0)  # 200 cm brush -> 30 x 20 x 4 m

# label -> location. In the open, in front of what the worker would be serving.
PATROL = [
    ("NPCPoint_V01_Rack", unreal.Vector(-450.0, 380.0, 0.0)),      # Rack01 / pallet bay aisle
    ("NPCPoint_V01_Supplier", unreal.Vector(650.0, 450.0, 0.0)),   # past the van
    ("NPCPoint_V01_Buyer", unreal.Vector(650.0, -450.0, 0.0)),
]
PAUSE_AT_POINT = 3.0

QUIT = "--quit" in sys.argv

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
by_label = {a.get_actor_label(): a for a in actors_sub.get_all_level_actors()}


def place_nav_bounds():
    actor = by_label.get(NAV_LABEL)
    if actor:
        print("REUSED %s" % NAV_LABEL)
        return
    # spawn_actor_from_class leaves a volume with an empty brush that bounds
    # nothing; spawning from the class object goes through the box-volume
    # factory, the same as dragging one in from the Place panel.
    actor = actors_sub.spawn_actor_from_object(unreal.NavMeshBoundsVolume.static_class(), NAV_CENTER)
    actor.set_actor_scale3d(NAV_SCALE)
    actor.set_actor_label(NAV_LABEL)
    print("SPAWNED %s" % NAV_LABEL)


def place_patrol_points():
    points = []
    for label, loc in PATROL:
        actor = by_label.get(label)
        if actor:
            print("REUSED %s" % label)
        else:
            actor = actors_sub.spawn_actor_from_class(unreal.TargetPoint, loc, unreal.Rotator(0.0, 0.0, 0.0))
            actor.set_actor_label(label)
            print("SPAWNED %s" % label)
        points.append(actor)
    return points


def place_npc(points):
    actor = by_label.get(NPC_LABEL)
    if actor:
        print("REUSED %s" % NPC_LABEL)
    else:
        npc_class = unreal.EditorAssetLibrary.load_blueprint_class(NPC_BP)
        # Start on the first point, standing on the floor (capsule half height 92).
        start = points[0].get_actor_location() + unreal.Vector(0.0, 0.0, 100.0)
        actor = actors_sub.spawn_actor_from_class(npc_class, start, unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label(NPC_LABEL)
        print("SPAWNED %s" % NPC_LABEL)
    actor.set_editor_property("patrol_points", points)
    actor.set_editor_property("pause_at_point", PAUSE_AT_POINT)


place_nav_bounds()
points = place_patrol_points()
place_npc(points)
level_sub.save_current_level()
print("SAVED %s" % MAP)

# --------------------------------------------------------------------------
# NavMesh: build, check the route, save
# --------------------------------------------------------------------------
#
# A map loaded at startup sits under the navigation AsyncLoadLock, which drops
# rebuild requests and does not rebuild when it lifts. So wait for the lock,
# ask for a rebuild, and only call it done when every leg of the patrol has a
# real path.

nav_state = {"frames": 0, "asked_at": None, "done": False}
LEGS = [(PATROL[i][1], PATROL[(i + 1) % len(PATROL)][1]) for i in range(len(PATROL))]


def nav_tick(_dt):
    if nav_state["done"]:
        return
    nav_state["frames"] += 1
    world = editor_sub.get_editor_world()
    if nav_state["frames"] > 6000:
        finish("WARN NavMesh not ready after 6000 frames -- check %s covers the floor" % NAV_LABEL)
        return
    if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(world):
        return
    if nav_state["asked_at"] is None or nav_state["frames"] - nav_state["asked_at"] > 600:
        nav_state["asked_at"] = nav_state["frames"]
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        return
    lengths = []
    for a, b in LEGS:
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b)
        if not (path and path.is_valid() and len(path.path_points) >= 2):
            return
        lengths.append(path.get_path_length())
    level_sub.save_current_level()
    finish("NAV ready, level saved. Leg lengths (cm): %s"
           % ", ".join("%.0f" % l for l in lengths))


def finish(message):
    nav_state["done"] = True
    print(message)
    unreal.unregister_slate_post_tick_callback(nav_handle)
    if QUIT:
        unreal.SystemLibrary.quit_editor()


nav_handle = unreal.register_slate_post_tick_callback(nav_tick)
