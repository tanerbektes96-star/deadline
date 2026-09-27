"""Unreal Editor Python — build the travel map screen's widget tree.

    "UnrealEditor-Cmd.exe" <uproject> -run=pythonscript \
        -script="D:/DEADLINE_/Content/Deadline/Core/build_travel_screen.py" \
        -unattended -nopause -nosplash -NullRHI

Rebuilds WBP_TravelMarker and WBP_TravelScreen from scratch every time, so the
layout lives here rather than in someone's memory of which alignment they set.
Safe to run repeatedly; it clears each tree first.

Needs the DEADLINE_Editor module (UWidgetBlueprint::WidgetTree is protected, so
stock Python cannot create a widget inside a Blueprint). Build the C++ first.

Layout follows Assets/DEADLINE_UI_Prompts.md 1.2/1.4 and the 2.14 reference:
every region carries its own 1 px outline so its extent is visible, rather than
regions being implied by spacing alone.
"""

import unreal

TOOLS = unreal.DeadlineWidgetTools
UI_DIR = "/Game/Deadline/UI"

# --- palette (DEADLINE_UI_Prompts.md 1.2) ----------------------------------

def rgb(hexcode, alpha=1.0):
    """#RRGGBB from the palette to a linear colour. Slate wants linear, the
    palette is written in sRGB, and skipping the conversion makes every panel
    noticeably lighter than the design says."""
    value = int(hexcode, 16)
    channels = [((value >> 16) & 0xFF), ((value >> 8) & 0xFF), (value & 0xFF)]
    linear = [pow(c / 255.0, 2.2) for c in channels]
    return unreal.LinearColor(linear[0], linear[1], linear[2], alpha)


GROUND = rgb("16191A")
PANEL = rgb("1E2325")
MAP_FILL = rgb("121517")
LINE = rgb("333B3E")
BODY = rgb("E8E4DC")
MUTED = rgb("8B938F")
ACCENT = rgb("5B8CA8")
ACCENT_HOVER = rgb("6E9FBA")
ACCENT_PRESS = rgb("48738C")
CLEAR = unreal.LinearColor(0.0, 0.0, 0.0, 0.0)

# --- typography (DEADLINE_UI_Prompts.md 1.3, same assets the market screen uses)
FONTS_DIR = "/Game/Deadline/UI/Fonts/"
HEADING = FONTS_DIR + "BarlowCondensed-SemiBold_Font"   # signage: names and titles
LABEL = FONTS_DIR + "Inter_18pt-Regular_Font"           # prose: words you read
NUMBER = FONTS_DIR + "IBMPlexMono-Medium_Font"          # tabular: digits that line up

FILL = unreal.SlateSizeRule.FILL
AUTO = unreal.SlateSizeRule.AUTOMATIC
H_FILL = unreal.HorizontalAlignment.H_ALIGN_FILL
H_CENTER = unreal.HorizontalAlignment.H_ALIGN_CENTER
H_RIGHT = unreal.HorizontalAlignment.H_ALIGN_RIGHT
V_FILL = unreal.VerticalAlignment.V_ALIGN_FILL
V_CENTER = unreal.VerticalAlignment.V_ALIGN_CENTER

_report = []


def log(message):
    unreal.log("[Deadline travel-ui] {0}".format(message))
    _report.append(str(message))


# --- small helpers ---------------------------------------------------------

def brush(fill, outline=None, radius=2.0, width=1.0):
    """A flat panel brush with an optional hairline outline (UI prompts 1.4:
    2 px corner radius, 1 px borders, no gradients)."""
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


def slot_of(widget):
    return widget.get_editor_property("slot")


def box_slot(widget, size=None, value=1.0, padding=None, h=None, v=None):
    """Set up a HorizontalBox / VerticalBox slot."""
    s = slot_of(widget)
    if size is not None:
        s.set_editor_property("size", unreal.SlateChildSize(value=value, size_rule=size))
    if padding is not None:
        s.set_editor_property("padding", padding)
    if h is not None:
        s.set_editor_property("horizontal_alignment", h)
    if v is not None:
        s.set_editor_property("vertical_alignment", v)


def align_slot(widget, padding=None, h=H_FILL, v=V_FILL):
    """Overlay / Border / SizeBox / Button slots: alignment and padding only."""
    s = slot_of(widget)
    if padding is not None:
        s.set_editor_property("padding", padding)
    s.set_editor_property("horizontal_alignment", h)
    s.set_editor_property("vertical_alignment", v)


