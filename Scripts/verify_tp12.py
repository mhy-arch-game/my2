import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_tp12.json")
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
    out = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        loc = call(a.get_actor_location)
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "TimeEraPortal" not in str(call(c.get_class().get_name)):
                continue
            t = gp(c, "TeleportOffset")
            vo = gp(c, "VerticalOffset")
            det = {"x": round(float(t.x),1) if t is not None else None,
                   "y": round(float(t.y),1) if t is not None else None,
                   "z": round(float(t.z),1) if t is not None else None,
                   "vo": float(vo) if vo is not None else None,
                   "loc_z": round(float(loc.z),1)}
            out[lbl] = det
    report["teleporters"] = out
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("VERIFY TP12 done")