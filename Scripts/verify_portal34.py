import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_portal34.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    info = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        z = round(float(call(a.get_actor_location).z), 1)
        era = None
        vo = None
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            cn = str(call(c.get_class().get_name))
            if "TimeEraComponent" in cn:
                era = str(gp(c, "Era"))
            if "TimeEraPortal" in cn:
                vo = gp(c, "VerticalOffset")
        dest = None
        if vo is not None and era is not None:
            if "ANCIENT" in era:
                dest = round(z + float(vo), 1)
            elif "MODERN" in era:
                dest = round(z - float(vo), 1)
        info[lbl] = {"z": z, "era": era, "verticalOffset": str(vo), "predicted_dest_z": dest}
    report["portals"] = info
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VERIFY34 " + json.dumps(report, ensure_ascii=True)[:2400])