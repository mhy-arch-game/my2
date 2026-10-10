import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_proximity2.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:80] + ">"

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    res = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("jiguanqiang"):
            continue
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        classes = [str(call(c.get_class().get_name)) for c in comps]
        ps = {}
        for c in comps:
            if "ProximitySequence" not in str(call(c.get_class().get_name)):
                continue
            for n in ("Sequence", "TriggerDistance", "bOnce", "bBlockOnFinish",
                      "bApplyInitialStateOnBeginPlay", "bSlowPlayerNearby",
                      "SlowZoneDistance", "SlowSpeedMultiplier"):
                ps[n] = str(call(c.get_editor_property, n))[:90]
        res[lbl] = {"components": classes, "proximity": ps,
                    "hidden": str(call(a.get_editor_property, "bHidden")),
                    "collision": str(call(a.get_actor_enable_collision))}
    report["actors"] = res
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VERIFY2 " + json.dumps(report, ensure_ascii=True)[:2600])
print("[VERIFY2 done]")