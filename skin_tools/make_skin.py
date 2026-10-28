"""Writes the Velvet Interpolator screen skin (TUI.json, Q-Links.json and PNG images).

Structure copied from the Velvet Chords skin: a tab with a root component, local component definitions for
each control, and a Button child per control. The action definitions are copied verbatim from Velvet Chords'
skin, so only the layout, the images and the parameter bindings are this plugin's own.

Parameter bindings (plugin parameter index -> control):
  0 Record (toggle), 1 Analyse (toggle), 2 Transpose (5 segments: -12 -6 0 +6 +12),
  3 Inversion (3 segments: root, 1st, 2nd), 5 Line (toggle).  Parameter 4 (Progress) is read-only, no control.
"""
import copy, json, os, shutil, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
DEFS = os.path.join(HERE, "assets", "velvet_action_defs.json")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "out", "Plugin Skins")
W, H = 1280, 628
# Palette sampled from the RSFX skin: charcoal background, amber LED, oxblood bypass, dark bypass-off plate.
RS_BG = os.path.join(HERE, "assets", "rs_bg.png")
# Velvet palette: graphite chassis, champagne gold, silver, and a restrained violet accent
GRAPHITE_TOP = (44, 46, 52)
GRAPHITE_BOT = (22, 23, 27)
PANEL = (30, 31, 36)
PANEL_EDGE = (62, 65, 72)
PANEL_HI = (78, 82, 90)
CHAMPAGNE = (236, 213, 150)
CHAMPAGNE_DARK = (176, 142, 78)
CHAMPAGNE_BRIGHT = (250, 236, 196)
SILVER = (206, 211, 219)
SILVER_DIM = (130, 136, 146)
VIOLET = (150, 128, 214)
VIOLET_DIM = (60, 52, 88)
INK = (20, 21, 25)
BLACK_DEEP = (12, 13, 16)
# aliases used by the drawing code below
AMBER = CHAMPAGNE
AMBER_DIM = (70, 56, 34)
OXBLOOD = (120, 34, 26)
TEXT_OFF = SILVER_DIM
TEXT_DARK = INK
TEXT_LABEL = SILVER
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_SMALL = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

_defs = json.load(open(DEFS))
TOGGLE_ACTIONS = _defs["toggle_actions"]     # Enter Pressed -> Toggle Switch (from Velvet Chords' skin)
SEGMENT_ACTIONS = _defs["segment_actions"]   # Mouse Down -> Q-Link Data
ROOT_BG = _defs["root_background"]
KNOB_TEMPLATE = json.load(open(os.path.join(HERE, "assets", "knob_template.json")))   # RSFX medium knob
METER_TEMPLATE = json.load(open(os.path.join(HERE, "assets", "meter_template.json")))  # RSFX meter film strip
LED_TEMPLATE = json.load(open(os.path.join(HERE, "assets", "led_template.json")))      # RSFX 16px LED

def grad(w, h, top, bot):
    """A vertical gradient image of size w x h."""
    base = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        base.putpixel((0, y), tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)))
    return base.resize((w, h))

def rounded_mask(w, h, radius):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
    return m

