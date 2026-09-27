"""Unreal Editor Python — build the forecast board (GDD 17) and bind it to N.

    UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript=<this file>
        -unattended -nosplash -nop4 -RenderOffscreen

Creates (or reuses) WBP_ForecastCard and WBP_ForecastScreen under
/Game/Deadline/UI, rebuilds both trees from scratch, sets the screen's
CardWidgetClass, and adds IA_Forecast (N) to IMC_Default and BP_DeadlinePlayer.
Safe to run repeatedly.

Needs the DEADLINE_Editor module (widget trees cannot be built from stock
Python) and the C++ classes UForecastCardWidget / UForecastScreenWidget, so
build the C++ first. Helpers mirror build_travel_screen.py; palette and fonts
follow DEADLINE_UI_Prompts.md 1.2 / 1.3.
"""

import unreal

TOOLS = unreal.DeadlineWidgetTools
UI_DIR = "/Game/Deadline/UI"
INPUT_DIR = "/Game/Deadline/Core/Input"


def rgb(hexcode, alpha=1.0):
    """#RRGGBB (sRGB, as the palette is written) to linear."""
    value = int(hexcode, 16)
    channels = [((value >> 16) & 0xFF), ((value >> 8) & 0xFF), (value & 0xFF)]
    linear = [pow(c / 255.0, 2.2) for c in channels]
    return unreal.LinearColor(linear[0], linear[1], linear[2], alpha)


GROUND = rgb("16191A")
PANEL = rgb("1E2325")
RAISED = rgb("272D30")
LINE = rgb("333B3E")
BODY = rgb("E8E4DC")
MUTED = rgb("8B938F")
ACCENT = rgb("5B8CA8")
WHITE = unreal.LinearColor(1.0, 1.0, 1.0, 1.0)
CLEAR = unreal.LinearColor(0.0, 0.0, 0.0, 0.0)

FONTS_DIR = "/Game/Deadline/UI/Fonts/"
HEADING = FONTS_DIR + "BarlowCondensed-SemiBold_Font"
LABEL = FONTS_DIR + "Inter_18pt-Regular_Font"
NUMBER = FONTS_DIR + "IBMPlexMono-Medium_Font"

FILL = unreal.SlateSizeRule.FILL
AUTO = unreal.SlateSizeRule.AUTOMATIC
H_FILL = unreal.HorizontalAlignment.H_ALIGN_FILL
H_CENTER = unreal.HorizontalAlignment.H_ALIGN_CENTER
H_RIGHT = unreal.HorizontalAlignment.H_ALIGN_RIGHT
V_FILL = unreal.VerticalAlignment.V_ALIGN_FILL
V_CENTER = unreal.VerticalAlignment.V_ALIGN_CENTER

_report = []


def log(message):
    unreal.log("[Deadline forecast-ui] {0}".format(message))
    _report.append(str(message))


# --- helpers (as in build_travel_screen.py) ---------------------------------

def brush(fill, outline=None, radius=2.0, width=1.0):
    b = unreal.SlateBrush()
    b.set_editor_property("draw_as", unreal.SlateBrushDrawType.ROUNDED_BOX)
    b.set_editor_property("tint_color", unreal.SlateColor(specified_color=fill))
    o = unreal.SlateBrushOutlineSettings()
    o.set_editor_property("rounding_type", unreal.SlateBrushRoundingType.FIXED_RADIUS)
    o.set_editor_property("corner_radii", unreal.Vector4(radius, radius, radius, radius))
    o.set_editor_property("color", unreal.SlateColor(specified_color=outline if outline else CLEAR))
    o.set_editor_property("width", width if outline else 0.0)
    b.set_editor_property("outline_settings", o)
    return b


def margin(left=0.0, top=0.0, right=0.0, bottom=0.0):
    return unreal.Margin(left=left, top=top, right=right, bottom=bottom)


def box_slot(widget, size=None, value=1.0, padding=None, h=None, v=None):
    s = widget.get_editor_property("slot")
    if size is not None:
        s.set_editor_property("size", unreal.SlateChildSize(value=value, size_rule=size))
    if padding is not None:
        s.set_editor_property("padding", padding)
    if h is not None:
        s.set_editor_property("horizontal_alignment", h)
    if v is not None:
        s.set_editor_property("vertical_alignment", v)


def align_slot(widget, padding=None, h=H_FILL, v=V_FILL):
    s = widget.get_editor_property("slot")
    if padding is not None:
        s.set_editor_property("padding", padding)
    s.set_editor_property("horizontal_alignment", h)
    s.set_editor_property("vertical_alignment", v)


