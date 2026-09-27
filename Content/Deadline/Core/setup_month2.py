"""Unreal Editor Python - Month 2 asset setup (market screen).

Run headless, with the editor closed:

    UnrealEditor-Cmd.exe D:\\DEADLINE_\\DEADLINE_.uproject -run=pythonscript
        -script="D:/DEADLINE_/Content/Deadline/Core/setup_month2.py"
        -unattended -nopause -nosplash -NullRHI

or from inside the editor: Tools > Execute Python Script.

Creates, all under /Game/Deadline/:
  Core/Input/IA_Market            boolean action, mapped to M in IMC_Default
  UI/WBP_MarketChart              parent UMarketChartWidget
  UI/WBP_MarketRow                parent UMarketRowWidget
  UI/WBP_MarketScreen             parent UMarketScreenWidget

and wires:
  BP_DeadlinePlayer.MarketAction  -> IA_Market
  WBP_MarketScreen.RowWidgetClass -> WBP_MarketRow

The Blueprints come out EMPTY on purpose. Laying out child widgets is Designer
work (CLAUDE.md: the agent writes the C++ base and binds by name, the user
places the widgets). The names each one needs are in
Assets/KULLANICI_GOREVLERI.md; until they exist the Blueprint will not compile,
which is the intended failure -- it names the widget you forgot.

Safe to run repeatedly: existing assets are reused, never duplicated.
The M mapping is added without touching Month 1's keys.

API notes, confirmed against UE 5.8:
  - Widget Blueprints need WidgetBlueprintFactory, not BlueprintFactory
  - a mapping is added with imc.map_key(); do NOT call unmap_all() here or
    Month 1's WASD bindings go with it
"""

import unreal

INPUT_DIR = "/Game/Deadline/Core/Input"
UI_DIR = "/Game/Deadline/UI"
CORE_DIR = "/Game/Deadline/Core"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

_report = []


def log(message):
    unreal.log("[Deadline setup2] {0}".format(message))
    _report.append("OK   " + str(message))


def warn(message):
    unreal.log_warning("[Deadline setup2] {0}".format(message))
    _report.append("WARN " + str(message))


def make_asset(name, package_path, asset_class, factory):
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
    key = unreal.Key()
    if not key.import_text(key_name):
        warn("unknown key '{0}'".format(key_name))
    return key


# --------------------------------------------------------------------------
# input
# --------------------------------------------------------------------------

def create_market_input():
    action = make_asset("IA_Market", INPUT_DIR, None, unreal.InputAction_Factory())
    if not action:
        return None
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)

    imc = unreal.load_asset(INPUT_DIR + "/IMC_Default")
    if not imc:
        warn("IMC_Default not found; run setup_month1.py first")
        return action

    # Add, do not rebuild. unmap_all() here would drop WASD.
    data = imc.get_editor_property("default_key_mappings")
    for entry in data.get_editor_property("mappings"):
        if entry.get_editor_property("action") == action:
            log("IA_Market already mapped, leaving it alone")
            return action

    imc.map_key(action, make_key("M"))
    unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
    log("mapped IA_Market to M")
    return action


# --------------------------------------------------------------------------
# widget blueprints
# --------------------------------------------------------------------------

def make_widget_blueprint(name, parent_class):
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    wbp = make_asset(name, UI_DIR, None, factory)
    save(wbp)
    return wbp


def create_widgets():
    chart = make_widget_blueprint("WBP_MarketChart", unreal.MarketChartWidget)
    row = make_widget_blueprint("WBP_MarketRow", unreal.MarketRowWidget)
    screen = make_widget_blueprint("WBP_MarketScreen", unreal.MarketScreenWidget)

    if screen and row:
        # The one thing the Designer cannot infer from a widget name.
        cdo = unreal.get_default_object(screen.generated_class())
        cdo.set_editor_property("row_widget_class", row.generated_class())
        save(screen)
        log("WBP_MarketScreen.RowWidgetClass -> WBP_MarketRow")

    return chart, row, screen


# --------------------------------------------------------------------------
# wiring
# --------------------------------------------------------------------------

def wire_player(market_action):
    bp_player = unreal.load_asset(CORE_DIR + "/BP_DeadlinePlayer")
    if not bp_player:
        warn("BP_DeadlinePlayer not found; run setup_month1.py first")
        return
    if not market_action:
        return
    cdo = unreal.get_default_object(bp_player.generated_class())
    cdo.set_editor_property("market_action", market_action)
    save(bp_player)
    log("BP_DeadlinePlayer.MarketAction -> IA_Market")


def main():
    market_action = create_market_input()
    create_widgets()
    wire_player(market_action)

    unreal.log("")
    unreal.log("[Deadline setup2] ---- summary ----")
    for line in _report:
        unreal.log("[Deadline setup2] " + line)
    unreal.log("[Deadline setup2] Next: lay the widgets out in the Designer. "
               "Exact names are in Assets/KULLANICI_GOREVLERI.md.")


if __name__ == "__main__":
    main()
