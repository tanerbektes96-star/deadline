# Month 3 greybox: one actor per GDD 11 storage class, next to the existing rack.
# Positions are placeholders -- move them in the editor, the class is what matters.
import unreal

MAP = "/Game/Deadline/Maps/LVL_Greybox"
unreal.EditorLoadingAndSavingUtils.load_map(MAP)

actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

existing = {}
for a in actors_sub.get_all_level_actors():
    if a.get_class().get_name() == "StorageZoneActor":
        existing[a.get_actor_label()] = a

# label -> (class, location, columns, levels, spacing, origin)
ZONES = [
    # label, class, location, columns, levels, spacing (across, up|back), nudge
    # Spacing is the container's own size plus a gap. Set it exactly to the box
    # width and the row renders as one unbroken slab -- you cannot tell four
    # boxes from one wall. Column and level counts are whatever then fits
    # inside the zone footprint.
    ("StorageZone_Rack01",   unreal.StorageClass.RACK,
     unreal.Vector(-900.0,  500.0, 100.0), 3, 3, unreal.Vector2D(72.0, 70.0),
     unreal.Vector(0.0, 0.0, 0.0)),
    ("StorageZone_PalletBay", unreal.StorageClass.PALLET_BAY,
     unreal.Vector(-900.0,  180.0,   8.0), 2, 2, unreal.Vector2D(140.0, 100.0),
     unreal.Vector(0.0, 0.0, 0.0)),
    ("StorageZone_ColdZone",  unreal.StorageClass.COLD_ZONE,
     unreal.Vector(-900.0, -140.0, 110.0), 4, 4, unreal.Vector2D(50.0, 50.0),
     unreal.Vector(0.0, 0.0, 0.0)),
    ("StorageZone_SecureRack", unreal.StorageClass.SECURE_RACK,
     unreal.Vector(-900.0, -420.0, 100.0), 4, 5, unreal.Vector2D(38.0, 40.0),
     unreal.Vector(0.0, 0.0, 0.0)),
]

for label, storage_class, loc, cols, levels, spacing, origin in ZONES:
    actor = existing.get(label)
    if actor is None:
        actor = actors_sub.spawn_actor_from_class(
            unreal.StorageZoneActor, loc, unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label(label)
        print("SPAWNED %s" % label)
    else:
        print("REUSED %s" % label)

    actor.set_editor_property("storage_class", storage_class)
    actor.set_editor_property("slot_columns", cols)
    actor.set_editor_property("slot_levels", levels)
    actor.set_editor_property("slot_spacing", spacing)
    actor.set_editor_property("slot_origin", origin)
    actor.set_editor_property("display_start_slot", 0)

# --------------------------------------------------------------------------
# input: F loads a box into the zone you are standing in (GDD 3.3)
# --------------------------------------------------------------------------

INPUT_DIR = "/Game/Deadline/Core/Input"


def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        print("WARN unknown key %s" % name)
    return key


def remap_market_to_b():
    """GDD 3.3 gives M to the map. Month 2 had put the market screen there
    because the table lists no market key at all; the travel map needs M back,
    so the market moves to B and the GDD gains the row it was missing."""
    imc = unreal.load_asset(INPUT_DIR + "/IMC_Default")
    market = unreal.load_asset(INPUT_DIR + "/IA_Market")
    if imc is None or market is None:
        print("WARN IMC_Default or IA_Market missing")
        return

    data = imc.get_editor_property("default_key_mappings")
    for entry in data.get_editor_property("mappings"):
        if entry.get_editor_property("action") != market:
            continue
        key = entry.get_editor_property("key")
        if key.export_text() == "B":
            print("REUSED IA_Market on B")
            return
        # Engine mutators, not a direct write to default_key_mappings:
        # a hand-written mappings array does not survive the save (H007).
        imc.unmap_key(market, key)
        imc.map_key(market, make_key("B"))
        unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
        print("SPAWNED IA_Market moved %s -> B (M reserved for the map)" % key.export_text())
        return
    print("WARN IA_Market has no mapping to move")


def create_load_input():
    full = INPUT_DIR + "/IA_Load"
    action = unreal.load_asset(full)
    if action is None:
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "IA_Load", INPUT_DIR, None, unreal.InputAction_Factory())
        print("SPAWNED IA_Load")
    else:
        print("REUSED IA_Load")
    if action is None:
        return None

    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False)

    imc = unreal.load_asset(INPUT_DIR + "/IMC_Default")
    if imc is None:
        print("WARN IMC_Default missing; run setup_month1.py first")
        return action

    # Add, never rebuild: unmap_all() here would drop WASD (HATA_GUNLUGU H007).
    data = imc.get_editor_property("default_key_mappings")
    for entry in data.get_editor_property("mappings"):
        if entry.get_editor_property("action") == action:
            print("REUSED IA_Load mapping")
            return action

    imc.map_key(action, make_key("F"))
    unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
    print("SPAWNED IA_Load -> F")
    return action