def fill_canvas(widget):
    s = widget.get_editor_property("slot")
    data = unreal.AnchorData()
    data.set_editor_property("anchors", unreal.Anchors(
        minimum=unreal.Vector2D(0.0, 0.0), maximum=unreal.Vector2D(1.0, 1.0)))
    data.set_editor_property("offsets", margin())
    data.set_editor_property("alignment", unreal.Vector2D(0.0, 0.0))
    s.set_editor_property("layout_data", data)


_font_cache = {}


def text(widget, size, colour, family=LABEL, justify=None, wrap=False):
    if family not in _font_cache:
        asset = unreal.load_asset(family)
        if asset is None:
            raise RuntimeError("font missing: " + family)
        _font_cache[family] = asset
    font = widget.get_editor_property("font")
    font.set_editor_property("font_object", _font_cache[family])
    font.set_editor_property("typeface_font_name", "Default")
    font.set_editor_property("size", float(size))
    widget.set_editor_property("font", font)
    widget.set_editor_property("color_and_opacity", unreal.SlateColor(specified_color=colour))
    if justify is not None:
        widget.set_editor_property("justification", justify)
    if wrap:
        widget.set_editor_property("auto_wrap_text", True)


def button_style(button, normal, hovered, pressed, radius=2.0):
    prop = "widget_style"
    try:
        button.get_editor_property(prop)
    except Exception:
        prop = "style"
    style = button.get_editor_property(prop)
    style.set_editor_property("normal", brush(normal, radius=radius))
    style.set_editor_property("hovered", brush(hovered, radius=radius))
    style.set_editor_property("pressed", brush(pressed, radius=radius))
    style.set_editor_property("disabled", brush(rgb("222729"), radius=radius))
    button.set_editor_property(prop, style)


def add(bp, cls, name, parent):
    widget = TOOLS.add_widget(bp, cls, name, parent)
    if widget is None:
        raise RuntimeError("could not create {0}".format(name))
    return widget


def widget_blueprint(name, parent_class):
    full = "{0}/{1}".format(UI_DIR, name)
    existing = unreal.load_asset(full)
    if existing:
        log("reused " + name)
        return existing
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, UI_DIR, None, factory)
    if bp is None:
        raise RuntimeError("could not create " + name)
    log("created " + name)
    return bp


# --- WBP_ForecastCard --------------------------------------------------------

def build_card():
    bp = widget_blueprint("WBP_ForecastCard", unreal.ForecastCardWidget)
    TOOLS.clear_widget_tree(bp)

    # White tint: the card's colour is set in code with SetBrushColor
    # (panel, or raised when selected), which multiplies with the tint.
    frame = add(bp, unreal.Border, "CardFrame", None)
    frame.set_editor_property("background", brush(WHITE, LINE))
    frame.set_editor_property("brush_color", PANEL)
    frame.set_editor_property("padding", margin())

    click = add(bp, unreal.Button, "CardButton", frame)
    button_style(click, CLEAR, unreal.LinearColor(1.0, 1.0, 1.0, 0.04), unreal.LinearColor(1.0, 1.0, 1.0, 0.08))
    align_slot(click)

    column = add(bp, unreal.VerticalBox, "CardContent", click)
    align_slot(column, padding=margin(14.0, 12.0, 14.0, 12.0))

    top = add(bp, unreal.HorizontalBox, "CardTopRow", column)
    box_slot(top, size=AUTO)
    name = add(bp, unreal.TextBlock, "NameText", top)
    text(name, 19, BODY, HEADING)
    box_slot(name, size=FILL, v=V_CENTER)
    odds = add(bp, unreal.TextBlock, "ConfidenceText", top)
    text(odds, 20, BODY, NUMBER, unreal.TextJustify.RIGHT)
    box_slot(odds, size=AUTO, v=V_CENTER, padding=margin(12.0, 0.0, 0.0, 0.0))

    status = add(bp, unreal.TextBlock, "StatusText", column)
    text(status, 12, MUTED, LABEL)
    box_slot(status, size=AUTO, padding=margin(0.0, 1.0, 0.0, 6.0))

    bar_box = add(bp, unreal.SizeBox, "ConfidenceBarBox", column)
    bar_box.set_height_override(4.0)
    box_slot(bar_box, size=AUTO, padding=margin(0.0, 0.0, 0.0, 8.0))
    bar = add(bp, unreal.ProgressBar, "ConfidenceBar", bar_box)
    style = bar.get_editor_property("widget_style")
    style.set_editor_property("background_image", brush(LINE, radius=2.0))
    style.set_editor_property("fill_image", brush(WHITE, radius=2.0))
    bar.set_editor_property("widget_style", style)
    align_slot(bar)

    body = add(bp, unreal.TextBlock, "BodyText", column)
    text(body, 13, BODY, LABEL, wrap=True)
    box_slot(body, size=AUTO, padding=margin(0.0, 0.0, 0.0, 8.0))

    bottom = add(bp, unreal.HorizontalBox, "CardBottomRow", column)
    box_slot(bottom, size=AUTO)
    impact = add(bp, unreal.TextBlock, "ImpactText", bottom)
    text(impact, 12, MUTED, LABEL)
    box_slot(impact, size=FILL, v=V_CENTER)
    exposure = add(bp, unreal.TextBlock, "ExposureText", bottom)
    text(exposure, 12, BODY, NUMBER, unreal.TextJustify.RIGHT)
    box_slot(exposure, size=AUTO, v=V_CENTER, padding=margin(12.0, 0.0, 0.0, 0.0))

    TOOLS.compile_and_save(bp)
    log("WBP_ForecastCard: {0} widgets".format(len(TOOLS.get_widget_names(bp))))
    return bp


