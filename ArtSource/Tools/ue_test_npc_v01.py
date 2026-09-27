# In-game test of BP_NPC_V01: ADeadlineNPCController patrols two points on a NavMesh with a
# wall between them, so a straight line is impossible and the NPC has to path around it.
# Logs arrivals, the detour and gait data; writes scene-capture PNGs to Saved/NPCTest.
# Run with -ExecCmds (NOT -ExecutePythonScript, which quits a few frames after the script returns):
#   UnrealEditor-Cmd.exe DEADLINE_.uproject "-ExecCmds=py <this file>" -unattended -nosplash -nop4 -RenderOffscreen
import math
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT_DIR = os.path.join(PROJECT, "Saved", "NPCTest")
LEVEL = "/Game/Deadline/Maps/Test/LVL_Test_NPC_V01"
NPC_BP = "/Game/Deadline/Characters/NPC/V01/BP_NPC_V01"
POINT_A, POINT_B = unreal.Vector(-1200, 0, 0), unreal.Vector(1200, 0, 0)
START = unreal.Vector(-1500, 300, 100)
WALL_HALF_WIDTH = 400  # the wall spans y = -400..400 at x = 0
DURATION = 30.0
CAPTURES = [3.0, 7.0, 11.0, 18.0]

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
os.makedirs(OUT_DIR, exist_ok=True)
for f in os.listdir(OUT_DIR):
    if f.endswith(".png"):
        os.remove(os.path.join(OUT_DIR, f))


def log(msg):
    unreal.log_warning("NPCTEST " + msg)


# --- 1. Test level ---------------------------------------------------------------
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        # keep WorldSettings and the NavigationData actors the nav system owns
        if not isinstance(a, (unreal.WorldSettings, unreal.NavigationData)):
            eas.destroy_actor(a)
else:
    les.new_level(LEVEL)
world = ues.get_editor_world()
# Plain GameModeBase: keeps the project's own game mode (economy, HUD, saves) out of the test.
world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)

floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(60, 60, 1))
wall = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 100))
wall.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
wall.set_actor_scale3d(unreal.Vector(0.5, WALL_HALF_WIDTH * 2 / 100, 2))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -50, 30))
sun.light_component.set_intensity(8)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
# spawn_actor_from_class leaves a volume with an empty brush that bounds nothing; spawning from
# the class object goes through the editor's box-volume factory, like a drag from the Place panel.
bounds = eas.spawn_actor_from_object(unreal.NavMeshBoundsVolume.static_class(), unreal.Vector(0, 0, 100))
bounds.set_actor_scale3d(unreal.Vector(25, 25, 3))  # 200 cm brush -> 50 m x 50 m x 6 m
_origin, _extent = bounds.get_actor_bounds(False)
log(f"nav bounds extent={_extent}")

point_a = eas.spawn_actor_from_class(unreal.TargetPoint, POINT_A)
point_b = eas.spawn_actor_from_class(unreal.TargetPoint, POINT_B)
point_a.set_actor_label("Patrol_A")
point_b.set_actor_label("Patrol_B")
npc_class = unreal.EditorAssetLibrary.load_blueprint_class(NPC_BP)
npc = eas.spawn_actor_from_class(npc_class, START)
npc.set_editor_property("patrol_points", [point_a, point_b])
npc.set_editor_property("pause_at_point", 1.5)
# Python cannot spawn into the PIE world, so the capture camera is placed here and found in PIE.
eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, -2300, 1500), unreal.Rotator(0, -32, 90))
log("level built, waiting for navigation")

# --- 2. Wait for the NavMesh, then PIE ------------------------------------------------
state = {"stage": "nav", "frames": 0, "t0": None, "npc": None, "cap": None, "rt": None,
         "caps": set(), "trail": [], "arrivals": [], "near": None, "feet": [], "ended": False}


def tick(dt):
    try:
        _tick()
    except Exception as e:  # never leave PIE running on an error
        log(f"ERROR {e!r}")
        finish()