def create_input(name, key):
    """One boolean input action bound to one key, added to IMC_Default."""
    full = INPUT_DIR + "/" + name
    action = unreal.load_asset(full)
    if action is None:
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, INPUT_DIR, None, unreal.InputAction_Factory())
        print("SPAWNED %s" % name)
    else:
        print("REUSED %s" % name)
    if action is None:
        return None

    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False)

    imc = unreal.load_asset(INPUT_DIR + "/IMC_Default")
    if imc is None:
        print("WARN IMC_Default missing; run setup_month1.py first")
        return action

    # Add, never rebuild: unmap_all() here would drop WASD (HATA_GUNLUGU H007).
    data = imc.get_editor_property("default_key_mappings")
    for entry in data.get_editor_property("mappings"):
        if entry.get_editor_property("action") == action:
            print("REUSED %s mapping" % name)
            return action

    imc.map_key(action, make_key(key))
    unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
    print("SPAWNED %s -> %s" % (name, key))
    return action


def wire_player(load_action, map_action):
    bp = unreal.load_asset("/Game/Deadline/Core/BP_DeadlinePlayer")
    if bp is None:
        print("WARN BP_DeadlinePlayer missing")
        return
    cdo = unreal.get_default_object(bp.generated_class())
    if load_action is not None:
        cdo.set_editor_property("load_action", load_action)
        print("SAVED BP_DeadlinePlayer.LoadAction -> IA_Load")
    if map_action is not None:
        cdo.set_editor_property("map_action", map_action)
        print("SAVED BP_DeadlinePlayer.MapAction -> IA_Map")
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)


# --------------------------------------------------------------------------
# the starting truck (GDD 9.5 V01)
# --------------------------------------------------------------------------

VEHICLES = [
    ("Vehicle_Van01", "V01", unreal.Vector(200.0, -820.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)),
]


def place_vehicles():
    existing = {}
    for a in actors_sub.get_all_level_actors():
        if a.get_class().get_name() == "VehicleActor":
            existing[a.get_actor_label()] = a

    for label, vehicle_id, loc, rot in VEHICLES:
        actor = existing.get(label)
        if actor is None:
            actor = actors_sub.spawn_actor_from_class(unreal.VehicleActor, loc, rot)
            actor.set_actor_label(label)
            print("SPAWNED %s" % label)
        else:
            print("REUSED %s" % label)
        actor.set_editor_property("vehicle_key", unreal.Name(label))
        actor.set_editor_property("vehicle_id", unreal.Name(vehicle_id))
        # Wider than a Box L (60 cm) so the load reads as boxes, not a slab.
        actor.set_editor_property("slot_spacing", unreal.Vector(72.0, 72.0, 70.0))


# --------------------------------------------------------------------------
# the pallet jack (GDD 5.2)
# --------------------------------------------------------------------------
#
# Parked beside the pallet bay, which is the only place it has anything to do.
# A person cannot lift a loaded pallet; this is what makes one movable, and
# there is exactly one of them in the warehouse on purpose -- if you left it at
# the truck you walk back for it.

