import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_pair34_v2.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    changed = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl not in ("kongjianchuansuoqi3", "kongjianchuansuoqi4"):
            continue
        loc = call(a.get_actor_location)
        entry = {"loc_before": [round(float(loc.x), 1), round(float(loc.y), 1), round(float(loc.z), 1)]}
        if lbl == "kongjianchuansuoqi4":
            # 把 4 抬到它那侧的站立面（与同区域"今"半 6 号一致）
            call(a.set_actor_location, unreal.Vector(float(loc.x), float(loc.y), -3700.0), False, False)
            loc2 = call(a.get_actor_location)
            entry["loc_after"] = [round(float(loc2.x), 1), round(float(loc2.y), 1), round(float(loc2.z), 1)]
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "TimeEraPortal" not in str(call(c.get_class().get_name)):
                continue
            entry["vo_before"] = str(gp(c, "VerticalOffset"))
            call(c.set_editor_property, "VerticalOffset", -5000.0,
                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            entry["vo_after"] = str(gp(c, "VerticalOffset"))
        changed[lbl] = entry
    report["changed"] = changed
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###FIX2 " + json.dumps(report, ensure_ascii=True)[:800])