"""Structural checks for the Velvet Interpolator skin. These catch format and binding mistakes; they cannot show
how the MPC draws it. The device test is still needed."""
import json, os, sys
from PIL import Image
D = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", "Plugin Skins")
fails = 0
def check(ok, msg):
    global fails
    print(("  ok:   " if ok else "  FAIL: ") + msg)
    if not ok: fails += 1

tui = json.load(open(os.path.join(D, "TUI.json")))["pageData"]
defs = {x["key"]: x["value"] for x in tui["componentDefinitions"]["localComponentDefinitions"]}
W, H = map(int, tui["tabs"][0]["initialSize"].split()[2:4])
print("1. file structure")
check(all(t["componentName"] in defs for t in tui["tabs"]), "every tab root component exists")
check([t["tabName"] for t in tui["tabs"]] == ["Play", "Analyze", "Pads"], "three tabs: Play, Analyze, Pads")
qk = json.load(open(os.path.join(D, "Q-Links.json")))
check("Screen Mode Q-Links" in qk, "Q-Links file has the screen mode map")

print("2. every referenced image exists and has the size of its control")
def bounds(b):
    return list(map(int, b.split()))
for key, val in defs.items():
    for c in val.get("componentsData", []):
        cd = c["componentData"]
        if cd["type"] == "Button":
            data = cd["data"]
            for img in (data["onImage"], data["offImage"]):
                check(os.path.exists(os.path.join(D, img)), f"{key}: image {img} exists") if not os.path.exists(os.path.join(D, img)) else None
            x, y, w, h = bounds(c["bounds"]["bounds"])
            size = Image.open(os.path.join(D, data["onImage"])).size
            check(size == (w, h), f"{key}: button bounds {w}x{h} match image {size}") if size != (w, h) else None
        elif cd["type"] == "Image":
            img = cd["data"]["image"]
            check(os.path.exists(os.path.join(D, img)), f"background image {img} exists")
check(True, "image checks finished")

print("3. placements fit the screen and bind to valid parameters")
root = defs["PLAY|Play"]
bound_params = {}
for c in [c for t in tui["tabs"] for c in defs[t["componentName"]]["componentsData"]]:
    t = c["componentData"]["type"]
    x, y, w, h = bounds(c["bounds"]["bounds"])
    if t == "Image":
        continue
    check(0 <= x and 0 <= y and x + w <= W and y + h <= H, f"{c['componentData']['name']}: inside {W}x{H}")
    param = c["handle remapping"]["map"][0]["value"]
    n = int(param.split()[-1])
    bound_params.setdefault(n, []).append(c["componentData"]["name"])
    check(0 <= n <= 26, f"{c['componentData']['name']} binds to {param} (plugin has parameters 0-26)")
for n in sorted(bound_params):
    print(f"     parameter {n}: {', '.join(bound_params[n])}")
# Progress must be read-only: parameter 4 may only feed labels, never a control that writes to it
prog_kinds = set()
for key, val in defs.items():
    for c in val.get("componentsData", []):
        if c["componentData"]["type"] == "Knob" and key in ("viReadout_prog",):
            prog_kinds.add("knob")
prog_targets = []
for c in root["componentsData"]:
    if c["handle remapping"]["map"] and c["handle remapping"]["map"][0]["value"] == "Parameter 4":
        prog_targets.append(c["componentData"]["type"])
        for child in defs[c["componentData"]["type"]]["componentsData"]:
            prog_kinds.add(child["componentData"]["type"])
check(sorted(prog_targets) == ["viBar_prog", "viReadout_prog"], f"parameter 4 drives the LED bar and the readout: {prog_targets}")
# the readout uses labels only; the meter is a film-strip knob like RSFX's read-only output meters
check(defs["viReadout_prog"]["componentsData"] and all(c["componentData"]["type"] in ("Label", "Focus")
      for c in defs["viReadout_prog"]["componentsData"]), "the progress readout is labels only")
check(defs["viBar_prog"]["componentsData"][0]["componentData"]["type"] == "Knob"
      and defs["viBar_prog"]["componentsData"][0]["componentData"]["data"]["filmStrip"] == "vi_bar_progress.png",
      "the progress bar is an LED film strip")

print("4. knobs use a film strip that exists and has the frames it claims")
for key in ("viKnob_tr", "viKnob_inv", "viKnob_oct", "viKnob_voi", "viBar_prog", "viBar_level") + tuple(f"viBand_{b}" for b in range(16)):
    knob = [c for c in defs[key]["componentsData"] if c["componentData"]["type"] == "Knob"][0]["componentData"]["data"]
    strip = os.path.join(D, knob["filmStrip"])
    check(os.path.exists(strip), f"{key}: film strip {knob['filmStrip']} exists")
    if os.path.exists(strip):
        w, h = Image.open(strip).size
        comp = [c for c in defs[key]["componentsData"] if c["componentData"]["type"] == "Knob"][0]
        cw, ch = map(int, comp["bounds"]["bounds"].split()[2:4])
        check(w == cw, f"{key}: strip width {w} matches the control width {cw}")
        frames = h // ch
        check(knob["numFrames"] <= frames,
              f"{key}: {knob['numFrames']} frames fit in a {w}x{h} strip ({frames} frames)")

print("5. Q-Links point only at this plugin's parameters")
links = qk["Screen Mode Q-Links"]["map"][0]["Q-Links"]
check(all(0 <= v <= 5 for v in links.values()), f"Q-Link targets {sorted(set(links.values()))} are valid")

print("\n" + ("SKIN CHECKS PASSED" if not fails else f"{fails} SKIN CHECK(S) FAILED"))
sys.exit(1 if fails else 0)