def _tick():
    if state["ended"]:
        return
    state["frames"] += 1
    if state["stage"] == "nav":
        # "not being built" is also true before the build has started, so wait for a real path.
        ew = ues.get_editor_world()
        path = None
        # The map loads under AsyncLoadLock, which swallows rebuild requests and does not rebuild
        # when it lifts: ask only once the lock is gone, and again if nothing came of it.
        if unreal.NavigationSystemV1.is_navigation_being_built_or_locked(ew):
            if state["frames"] > 3000:
                log("ERROR navigation stayed locked")
                finish()
            return
        if state.get("rebuild_at") is None or state["frames"] - state["rebuild_at"] > 600:
            state["rebuild_at"] = state["frames"]
            unreal.SystemLibrary.execute_console_command(ew, "RebuildNavigation")
            log(f"RebuildNavigation requested at frame {state['frames']}")
            return
        if not unreal.NavigationSystemV1.is_navigation_being_built(ew):
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(ew, POINT_A, POINT_B)
        if not (path and path.is_valid() and len(path.path_points) > 2):
            if state["frames"] % 300 == 0:
                navs = unreal.GameplayStatics.get_all_actors_of_class(ew, unreal.RecastNavMesh)
                proj = unreal.NavigationSystemV1.project_point_to_navigation(ew, POINT_A, None, None, unreal.Vector(200, 200, 300))
                log(f"DIAG frame {state['frames']}: building={unreal.NavigationSystemV1.is_navigation_being_built(ew)} "
                    f"recast={[n.get_name() for n in navs]} projectA={proj} "
                    f"path={'none' if path is None else (path.is_valid(), len(path.path_points))}")
            if state["frames"] > 3000:
                log("ERROR no NavMesh path from A to B")
                finish()
            return
        state["stage"] = "pie"  # set first: begin_play can tick this callback re-entrantly
        les.save_current_level()
        log(f"navigation ready after {state['frames']} frames: A->B path has {len(path.path_points)} points, "
            f"length {path.get_path_length():.0f} cm (straight line {POINT_B.x - POINT_A.x:.0f})")
        les.editor_request_begin_play()
        return

    gw = ues.get_game_world()
    if gw is None:
        return
    if state["npc"] is None:
        found = unreal.GameplayStatics.get_all_actors_of_class(gw, npc_class)
        if not found or found[0].mesh.get_anim_instance() is None:
            return
        n = state["npc"] = found[0]
        rt = unreal.RenderingLibrary.create_render_target2d(gw, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        cap = unreal.GameplayStatics.get_all_actors_of_class(gw, unreal.SceneCapture2D)[0]
        comp = cap.capture_component2d
        comp.set_editor_property("texture_target", rt)
        comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property("capture_every_frame", False)
        comp.set_editor_property("fov_angle", 75.0)
        state["cap"], state["rt"] = cap, rt
        state["t0"] = unreal.GameplayStatics.get_time_seconds(gw)
        log(f"npc={n.get_name()} controller={n.get_controller().get_class().get_name()} "
            f"anim={n.mesh.get_anim_instance().get_class().get_name()} "
            f"points={[p.get_name() for p in n.get_editor_property('patrol_points')]}")
        return

    n = state["npc"]
    t = unreal.GameplayStatics.get_time_seconds(gw) - state["t0"]
    loc = n.get_actor_location()
    speed = n.get_velocity().length()
    state["trail"].append((t, loc.x, loc.y, speed))
    if speed > 150:
        base = loc.z - 92
        state["feet"].append(max(n.mesh.get_socket_location("foot_l").z, n.mesh.get_socket_location("foot_r").z) - base)
    for name, p in (("A", POINT_A), ("B", POINT_B)):
        close = math.hypot(loc.x - p.x, loc.y - p.y) < 100
        if close and state["near"] != name:
            state["arrivals"].append((name, round(t, 1)))
            log(f"arrived {name} at t={t:.1f}")
        if close:
            state["near"] = name
    for c in CAPTURES:
        if t >= c and c not in state["caps"]:
            state["caps"].add(c)
            state["cap"].capture_component2d.capture_scene()
            unreal.RenderingLibrary.export_render_target(gw, state["rt"], OUT_DIR, f"nav_{c:04.1f}.png")
    if t >= DURATION:
        finish()


def finish():
    if state["ended"]:
        return
    state["ended"] = True
    trail = state["trail"]
    if trail:
        crossings = [(t, y) for (t, x, y, s) in trail if abs(x) < 60]
        moving = [s for (_, _, _, s) in trail if s > 10]
        log(f"arrivals={state['arrivals']}")
        log(f"wall crossings: {len(crossings)} samples, min |y| at x~0 = "
            f"{min((abs(y) for _, y in crossings), default=float('nan')):.0f} cm (wall half-width {WALL_HALF_WIDTH})")
        log(f"speed while moving: {min(moving, default=0):.0f}..{max(moving, default=0):.0f} cm/s, "
            f"moving {len(moving) / len(trail):.0%} of the time")
        log(f"highest foot while walking: {max(state['feet'], default=0):.0f} cm above floor")
    unreal.unregister_slate_post_tick_callback(handle)
    if ues.get_game_world():
        les.editor_request_end_play()
    log("DONE")
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
