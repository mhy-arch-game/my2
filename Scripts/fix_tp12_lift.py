import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_tp12_lift.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:80]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    done = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl not in ("kongjianchuansuoqi1", "kongjianchuansuoqi2"):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "TimeEraPortal" not in str(call(c.get_class().get_name)):
                continue
            before = gp(c, "TeleportOffset")
            call(c.set_editor_property, "TeleportOffset", unreal.Vector(5.0, 0.0, 100.0),
                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            after = gp(c, "TeleportOffset")
            done[lbl] = {"before": str(before)[:60], "after": str(after)[:60],
                         "vo": str(gp(c, "VerticalOffset")),
                         "place_on_ground": str(gp(c, "bPlaceOnGround")),
                         "trace_dist": str(gp(c, "GroundTraceDistance"))}
    report["fixed"] = done
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("FIX TP12 done")