def fill_canvas(widget):
    """Stretch a canvas child over its whole parent."""
    s = slot_of(widget)
    data = unreal.AnchorData()
    data.set_editor_property("anchors", unreal.Anchors(
        minimum=unreal.Vector2D(0.0, 0.0), maximum=unreal.Vector2D(1.0, 1.0)))
    data.set_editor_property("offsets", margin(0.0, 0.0, 0.0, 0.0))
    data.set_editor_property("alignment", unreal.Vector2D(0.0, 0.0))
    s.set_editor_property("layout_data", data)


_font_cache = {}


def font_asset(path):
    if path not in _font_cache:
        asset = unreal.load_asset(path)
        if asset is None:
            raise RuntimeError("font missing: " + path)
        _font_cache[path] = asset
    return _font_cache[path]


def text(widget, size, colour, family=LABEL, justify=None):
    """Numbers get the monospaced face so km, litres and clock times line up
    down the sidebar; names get the condensed one; everything else reads as
    prose (UI prompts 1.3)."""
    font = widget.get_editor_property("font")
    font.set_editor_property("font_object", font_asset(family))
    font.set_editor_property("typeface_font_name", "Default")
    font.set_editor_property("size", float(size))
    widget.set_editor_property("font", font)
    widget.set_editor_property("color_and_opacity", unreal.SlateColor(specified_color=colour))
    if justify is not None:
        widget.set_editor_property("justification", justify)


def _has_property(obj, name):
    try:
        obj.get_editor_property(name)
        return True
    except Exception:
        return False


def button_style(button, normal, hovered, pressed, radius=2.0):
    """Buttons get all three states explicitly. The engine default turns nearly
    white on hover, which erases the label sitting on top of it."""
    # UButton calls it WidgetStyle in C++ and shows it as "Style" in the
    # Details panel; Python sees the C++ name.
    prop = "widget_style" if _has_property(button, "widget_style") else "style"
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


# --- WBP_TravelMarker ------------------------------------------------------

def build_marker():
    bp = unreal.load_asset(UI_DIR + "/WBP_TravelMarker")
    TOOLS.clear_widget_tree(bp)

    root = add(bp, unreal.VerticalBox, "MarkerRoot", None)

    # The size box is what keeps the pin round. Centre it in its slot, or the
    # vertical box stretches it to the width of the label underneath and the
    # ring turns into a squashed plate.
    size_box = add(bp, unreal.SizeBox, "MarkerSizeBox", root)
    size_box.set_width_override(30.0)
    size_box.set_height_override(30.0)
    box_slot(size_box, size=AUTO, h=H_CENTER)

    stack = add(bp, unreal.Overlay, "MarkerStack", size_box)
    align_slot(stack)

    ring = add(bp, unreal.Border, "MarkerRing", stack)
    ring.set_editor_property("background", brush(BODY, radius=15.0))
    align_slot(ring)

    dot = add(bp, unreal.Border, "MarkerDot", stack)
    dot.set_editor_property("background", brush(ACCENT, radius=9.0))
    align_slot(dot, padding=margin(6.0, 6.0, 6.0, 6.0))

    click = add(bp, unreal.Button, "MarkerButton", stack)
    button_style(click, CLEAR, unreal.LinearColor(1.0, 1.0, 1.0, 0.10),
                 unreal.LinearColor(1.0, 1.0, 1.0, 0.18), radius=15.0)
    align_slot(click)

    label = add(bp, unreal.TextBlock, "MarkerLabel", root)
    text(label, 11, MUTED, LABEL, unreal.TextJustify.CENTER)
    box_slot(label, size=AUTO, h=H_CENTER, padding=margin(0.0, 3.0, 0.0, 0.0))

    TOOLS.compile_and_save(bp)
    log("WBP_TravelMarker: {0}".format([str(n) for n in TOOLS.get_widget_names(bp)]))


# --- WBP_TravelScreen ------------------------------------------------------

