import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "temp_big_trigger.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    done = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl not in ("jiguanqiang1", "jiguanqiang2"):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "ProximitySequence" not in str(call(c.get_class().get_name)):
                continue
            call(c.set_editor_property, "TriggerDistance", 100000.0)
            call(c.set_editor_property, "DebugLogRange", 6000.0)
            done[lbl] = [str(call(c.get_editor_property, "TriggerDistance")), str(call(c.get_editor_property, "DebugLogRange"))]
    report["set"] = done
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:40]
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###BIGTRIG " + json.dumps(report, ensure_ascii=True)[:500])
print("[BIGTRIG done]")