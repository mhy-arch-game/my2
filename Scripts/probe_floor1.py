import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_floor1.json")
PX, PY, PZ = 620.0, -290.0, 100.0
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:70]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cands = []
    for a in ss.get_all_level_actors():
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        if not any("StaticMeshComponent" in str(call(c.get_class().get_name)) for c in comps):
            continue
        r = call(a.get_actor_bounds, False)
        if isinstance(r, str) or r is None or len(r) < 2:
            continue
        o, e = r[0], r[1]
        x0, x1 = float(o.x) - float(e.x), float(o.x) + float(e.x)
        y0, y1 = float(o.y) - float(e.y), float(o.y) + float(e.y)
        z0, z1 = float(o.z) - float(e.z), float(o.z) + float(e.z)
        if not (x0 <= PX <= x1 and y0 <= PY <= y1):
            continue
        if z1 < PZ - 500 or z1 > PZ + 500:
            continue
        cands.append({"label": str(call(a.get_actor_label))[:32],
                      "top": round(z1, 1), "bottom": round(z0, 1), "thick": round(z1 - z0, 1),
                      "xy_ext": [round(float(e.x),1), round(float(e.y),1)]})
    cands.sort(key=lambda c: abs(c["top"] - PZ))
    report["point"] = [PX, PY, PZ]
    report["candidates"] = cands[:12]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("FLOOR PROBE done")