def card(bp, parent, name, is_last=False):
    """One outlined sidebar card: icon column on the left, text column on the
    right. The icon fills the card's height so a three-line card still reads as
    one block rather than an icon floating next to a taller stack."""
    frame = add(bp, unreal.Border, name, parent)
    frame.set_editor_property("background", brush(PANEL, LINE))
    frame.set_editor_property("padding", margin(14.0, 12.0, 14.0, 12.0))
    box_slot(frame, size=AUTO, padding=margin(0.0, 0.0, 0.0, 0.0 if is_last else 10.0))

    row = add(bp, unreal.HorizontalBox, name + "Row", frame)
    align_slot(row)

    # Fixed on both axes and centred: letting the icon fill the card made the
    # three-line card's icon taller than the rest, and four different icon
    # sizes down one column reads as a mistake rather than as hierarchy.
    icon_box = add(bp, unreal.SizeBox, name + "IconBox", row)
    icon_box.set_width_override(30.0)
    icon_box.set_height_override(30.0)
    box_slot(icon_box, size=AUTO, v=V_CENTER, padding=margin(0.0, 0.0, 12.0, 0.0))

    icon = add(bp, unreal.Border, name + "Icon", icon_box)
    icon.set_editor_property("background", brush(ACCENT, radius=3.0))
    align_slot(icon)

    column = add(bp, unreal.VerticalBox, name + "Text", row)
    box_slot(column, size=FILL, v=V_CENTER)
    return column