JACK_LABEL = "PalletJack01"
# The pallet bay pad is 2.8 x 2.0 m centred on (-900, 180). Park the jack just
# off its open edge, where you walk up to it, rather than out in the aisle.
JACK_LOCATION = unreal.Vector(-640.0, 180.0, 0.0)
JACK_ROTATION = unreal.Rotator(0.0, 180.0, 0.0)


def place_jack_dock():
    """A painted square on the floor where the jack lives.

    Asked for directly: a tool with no home is a tool you spend the game
    looking for. It is scenery -- no collision, no interaction -- because the
    jack does not need a dock to work, the player needs one to find it.
    """
    label = "PalletJackDock"
    for a in actors_sub.get_all_level_actors():
        if a.get_actor_label() == label:
            a.set_actor_location(JACK_LOCATION, False, False)
            print("MOVED %s" % label)
            return

    pad = actors_sub.spawn_actor_from_class(
        unreal.StaticMeshActor, JACK_LOCATION, unreal.Rotator(0.0, 0.0, 0.0))
    pad.set_actor_label(label)
    comp = pad.static_mesh_component
    comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    # 2.2 x 1.6 m, one centimetre proud of the floor.
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    pad.set_actor_scale3d(unreal.Vector(2.2, 1.6, 0.01))
    pad.set_actor_location(
        unreal.Vector(JACK_LOCATION.x, JACK_LOCATION.y, 0.5), False, False)
    print("SPAWNED %s" % label)


def place_pallet_jack():
    for a in actors_sub.get_all_level_actors():
        if a.get_class().get_name() == "PalletJackActor":
            # Move it even when it already exists: the parking spot is part of
            # the layout, and a jack left somewhere unhelpful is the bug this
            # script is meant to prevent.
            a.set_actor_location(JACK_LOCATION, False, False)
            a.set_actor_rotation(JACK_ROTATION, False)
            print("MOVED %s" % a.get_actor_label())
            return

    actor = actors_sub.spawn_actor_from_class(
        unreal.PalletJackActor, JACK_LOCATION, JACK_ROTATION)
    actor.set_actor_label(JACK_LABEL)
    print("SPAWNED %s" % JACK_LABEL)


# --------------------------------------------------------------------------
# travel points (GDD 9.2)
# --------------------------------------------------------------------------

# The greybox world is laid out to match the travel map: a destination's world
# position is its marker position scaled up, anchored on the warehouse. Nothing
# requires this -- nobody drives between them -- but it keeps the map honest
# about which way things are from each other while the city is grey.
MAP_TO_WORLD_CM = 12000.0
HOME_ANCHOR = unreal.Vector(0.0, -400.0, 0.0)

# id, label, mapx, mapy   (mirrors DT_Destinations.csv)
DESTINATIONS = [
    ("D01", "Dest_Depo",          0.231, 0.694),
    ("D02", "Dest_SanayiTedarik", 0.251, 0.305),
    ("D03", "Dest_KuzeyToptanci", 0.483, 0.137),
    ("D04", "Dest_MerkezAlici",   0.593, 0.281),
    ("D05", "Dest_DoguPerakende", 0.781, 0.233),
    ("D06", "Dest_LimanTerminal", 0.769, 0.477),
    ("D07", "Dest_GuneyRampa",    0.599, 0.683),
]


def destination_world(mapx, mapy):
    home = DESTINATIONS[0]
    return unreal.Vector(
        HOME_ANCHOR.x + (mapx - home[2]) * MAP_TO_WORLD_CM,
        HOME_ANCHOR.y + (mapy - home[3]) * MAP_TO_WORLD_CM,
        HOME_ANCHOR.z)


def place_destinations():
    existing = {}
    for a in actors_sub.get_all_level_actors():
        if a.get_class().get_name() == "DestinationActor":
            existing[a.get_actor_label()] = a

    for dest_id, label, mapx, mapy in DESTINATIONS:
        loc = destination_world(mapx, mapy)
        actor = existing.get(label)
        if actor is None:
            actor = actors_sub.spawn_actor_from_class(
                unreal.DestinationActor, loc, unreal.Rotator(0.0, 0.0, 0.0))
            actor.set_actor_label(label)
            print("SPAWNED %s (%s)" % (label, dest_id))
        else:
            actor.set_actor_location(loc, False, False)
            print("REUSED %s (%s)" % (label, dest_id))
        actor.set_editor_property("destination_id", unreal.Name(dest_id))