# --- WBP_ForecastScreen ------------------------------------------------------

def build_screen(card_bp):
    bp = widget_blueprint("WBP_ForecastScreen", unreal.ForecastScreenWidget)
    TOOLS.clear_widget_tree(bp)

    canvas = add(bp, unreal.CanvasPanel, "RootCanvas", None)
    screen = add(bp, unreal.Border, "ScreenBorder", canvas)
    screen.set_editor_property("background", brush(GROUND))
    screen.set_editor_property("padding", margin(18.0, 18.0, 18.0, 18.0))
    fill_canvas(screen)

    main = add(bp, unreal.VerticalBox, "MainColumn", screen)
    align_slot(main)

    # ---- header ----------------------------------------------------------
    header = add(bp, unreal.Border, "Header", main)
    header.set_editor_property("background", brush(PANEL, LINE))
    header.set_editor_property("padding", margin(14.0, 8.0, 14.0, 8.0))
    box_slot(header, size=AUTO, padding=margin(0.0, 0.0, 0.0, 12.0))
    header_row = add(bp, unreal.HorizontalBox, "HeaderRow", header)
    align_slot(header_row)

    title = add(bp, unreal.TextBlock, "TitleText", header_row)
    title.set_text("TAHMİN PANOSU")
    text(title, 20, BODY, HEADING)
    box_slot(title, size=AUTO, v=V_CENTER)
    day = add(bp, unreal.TextBlock, "HeaderDayText", header_row)
    text(day, 14, MUTED, NUMBER)
    box_slot(day, size=AUTO, v=V_CENTER, padding=margin(18.0, 0.0, 0.0, 0.0))
    summary = add(bp, unreal.TextBlock, "SummaryText", header_row)
    text(summary, 13, MUTED, LABEL, unreal.TextJustify.RIGHT)
    box_slot(summary, size=FILL, v=V_CENTER, padding=margin(18.0, 0.0, 18.0, 0.0))

    close_box = add(bp, unreal.SizeBox, "CloseBox", header_row)
    close_box.set_width_override(26.0)
    close_box.set_height_override(26.0)
    box_slot(close_box, size=AUTO, h=H_RIGHT, v=V_CENTER)
    close = add(bp, unreal.Button, "CloseButton", close_box)
    button_style(close, PANEL, LINE, rgb("444D51"))
    align_slot(close)
    close_label = add(bp, unreal.TextBlock, "CloseLabelText", close)
    close_label.set_text("X")
    text(close_label, 12, MUTED, LABEL, unreal.TextJustify.CENTER)
    align_slot(close_label, h=H_CENTER, v=V_CENTER)

    # ---- body: cards left, detail right ----------------------------------
    body = add(bp, unreal.HorizontalBox, "BodyRow", main)
    box_slot(body, size=FILL)

    left = add(bp, unreal.VerticalBox, "LeftColumn", body)
    box_slot(left, size=FILL, value=0.42, padding=margin(0.0, 0.0, 14.0, 0.0))

    empty = add(bp, unreal.TextBlock, "EmptyText", left)
    empty.set_text("Bugün kulağına gelen bir söylenti yok.")
    text(empty, 14, MUTED, LABEL, wrap=True)
    box_slot(empty, size=AUTO, padding=margin(4.0, 4.0, 4.0, 12.0))

    scroll = add(bp, unreal.ScrollBox, "CardScroll", left)
    box_slot(scroll, size=FILL)
    cards = add(bp, unreal.VerticalBox, "CardList", scroll)

    detail_frame = add(bp, unreal.Border, "DetailFrame", body)
    detail_frame.set_editor_property("background", brush(PANEL, LINE))
    detail_frame.set_editor_property("padding", margin(24.0, 20.0, 24.0, 20.0))
    box_slot(detail_frame, size=FILL, value=0.58)

    detail = add(bp, unreal.VerticalBox, "DetailColumn", detail_frame)
    align_slot(detail)

    status = add(bp, unreal.TextBlock, "DetailStatusText", detail)
    text(status, 12, MUTED, LABEL)
    box_slot(status, size=AUTO)
    dtitle = add(bp, unreal.TextBlock, "DetailTitleText", detail)
    text(dtitle, 32, BODY, HEADING)
    box_slot(dtitle, size=AUTO, padding=margin(0.0, 2.0, 0.0, 4.0))
    odds = add(bp, unreal.TextBlock, "DetailOddsText", detail)
    text(odds, 22, BODY, NUMBER)
    box_slot(odds, size=AUTO, padding=margin(0.0, 0.0, 0.0, 12.0))
    dbody = add(bp, unreal.TextBlock, "DetailBodyText", detail)
    text(dbody, 15, BODY, LABEL, wrap=True)
    box_slot(dbody, size=AUTO, padding=margin(0.0, 0.0, 0.0, 14.0))

    rule = add(bp, unreal.Border, "DetailRule", detail)
    rule.set_editor_property("background", brush(LINE, radius=0.0))
    rule.set_editor_property("padding", margin(0.0, 0.5, 0.0, 0.5))
    box_slot(rule, size=AUTO, padding=margin(0.0, 0.0, 0.0, 14.0))

    stake = add(bp, unreal.TextBlock, "DetailStakeText", detail)
    text(stake, 16, BODY, LABEL, wrap=True)
    box_slot(stake, size=AUTO, padding=margin(0.0, 0.0, 0.0, 16.0))

    products_label = add(bp, unreal.TextBlock, "DetailProductsLabel", detail)
    products_label.set_text("ETKİLENEN ÜRÜNLER")
    text(products_label, 13, MUTED, HEADING)
    box_slot(products_label, size=AUTO, padding=margin(0.0, 0.0, 0.0, 6.0))
    products = add(bp, unreal.VerticalBox, "DetailProductList", detail)
    box_slot(products, size=AUTO)

    spacer = add(bp, unreal.Spacer, "DetailSpacer", detail)
    box_slot(spacer, size=FILL)

    hint_frame = add(bp, unreal.Border, "DetailHintFrame", detail)
    hint_frame.set_editor_property("background", brush(RAISED))
    hint_frame.set_editor_property("padding", margin(14.0, 10.0, 14.0, 10.0))
    box_slot(hint_frame, size=AUTO, padding=margin(0.0, 14.0, 0.0, 0.0))
    hint = add(bp, unreal.TextBlock, "DetailHintText", hint_frame)
    text(hint, 14, BODY, LABEL, wrap=True)
    align_slot(hint)

    TOOLS.compile_and_save(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("card_widget_class", card_bp.generated_class())
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    log("WBP_ForecastScreen: {0} widgets, CardWidgetClass set".format(len(TOOLS.get_widget_names(bp))))


# --- input: N -------------------------------------------------------------------

def make_key(key_name):
    key = unreal.Key()
    key.import_text(key_name)
    return key


def bind_n_key():
    full = INPUT_DIR + "/IA_Forecast"
    action = unreal.load_asset(full)
    if action is None:
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "IA_Forecast", INPUT_DIR, None, unreal.InputAction_Factory())
        log("created IA_Forecast")
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False)

    imc = unreal.load_asset(INPUT_DIR + "/IMC_Default")
    if imc is None:
        log("WARN IMC_Default missing; run setup_month1.py first")
        return
    # Add, never rebuild: unmap_all() would drop WASD (HATA_GUNLUGU H007).
    data = imc.get_editor_property("default_key_mappings")
    if any(e.get_editor_property("action") == action for e in data.get_editor_property("mappings")):
        log("IA_Forecast already mapped")
    else:
        imc.map_key(action, make_key("N"))
        unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Default", only_if_is_dirty=False)
        log("IA_Forecast -> N")

    player = unreal.load_asset("/Game/Deadline/Core/BP_DeadlinePlayer")
    if player is None:
        log("WARN BP_DeadlinePlayer missing")
        return
    cdo = unreal.get_default_object(player.generated_class())
    cdo.set_editor_property("forecast_action", action)
    unreal.EditorAssetLibrary.save_loaded_asset(player, only_if_is_dirty=False)
    log("BP_DeadlinePlayer.ForecastAction -> IA_Forecast")


def main():
    card = build_card()
    build_screen(card)
    bind_n_key()
    unreal.log("[Deadline forecast-ui] ---- done ----")
    for line in _report:
        unreal.log("[Deadline forecast-ui] " + line)


main()