def build_screen():
    bp = unreal.load_asset(UI_DIR + "/WBP_TravelScreen")
    route_class = unreal.load_asset(UI_DIR + "/WBP_TravelRoute").generated_class()
    TOOLS.clear_widget_tree(bp)

    canvas = add(bp, unreal.CanvasPanel, "RootCanvas", None)

    screen = add(bp, unreal.Border, "ScreenBorder", canvas)
    screen.set_editor_property("background", brush(GROUND))
    screen.set_editor_property("padding", margin(18.0, 18.0, 18.0, 18.0))
    fill_canvas(screen)

    row = add(bp, unreal.HorizontalBox, "MainRow", screen)
    align_slot(row)

    # ---- map side --------------------------------------------------------
    map_column = add(bp, unreal.VerticalBox, "MapColumn", row)
    box_slot(map_column, size=FILL, value=0.78, padding=margin(0.0, 0.0, 14.0, 0.0))

    header = add(bp, unreal.Border, "MapHeader", map_column)
    header.set_editor_property("background", brush(PANEL, LINE))
    header.set_editor_property("padding", margin(14.0, 8.0, 14.0, 8.0))
    box_slot(header, size=AUTO, padding=margin(0.0, 0.0, 0.0, 10.0))

    header_row = add(bp, unreal.HorizontalBox, "MapHeaderRow", header)
    align_slot(header_row)

    title = add(bp, unreal.TextBlock, "TitleText", header_row)
    text(title, 15, MUTED, HEADING)
    box_slot(title, size=AUTO, v=V_CENTER)

    where = add(bp, unreal.TextBlock, "CurrentLocationText", header_row)
    text(where, 20, BODY, HEADING)
    box_slot(where, size=AUTO, v=V_CENTER, padding=margin(22.0, 0.0, 0.0, 0.0))

    frame = add(bp, unreal.Border, "MapFrame", map_column)
    frame.set_editor_property("background", brush(MAP_FILL, LINE))
    frame.set_editor_property("padding", margin(2.0, 2.0, 2.0, 2.0))
    box_slot(frame, size=FILL)

    stack = add(bp, unreal.Overlay, "MapStack", frame)
    align_slot(stack)

    # Order matters: the overlay draws later children on top, so the route has
    # to go under the markers or the dashes cross the pins.
    background = add(bp, unreal.Image, "MapBackground", stack)
    background.set_editor_property("brush", brush(CLEAR))
    align_slot(background)

    route = add(bp, route_class, "RouteView", stack)
    align_slot(route)

    markers = add(bp, unreal.CanvasPanel, "MarkerCanvas", stack)
    align_slot(markers)

    # ---- sidebar ---------------------------------------------------------
    sidebar = add(bp, unreal.VerticalBox, "Sidebar", row)
    box_slot(sidebar, size=FILL, value=0.22)

    top = add(bp, unreal.Border, "SidebarHeader", sidebar)
    top.set_editor_property("background", brush(PANEL, LINE))
    top.set_editor_property("padding", margin(12.0, 8.0, 12.0, 8.0))
    box_slot(top, size=AUTO, padding=margin(0.0, 0.0, 0.0, 10.0))

    top_row = add(bp, unreal.HorizontalBox, "SidebarHeaderRow", top)
    align_slot(top_row)

    heading = add(bp, unreal.TextBlock, "SidebarTitleText", top_row)
    heading.set_text("SEFER")
    text(heading, 14, MUTED, HEADING)
    box_slot(heading, size=FILL, v=V_CENTER)

    close_box = add(bp, unreal.SizeBox, "CloseBox", top_row)
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

    # Four cards, in the order the reference puts them: where, which truck,
    # how long, how far.
    pin = card(bp, sidebar, "CardDestination")
    name_text = add(bp, unreal.TextBlock, "DestinationNameText", pin)
    text(name_text, 19, BODY, HEADING)
    box_slot(name_text, size=AUTO)
    type_text = add(bp, unreal.TextBlock, "DestinationTypeText", pin)
    text(type_text, 12, MUTED, LABEL)
    box_slot(type_text, size=AUTO, padding=margin(0.0, 2.0, 0.0, 0.0))

    truck = card(bp, sidebar, "CardVehicle")
    vehicle_text = add(bp, unreal.TextBlock, "VehicleNameText", truck)
    text(vehicle_text, 19, BODY, HEADING)
    box_slot(vehicle_text, size=AUTO)
    load_text = add(bp, unreal.TextBlock, "VehicleLoadText", truck)
    text(load_text, 12, MUTED, NUMBER)
    box_slot(load_text, size=AUTO, padding=margin(0.0, 2.0, 0.0, 0.0))

    clock = card(bp, sidebar, "CardTime")
    time_text = add(bp, unreal.TextBlock, "TravelTimeText", clock)
    text(time_text, 17, BODY, NUMBER)
    box_slot(time_text, size=AUTO)
    arrival_text = add(bp, unreal.TextBlock, "ArrivalTimeText", clock)
    text(arrival_text, 12, MUTED, NUMBER)
    box_slot(arrival_text, size=AUTO, padding=margin(0.0, 2.0, 0.0, 0.0))
    traffic_text = add(bp, unreal.TextBlock, "TrafficText", clock)
    text(traffic_text, 12, MUTED, LABEL)
    box_slot(traffic_text, size=AUTO, padding=margin(0.0, 2.0, 0.0, 0.0))

    road = card(bp, sidebar, "CardRoute", is_last=True)
    distance_text = add(bp, unreal.TextBlock, "DistanceText", road)
    text(distance_text, 17, BODY, NUMBER)
    box_slot(distance_text, size=AUTO)
    fuel_text = add(bp, unreal.TextBlock, "FuelText", road)
    text(fuel_text, 12, MUTED, NUMBER)
    box_slot(fuel_text, size=AUTO, padding=margin(0.0, 2.0, 0.0, 0.0))

    status = add(bp, unreal.TextBlock, "StatusText", sidebar)
    text(status, 12, MUTED, LABEL)
    box_slot(status, size=AUTO, padding=margin(2.0, 10.0, 2.0, 0.0))

    tail = add(bp, unreal.Spacer, "SidebarSpacer", sidebar)
    box_slot(tail, size=FILL)

    confirm_box = add(bp, unreal.SizeBox, "ConfirmBox", sidebar)
    confirm_box.set_height_override(56.0)
    box_slot(confirm_box, size=AUTO, padding=margin(0.0, 10.0, 0.0, 0.0))

    confirm = add(bp, unreal.Button, "ConfirmButton", confirm_box)
    button_style(confirm, ACCENT, ACCENT_HOVER, ACCENT_PRESS)
    align_slot(confirm)

    confirm_label = add(bp, unreal.TextBlock, "ConfirmLabelText", confirm)
    text(confirm_label, 16, BODY, HEADING, unreal.TextJustify.CENTER)
    align_slot(confirm_label, h=H_CENTER, v=V_CENTER)

    TOOLS.compile_and_save(bp)
    log("WBP_TravelScreen: {0} widgets".format(len(TOOLS.get_widget_names(bp))))


def main():
    build_marker()
    build_screen()
    unreal.log("[Deadline travel-ui] ---- done ----")
    for line in _report:
        unreal.log("[Deadline travel-ui] " + line)


main()