# Which destinations have a counter you can walk up to, and what it deals in.
# D06 Liman Terminali and D07 Güney Rampa are deliberately bare: the port import
# channel needs a licence and a container order (GDD 6.1 #4, Month 5) and the
# grey channel is cash, at night, after goal 2 (GDD 6.1 #6, Month 7). Giving
# either of them an ordinary counter now would quietly delete what makes them
# different.
DESTINATION_POSTS = [
    ("TradePost_SupplyIndustrial", "Supplier", 0.251, 0.305, "W05"),   # Sanayi Tedarik
    ("TradePost_SupplyNorth",      "Supplier", 0.483, 0.137, "S03"),   # Kuzey Toptancı
    ("TradePost_BuyerCentral",     "Buyer",    0.593, 0.281, ""),      # Merkez Alıcı
    ("TradePost_BuyerEast",        "Buyer",    0.781, 0.233, ""),      # Doğu Perakende
]


def place_destination_posts():
    existing = {a.get_actor_label(): a for a in actors_sub.get_all_level_actors()}

    for label, role, mapx, mapy, product in DESTINATION_POSTS:
        dest = destination_world(mapx, mapy)
        # Off to the side of the arrival pad, so it is not under the truck.
        loc = unreal.Vector(dest.x + 520.0, dest.y + 260.0, dest.z + 50.0)

        post = existing.get(label)
        if post is None:
            post = actors_sub.spawn_actor_from_class(
                unreal.TradePostActor, loc, unreal.Rotator(0.0, 180.0, 0.0))
            post.set_actor_label(label)
            print("SPAWNED %s" % label)
        else:
            post.set_actor_location(loc, False, False)
            print("REUSED %s" % label)

        post.set_editor_property(
            "post_role",
            unreal.TradePostRole.BUYER if role == "Buyer" else unreal.TradePostRole.SUPPLIER)
        if product:
            post.set_editor_property("product_id", unreal.Name(product))


# --------------------------------------------------------------------------
# the travel map screen (GDD 9.2 / 17)
# --------------------------------------------------------------------------

UI_DIR = "/Game/Deadline/UI"


def make_widget_blueprint(name, parent_class):
    full = "{0}/{1}".format(UI_DIR, name)
    existing = unreal.load_asset(full)
    if existing:
        print("REUSED %s" % name)
        return existing
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    wbp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, UI_DIR, None, factory)
    if wbp:
        unreal.EditorAssetLibrary.save_loaded_asset(wbp, only_if_is_dirty=False)
        print("SPAWNED %s" % name)
    else:
        print("WARN could not create %s" % name)
    return wbp


def create_travel_widgets():
    marker = make_widget_blueprint("WBP_TravelMarker", unreal.TravelMarkerWidget)
    make_widget_blueprint("WBP_TravelRoute", unreal.TravelRouteWidget)
    screen = make_widget_blueprint("WBP_TravelScreen", unreal.TravelScreenWidget)

    if screen and marker:
        # The one thing the Designer cannot infer from a widget name.
        cdo = unreal.get_default_object(screen.generated_class())
        cdo.set_editor_property("marker_widget_class", marker.generated_class())
        unreal.EditorAssetLibrary.save_loaded_asset(screen, only_if_is_dirty=False)
        print("SAVED WBP_TravelScreen.MarkerWidgetClass -> WBP_TravelMarker")


place_destinations()
place_destination_posts()
create_travel_widgets()
remap_market_to_b()
load_action = create_load_input()
map_action = create_input("IA_Map", "M")
wire_player(load_action, map_action)
place_vehicles()
place_pallet_jack()
place_jack_dock()

level_sub.save_current_level()
print("SAVED %s" % MAP)
