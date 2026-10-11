import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_portal34.json")
FIX = {"kongjianchuansuoqi3": -5085.0, "kongjianchuansuoqi4": -5085.0}
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
    done = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl not in FIX:
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "TimeEraPortal" not in str(call(c.get_class().get_name)):
                continue
            before = gp(c, "VerticalOffset")
            call(c.set_editor_property, "VerticalOffset", FIX[lbl],
                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            after = gp(c, "VerticalOffset")
            done[lbl] = {"before": str(before), "after": str(after),
                         "loc_z": round(float(call(a.get_actor_location).z), 1)}
    report["fixed"] = done
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###FIX34 " + json.dumps(report, ensure_ascii=True)[:700])