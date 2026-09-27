"""Unreal Editor Python — Month 1 asset and greybox setup.

Run from inside the editor (Tools > Execute Python Script):

    D:/DEADLINE_/Content/Deadline/Core/setup_month1.py

or headless, with the editor closed:

    UnrealEditor-Cmd.exe D:\\DEADLINE_\\DEADLINE_.uproject -run=pythonscript
        -script="D:/DEADLINE_/Content/Deadline/Core/setup_month1.py" -unattended

Creates, all under /Game/Deadline/:
  Core/Input/IA_Move, IA_Look, IA_Sprint, IA_Interact, IA_Drop
  Core/Input/IMC_Default          WASD + mouse + Shift + E + Q
  Core/BP_Container               AContainerActor
  Core/BP_DeadlinePlayer          ADeadlinePlayerCharacter, input assets wired
  Core/BP_DeadlineGameMode        uses BP_DeadlinePlayer
  Maps/LVL_Greybox                floor, walls, ramp, supplier, buyer, storage, light

Safe to run repeatedly: existing assets are reused, never duplicated.
Level creation needs a real editor; headless runs skip it and say so.

API notes, all confirmed against UE 5.8 rather than guessed:
  - the Enhanced Input factories are InputAction_Factory and
    InputMappingContext_Factory (with the underscore)
  - an FKey is built by default-constructing unreal.Key() and import_text("W")
  - mappings must be written with imc.map_key(); the deprecated Mappings
    array is no longer read and a direct default_key_mappings write does not
    survive the save
  - input modifiers need the IMC as their outer or they serialise as null
"""

import unreal

ROOT = "/Game/Deadline"
INPUT_DIR = ROOT + "/Core/Input"
CORE_DIR = ROOT + "/Core"
MAPS_DIR = ROOT + "/Maps"
LEVEL_PATH = MAPS_DIR + "/LVL_Greybox"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

_report = []


def log(message):
    unreal.log("[Deadline setup] {0}".format(message))
    _report.append("OK   " + str(message))


def warn(message):
    unreal.log_warning("[Deadline setup] {0}".format(message))
    _report.append("WARN " + str(message))


# --------------------------------------------------------------------------
# asset helpers
# --------------------------------------------------------------------------

def make_asset(name, package_path, asset_class, factory):
    """Create an asset, or return the one already at that path."""
    full = "{0}/{1}".format(package_path, name)
    existing = unreal.load_asset(full)
    if existing:
        log("reusing {0}".format(full))
        return existing
    created = ASSET_TOOLS.create_asset(name, package_path, asset_class, factory)
    if created:
        log("created {0}".format(full))
    else:
        warn("could not create {0}".format(full))
    return created


def save(asset):
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def make_key(key_name):
    """Build an FKey from its name, e.g. "W", "LeftShift", "Mouse2D"."""
    key = unreal.Key()
    if not key.import_text(key_name):
        warn("unknown key '{0}'".format(key_name))
    return key


# --------------------------------------------------------------------------
# input assets
# --------------------------------------------------------------------------

def make_input_action(name, value_type):
    action = make_asset(name, INPUT_DIR, None, unreal.InputAction_Factory())
    if action:
        action.set_editor_property("value_type", value_type)
        save(action)
    return action


def swizzle_yxz(imc):
    """Send the key's X value into the Y channel, so W/S drive forward/back.

    Modifiers must be created with the mapping context as their outer, or they
    serialise as null and the mapping loads back with empty modifiers."""
    mod = unreal.new_object(unreal.InputModifierSwizzleAxis, outer=imc)
    mod.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return mod


def negate(imc):
    return unreal.new_object(unreal.InputModifierNegate, outer=imc)


def create_input():
    boolean = unreal.InputActionValueType.BOOLEAN
    axis2d = unreal.InputActionValueType.AXIS2D

    actions = {
        "IA_Move": make_input_action("IA_Move", axis2d),
        "IA_Look": make_input_action("IA_Look", axis2d),
        "IA_Sprint": make_input_action("IA_Sprint", boolean),
        "IA_Interact": make_input_action("IA_Interact", boolean),
        "IA_Drop": make_input_action("IA_Drop", boolean),
    }
    if not all(actions.values()):
        warn("some input actions are missing; create them by hand and re-run")
        return actions, None

    imc = make_asset("IMC_Default", INPUT_DIR, None, unreal.InputMappingContext_Factory())
    if not imc:
        return actions, None

    # X is strafe, Y is forward — matches ADeadlinePlayerCharacter::Input_Move.
    # GDD 3.3 key layout for the buttons.
    spec = [
        ("IA_Move", "W", [swizzle_yxz(imc)]),
        ("IA_Move", "S", [swizzle_yxz(imc), negate(imc)]),
        ("IA_Move", "D", []),
        ("IA_Move", "A", [negate(imc)]),
        ("IA_Look", "Mouse2D", []),
        ("IA_Sprint", "LeftShift", []),
        ("IA_Interact", "E", []),
        ("IA_Drop", "Q", []),
    ]

    # Use the engine mutators rather than writing the mappings array directly.
    # Setting default_key_mappings from Python looks right in memory but does
    # not survive the save, and the old Mappings array is deprecated and no
    # longer read (UE 5.7+), so either shortcut produces a context that applies
    # with zero mappings and no input at all.
    imc.unmap_all()
    for action_name, key_name, _ in spec:
        imc.map_key(actions[action_name], make_key(key_name))

    # map_key cannot carry modifiers, so attach them to the entries it made.
    data = imc.get_editor_property("default_key_mappings")
    entries = list(data.get_editor_property("mappings"))
    for entry, (_, _, modifiers) in zip(entries, spec):
        if modifiers:
            entry.set_editor_property("modifiers", modifiers)
    data.set_editor_property("mappings", entries)
    imc.set_editor_property("default_key_mappings", data)

    unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
    log("IMC_Default wired with {0} mappings".format(len(spec)))
    return actions, imc