def paste_gradient_plate(img, box, top, bot, radius, edge, hi):
    """Gradient plate with a light top edge and a fine outline, in the Velvet chassis style."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    g = grad(w, h, top, bot)
    img.paste(g, (x0, y0), rounded_mask(w, h, radius))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle(box, radius=radius, outline=edge, width=1)
    d.line([x0 + radius, y0 + 1, x1 - radius, y0 + 1], fill=hi)

def plate(name, w, h, label, on, led_on=None, font_path=FONT, size=24):
    """A Velvet control plate: graphite when off, champagne when on, with a silver or ink label."""
    img = Image.new("RGB", (w, h), PANEL)
    d = ImageDraw.Draw(img)
    if on:
        paste_gradient_plate(img, (1, 1, w - 1, h - 1), CHAMPAGNE_BRIGHT, CHAMPAGNE_DARK, 8, CHAMPAGNE_DARK, (255, 250, 230))
        text_col = TEXT_DARK
    else:
        paste_gradient_plate(img, (1, 1, w - 1, h - 1), GRAPHITE_TOP, GRAPHITE_BOT, 8, PANEL_EDGE, PANEL_HI)
        text_col = TEXT_OFF
    f = ImageFont.truetype(font_path, size)
    bb = d.textbbox((0, 0), label, font=f)
    tx = (w - (bb[2] - bb[0])) / 2 - bb[0] + (14 if led_on is not None else 0)
    ty = (h - (bb[3] - bb[1])) / 2 - bb[1]
    if led_on is not None:
        cx, cy, r = 22, h // 2, 7
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=VIOLET if led_on else VIOLET_DIM,
                  outline=INK if led_on else PANEL_EDGE)
    d.text((tx, ty), label, fill=text_col, font=f)
    img.save(os.path.join(IMG_DIR, name))

def button_def(key, onimg, offimg, w, h, button_id, group, actions):
    return {"key": key, "value": {
        "version": 4, "actions": actions,
        "backgroundData": ROOT_BG, "ignoreMousePresses": False, "disableCoarseDataWheel": False,
        "repeats": 1, "hideQLinkBounds": True,
        "componentsData": [{
            "version": 2,
            "componentData": {"version": 1, "name": "Button", "type": "Button", "data": {
                "version": 2, "onImage": onimg, "offImage": offimg, "buttonId": button_id,
                "numButtonsInGroup": group, "handleName": "Data", "gestureBehaviour": "Instant"}},
            "handle remapping": {"version": 1, "map": []},
            "bounds": {"version": 2, "acceptsHWFocus": "Yes", "showWhenDataModelInvalid": "Hide",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": f"0 0 {w} {h}",
                       "additionalInvalidatingHandles": []}}]}}

def place(key, name, x, y, w, h, param):
    return {"version": 2,
            "componentData": {"version": 1, "name": name, "type": key, "data": {"version": 1, "handleName": "Data"}},
            "handle remapping": {"version": 1, "map": [{"key": "Data", "value": f"Parameter {param}"}]},
            "bounds": {"version": 2, "acceptsHWFocus": "Yes", "showWhenDataModelInvalid": "Hide",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": f"{x} {y} {w} {h}",
                       "additionalInvalidatingHandles": []}}

def label_bg(text, x, y):
    return {"version": 2,
            "componentData": {"version": 1, "name": "Label " + text, "type": "Image", "data": {
                "version": 2, "imageType": "Regular", "colour": "0", "image": "vi_bg.png"}},
            "handle remapping": {"version": 1, "map": []},
            "bounds": {"version": 2, "acceptsHWFocus": "No", "showWhenDataModelInvalid": "Show",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": f"0 0 {W} {H}",
                       "additionalInvalidatingHandles": []}}

LED_W, LED_H, LED_FRAMES = 640, 18, 128
LED_OFF = (36, 38, 44)
LED_AMBER = CHAMPAGNE
LED_GREEN = SILVER
LED_RED = (226, 96, 80)
LED_SEGMENTS = 40

def led_colour(seg, lit, mode):
    """Colour of one LED segment: progress is all amber; level goes green, amber, red up the bar."""
    if not lit:
        return LED_OFF
    if mode == "progress":
        return LED_AMBER
    frac = seg / (LED_SEGMENTS - 1)
    if frac < 0.7:
        return LED_GREEN
    if frac < 0.9:
        return LED_AMBER
    return LED_RED

def led_strip(name, mode):
    """Film strip: frame k lights the first k/(FRAMES-1) of the bar. Frames stack vertically."""
    strip = Image.new("RGB", (LED_W, LED_H * LED_FRAMES), (14, 15, 18))
    d = ImageDraw.Draw(strip)
    seg_w = LED_W / LED_SEGMENTS
    for k in range(LED_FRAMES):
        fill = k / (LED_FRAMES - 1) * LED_SEGMENTS
        for seg in range(LED_SEGMENTS):
            lit = seg + 0.5 <= fill
            x0 = seg * seg_w + 2; x1 = (seg + 1) * seg_w - 2
            y0 = k * LED_H + 3; y1 = k * LED_H + LED_H - 3
            d.rounded_rectangle([x0, y0, x1, y1], radius=3, fill=led_colour(seg, lit, mode),
                                outline=(70, 60, 44) if not lit else None)
    strip.save(os.path.join(OUT, name))

KNOB_SIZE = 76
KNOB_FRAMES = 128

def led_images(folder):
    """Violet record LED, on and off, 16 x 16 like the layout expects."""
    for name, fill, rim in (("vi_led_on.png", VIOLET, (200, 190, 240)), ("vi_led_off.png", VIOLET_DIM, PANEL_EDGE)):
        img = Image.new("RGB", (16, 16), PANEL)
        d = ImageDraw.Draw(img)
        d.ellipse([2, 2, 13, 13], fill=fill, outline=rim)
        img.save(os.path.join(folder, name))

def knob_strip(name):
    """A film strip of rotation frames: graphite body, silver rim, champagne indicator sweeping -135..+135 degrees."""
    n = KNOB_SIZE
    strip = Image.new("RGB", (n, n * KNOB_FRAMES), BLACK_DEEP)
    c = n / 2
    for k in range(KNOB_FRAMES):
        frame = Image.new("RGB", (n, n), BLACK_DEEP)
        d = ImageDraw.Draw(frame)
        r = n / 2 - 4
        d.ellipse([c - r - 2, c - r - 2, c + r + 2, c + r + 2], fill=(58, 61, 68))           # rim shadow
        d.ellipse([c - r, c - r, c + r, c + r], fill=SILVER_DIM, outline=SILVER)            # silver rim
        inner = r - 5
        d.ellipse([c - inner, c - inner, c + inner, c + inner], fill=GRAPHITE_BOT)           # body
        d.arc([c - inner, c - inner, c + inner, c + inner], start=200, end=320, fill=PANEL_HI, width=2)  # highlight
        theta = math_radians(-135 + 270 * k / (KNOB_FRAMES - 1))
        x0, y0 = c + (inner * 0.18) * math_sin(theta), c - (inner * 0.18) * math_cos(theta)
        x1, y1 = c + (inner * 0.88) * math_sin(theta), c - (inner * 0.88) * math_cos(theta)
        d.line([x0, y0, x1, y1], fill=INK, width=5)                                           # indicator shadow
        d.line([x0, y0, x1, y1], fill=CHAMPAGNE_BRIGHT, width=3)                              # champagne indicator
        strip.paste(frame, (0, k * n))
    strip.save(os.path.join(OUT, name))

def math_radians(deg):
    import math
    return math.radians(deg)

def math_sin(t):
    import math
    return math.sin(t)

def math_cos(t):
    import math
    return math.cos(t)

BAND_W, BAND_H, BAND_SEGMENTS = 22, 150, 16

def band_strip(name):
    """Film strip for one LED column: frame k lights k/127 of the column, from the bottom up."""
    strip = Image.new("RGB", (BAND_W, BAND_H * LED_FRAMES), (14, 15, 18))
    d = ImageDraw.Draw(strip)
    seg_h = BAND_H / BAND_SEGMENTS
    for k in range(LED_FRAMES):
        lit_count = round(k / (LED_FRAMES - 1) * BAND_SEGMENTS)
        for seg in range(BAND_SEGMENTS):                       # seg 0 is the bottom
            lit = seg < lit_count
            y1 = k * BAND_H + BAND_H - seg * seg_h - 2
            y0 = y1 - seg_h + 2
            frac = seg / (BAND_SEGMENTS - 1)
            colour = LED_RED if frac >= 0.9 else (CHAMPAGNE if frac >= 0.7 else SILVER)
            d.rounded_rectangle([3, y0, BAND_W - 4, y1], radius=2,
                                fill=colour if lit else LED_OFF, outline=None if lit else (58, 60, 66))
    strip.save(os.path.join(OUT, name))

def build():
    global IMG_DIR
    os.makedirs(OUT, exist_ok=True)
    IMG_DIR = OUT
    # background: the RSFX charcoal texture, a header bar, and a rounded panel per section
    bg = grad(W, H, (40, 42, 48), (13, 14, 17))
    d = ImageDraw.Draw(bg)
    for y in range(0, H, 3):                                   # brushed-metal texture
        d.line([0, y, W, y], fill=(255, 255, 255), width=1)
    bg = Image.blend(bg, grad(W, H, (40, 42, 48), (13, 14, 17)), 0.96)
    d = ImageDraw.Draw(bg)
    d.rectangle([0, 0, W, 56], fill=BLACK_DEEP)
    d.line([0, 56, W, 56], fill=CHAMPAGNE_DARK, width=2)
    f_mark = ImageFont.truetype(FONT, 26)
    d.text((40, 13), "VELVET", fill=CHAMPAGNE, font=f_mark)
    d.text((40 + d.textlength("VELVET ", font=f_mark), 19), "INTERPOLATOR", fill=SILVER,
           font=ImageFont.truetype(FONT_SMALL, 16))
    d.text((W - 230, 20), "0.1.0", fill=TEXT_OFF, font=ImageFont.truetype(FONT_SMALL, 18))
    f_small = ImageFont.truetype(FONT_SMALL, 14)
    d.text((1140, 24), "REC", fill=TEXT_LABEL, font=f_small)
    d.text((712, 184), "FREQUENCY BANDS", fill=SILVER, font=ImageFont.truetype(FONT_SMALL, 14))
    d.text((712, 358), "80 Hz", fill=SILVER_DIM, font=ImageFont.truetype(FONT_SMALL, 13))
    d.text((1124, 358), "10 kHz", fill=SILVER_DIM, font=ImageFont.truetype(FONT_SMALL, 13))
    d.text((60, 428), "ANALYSE", fill=AMBER, font=ImageFont.truetype(FONT, 18))
    d.text((60, 474), "INPUT", fill=AMBER, font=ImageFont.truetype(FONT, 18))
    for (x0, y0, x1, y1) in [(40, 72, 1240, 168), (40, 184, 1240, 384)]:
        d.rounded_rectangle([x0, y0, x1, y1], radius=14, fill=PANEL, outline=PANEL_EDGE, width=2)
    # the LED screen: a dark display panel
    d.rounded_rectangle([40, 400, 1240, 506], radius=14, fill=(10, 11, 13), outline=AMBER_DIM, width=2)
    bg.save(os.path.join(OUT, "vi_bg.png"))

    defs = []
    images = {}
    # toggles: Record, Analyse, Line (one button each)
    toggles = [("viToggle_rec", "REC", "RECORD", 60, 96, 260, 56, 0),
               ("viToggle_ana", "ANA", "ANALYSE", 360, 96, 260, 56, 1),
               ("viToggle_line", "LINE", "LINE", 660, 96, 260, 56, 5)]
    for key, short, label, x, y, w, h, param in toggles:
        on, off = f"{key}_on.png", f"{key}_off.png"
        plate(on, w, h, label, on=True, led_on=True, size=24)
        plate(off, w, h, label, on=False, led_on=False, size=24)
        defs.append(button_def(key, on, off, w, h, 1, 1, TOGGLE_ACTIONS))
    # Transpose, Inversion, Octave and Voicing: RSFX-style film-strip knobs. Name and Value labels come from the plugin.
    for key in ("viKnob_tr", "viKnob_inv", "viKnob_oct", "viKnob_voi"):
        knob = copy.deepcopy(KNOB_TEMPLATE)
        kd = [c for c in knob["componentsData"] if c["componentData"]["type"] == "Knob"][0]["componentData"]["data"]
        kd["filmStrip"] = "vi_knob.png"
        kd["numFrames"] = KNOB_FRAMES
        defs.append({"key": key, "value": knob})
    knob_strip("vi_knob.png")
    led_images(OUT)
    led_strip("vi_bar_progress.png", "progress")
    led_strip("vi_bar_level.png", "level")
    # LED screen: horizontal LED bars (progress and input level) and a record LED
    for key, img in (("viBar_prog", "vi_bar_progress.png"), ("viBar_level", "vi_bar_level.png")):
        bar = copy.deepcopy(METER_TEMPLATE)
        knob = [c for c in bar["componentsData"] if c["componentData"]["type"] == "Knob"][0]
        knob["componentData"]["data"]["filmStrip"] = img
        knob["componentData"]["data"]["numFrames"] = LED_FRAMES
        knob["componentData"]["data"]["dragOrientation"] = "Horizontal"
        knob["bounds"]["bounds"] = f"0 0 {LED_W} {LED_H}"
        defs.append({"key": key, "value": bar})
    led = copy.deepcopy(LED_TEMPLATE)
    ld = led["componentsData"][0]["componentData"]["data"]
    ld["onImage"], ld["offImage"] = "vi_led_on.png", "vi_led_off.png"
    defs.append({"key": "viLed_rec", "value": led})
    # LED box: 16 columns, one per frequency band (parameters 7 to 22)
    for b in range(16):
        band_strip(f"vi_band_{b}.png")
        col = copy.deepcopy(METER_TEMPLATE)
        knob = [c for c in col["componentsData"] if c["componentData"]["type"] == "Knob"][0]
        knob["componentData"]["data"]["filmStrip"] = f"vi_band_{b}.png"
        knob["componentData"]["data"]["numFrames"] = LED_FRAMES
        knob["bounds"]["bounds"] = f"0 0 {BAND_W} {BAND_H}"
        defs.append({"key": f"viBand_{b}", "value": col})
    readout_keys = ("viReadout_prog", "viReadout_level", "viReadout_key", "viReadout_keyconf")
    for key in readout_keys:
        defs.append({"key": key, "value": copy.deepcopy(KNOB_TEMPLATE)})
    # the readout uses the knob's labels only: drop its film-strip so only Name and Value show
    for readout in defs[-len(readout_keys):]:
        readout["value"]["componentsData"] = [c for c in readout["value"]["componentsData"]
                                              if c["componentData"]["type"] != "Knob"]

    children = [label_bg("", 0, 0)]
    # (placement of the knobs and the progress readout is below)
    for key, short, label, x, y, w, h, param in toggles:
        children.append(place(key, label.title(), x, y, w, h, param))
    children.append(place("viKnob_tr", "Transpose", 300, 200, 108, 138, 2))
    children.append(place("viKnob_inv", "Inversion", 560, 200, 108, 138, 3))
    children.append(place("viLed_rec", "Record LED", 1190, 22, 16, 16, 0))
    for b in range(16):
        children.append(place(f"viBand_{b}", f"Band {b + 1}", 712 + b * 28, 204, BAND_W, BAND_H, 7 + b))
    children.append(place("viBar_prog", "Analyse progress", 200, 420, LED_W, LED_H, 4))
    children.append(place("viReadout_prog", "Progress", 880, 404, 260, 60, 4))
    children.append(place("viBar_level", "Input level", 200, 466, LED_W, LED_H, 6))
    children.append(place("viReadout_level", "Level", 880, 450, 260, 60, 6))

    def page(components):
        return {"version": 4, "actions": [], "backgroundData": ROOT_BG, "ignoreMousePresses": False,
                "disableCoarseDataWheel": False, "repeats": 1, "hideQLinkBounds": True, "componentsData": components}
    defs.append({"key": "PLAY|Play", "value": page(children)})
    # Analyze: the Analyse toggle, key and key confidence readouts, and the progress bar
    analyze = [label_bg("", 0, 0),
               place("viToggle_ana", "Analyse", 60, 96, 260, 56, 1),
               place("viReadout_key", "Key", 360, 96, 260, 60, 25),
               place("viReadout_keyconf", "Key confidence", 660, 96, 260, 60, 26),
               place("viBar_prog", "Analyse progress", 200, 420, LED_W, LED_H, 4),
               place("viReadout_prog", "Progress", 880, 404, 260, 60, 4)]
    defs.append({"key": "ANALYZE|Analyze", "value": page(analyze)})
    # Pads: Octave sets the pad window for melody_pads.mid; Voicing sets the chord shape in voiced_chords.mid
    pads = [label_bg("", 0, 0),
            place("viKnob_oct", "Octave", 300, 200, 108, 138, 23),
            place("viKnob_voi", "Voicing", 560, 200, 108, 138, 24)]
    defs.append({"key": "PADS|Pads", "value": page(pads)})
    tabs = [("Play", "PLAY|Play"), ("Analyze", "ANALYZE|Analyze"), ("Pads", "PADS|Pads")]
    tui = {"pageData": {
        "version": 1,
        "componentDefinitions": {"version": 2, "importFiles": [], "localComponentDefinitions": defs},
        "info": {"version": 1, "type": "CompleteDescription"},
        "tabs": [{"version": 3, "tabName": name, "fnKeyIndex": i, "fnKeySubIndex": 0,
                  "qlinkBoundsData": ["0 0 0 0"], "componentName": comp, "initialSize": f"0 0 {W} {H}",
                  "scale": 1.0} for i, (name, comp) in enumerate(tabs)]}}
    with open(os.path.join(OUT, "TUI.json"), "w") as f:
        json.dump(tui, f, indent=1)
    # Q-Links: knobs on the MPC map to the plugin's parameters (only the ones this plugin uses)
    qlinks = {"version": 4, "info": {"version": 1, "type": "CompleteDescription"},
              "Screen Mode Q-Links": {"version": 4, "map": [{"Tab": 1, "SubTab": 1, "Bank Direction": "Column",
                  "Q-Links": {"Q-Link 1": 2, "Q-Link 2": 3, "Q-Link 3": 1, "Q-Link 4": 0, "Q-Link 5": 5}}]},
              "Program Mode Q-Links": {"version": 4, "map": []}}
    with open(os.path.join(OUT, "Q-Links.json"), "w") as f:
        json.dump(qlinks, f, indent=1)
    print("wrote", OUT, "with", len(defs), "definitions and", len(os.listdir(OUT)), "files")

if __name__ == "__main__":
    build()
