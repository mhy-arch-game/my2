import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_floors.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:70] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(ss.get_all_level_actors())
    portals = []
    for a in actors:
        lbl = str(call(a.get_actor_label))
        if lbl.startswith("kongjianchuansuoqi"):
            loc = call(a.get_actor_location)
            portals.append((lbl, a, loc))
    out = {}
    for lbl, a, loc in portals:
        cands = []
        for b in actors:
            if b == a:
                continue
            blbl = str(call(b.get_actor_label))
            if blbl.startswith("kongjianchuansuoqi"):
                continue
            bl = call(b.get_actor_location)
            if abs(float(bl.x) - float(loc.x)) > 700 or abs(float(bl.y) - float(loc.y)) > 700:
                continue
            r = call(b.get_actor_bounds, False)
            if isinstance(r, str) or len(r) < 2:
                continue
            o, e = r[0], r[1]
            top = round(float(o.z) + float(e.z), 1)
            if abs(top - float(loc.z)) > 400:
                continue
            cands.append({"lbl": blbl[:28], "top_z": top, "half_xy": [round(float(e.x), 1), round(float(e.y), 1)]})
        cands.sort(key=lambda c: abs(c["top_z"] - float(loc.z)))
        out[lbl] = {"origin_z": round(float(loc.z), 1), "floor_candidates": cands[:5]}
    report["floors"] = out
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###FLOOR " + json.dumps(report, ensure_ascii=True)[:500])