# --------------------------------------------------------------------------
# blueprints
# --------------------------------------------------------------------------

def make_blueprint(name, package_path, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return make_asset(name, package_path, None, factory)


def create_blueprints(actions, imc):
    bp_container = make_blueprint("BP_Container", CORE_DIR, unreal.ContainerActor)
    save(bp_container)

    bp_player = make_blueprint("BP_DeadlinePlayer", CORE_DIR, unreal.DeadlinePlayerCharacter)
    if bp_player and imc:
        cdo = unreal.get_default_object(bp_player.generated_class())
        cdo.set_editor_property("default_mapping_context", imc)
        cdo.set_editor_property("move_action", actions["IA_Move"])
        cdo.set_editor_property("look_action", actions["IA_Look"])
        cdo.set_editor_property("sprint_action", actions["IA_Sprint"])
        cdo.set_editor_property("interact_action", actions["IA_Interact"])
        cdo.set_editor_property("drop_action", actions["IA_Drop"])
        save(bp_player)
        log("BP_DeadlinePlayer wired to the input assets")

    bp_gm = make_blueprint("BP_DeadlineGameMode", CORE_DIR, unreal.DeadlineGameMode)
    if bp_gm and bp_player:
        cdo = unreal.get_default_object(bp_gm.generated_class())
        cdo.set_editor_property("default_pawn_class", bp_player.generated_class())
        save(bp_gm)
        log("BP_DeadlineGameMode uses BP_DeadlinePlayer")

    return bp_player, bp_gm, bp_container


# --------------------------------------------------------------------------
# greybox level
# --------------------------------------------------------------------------

CUBE = "/Engine/BasicShapes/Cube.Cube"


def spawn_cube(actors, name, location_cm, scale_m, rotation=None):
    """A scaled engine cube. The cube is 1 m, so scale_m reads as metres."""
    actor = actors.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location_cm),
        unreal.Rotator(*(rotation or (0.0, 0.0, 0.0))))
    if not actor:
        return None
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(CUBE))
    actor.set_actor_scale3d(unreal.Vector(*scale_m))
    return actor


def build_level(bp_container):
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_sub or not actors:
        warn("no level editor available (headless run) - assets are done, "
             "re-run this script inside the editor to build LVL_Greybox")
        return False

    if not level_sub.new_level(LEVEL_PATH):
        warn("could not create {0}".format(LEVEL_PATH))
        return False
    log("new level {0}".format(LEVEL_PATH))

    # Warehouse shell: 30 x 20 m floor, 4 m walls.
    spawn_cube(actors, "SM_Floor", (0, 0, -50), (30.0, 20.0, 1.0))
    spawn_cube(actors, "SM_Wall_N", (0, 1000, 200), (30.0, 0.5, 4.0))
    spawn_cube(actors, "SM_Wall_S", (0, -1000, 200), (30.0, 0.5, 4.0))
    spawn_cube(actors, "SM_Wall_E", (1500, 0, 200), (0.5, 20.0, 4.0))
    spawn_cube(actors, "SM_Wall_W", (-1500, 0, 200), (0.5, 20.0, 4.0))
    spawn_cube(actors, "SM_Ramp", (1200, -700, 40), (4.0, 3.0, 0.2), (0.0, -8.0, 0.0))

    storage = actors.spawn_actor_from_class(
        unreal.StorageZoneActor, unreal.Vector(-900, 500, 100), unreal.Rotator(0, 0, 0))
    if storage:
        storage.set_actor_label("StorageZone_Rack01")
        if bp_container:
            storage.set_editor_property("container_class", bp_container.generated_class())

    supplier = actors.spawn_actor_from_class(
        unreal.TradePostActor, unreal.Vector(900, 600, 50), unreal.Rotator(0, 0, 180))
    if supplier:
        supplier.set_actor_label("TradePost_Supplier")
        supplier.set_editor_property("post_role", unreal.TradePostRole.SUPPLIER)
        supplier.set_editor_property("product_id", "S01")
        supplier.set_editor_property("post_name", unreal.Text("Dockside Supplier"))
        if bp_container:
            supplier.set_editor_property("container_class", bp_container.generated_class())

    buyer = actors.spawn_actor_from_class(
        unreal.TradePostActor, unreal.Vector(900, -600, 50), unreal.Rotator(0, 0, 180))
    if buyer:
        buyer.set_actor_label("TradePost_Buyer")
        buyer.set_editor_property("post_role", unreal.TradePostRole.BUYER)
        buyer.set_editor_property("post_name", unreal.Text("Retail Buyer"))

    start = actors.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
    if start:
        start.set_actor_label("PlayerStart")

    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(0, -50, -45))
    if sun:
        sun.set_actor_label("Sun")

    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400), unreal.Rotator(0, 0, 0))
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))

    level_sub.save_current_level()
    log("greybox level saved")
    return True


# --------------------------------------------------------------------------

def main():
    for folder in (INPUT_DIR, CORE_DIR, MAPS_DIR):
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)

    actions, imc = create_input()
    bp_player, bp_gm, bp_container = create_blueprints(actions, imc)
    build_level(bp_container)

    log("done")
    log("Next: Project Settings > Maps & Modes -> GameMode = BP_DeadlineGameMode, "
        "Editor Startup Map = LVL_Greybox")

    unreal.log("[Deadline setup] REPORT\n" + "\n".join(_report))


if __name__ == "__main__":
    main()
