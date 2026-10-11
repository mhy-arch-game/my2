import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_proximity.json")
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
        comps = []
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            cn = str(call(c.get_class().get_name))
            if "ProximitySequence" not in cn:
                continue
            seq = call(c.get_editor_property, "Sequence")
            comps.append({
                "name": str(call(c.get_name)),
                "class": cn,
                "Sequence": str(seq)[:120],
                "TriggerDistance": str(call(c.get_editor_property, "TriggerDistance")),
                "bOnce": str(call(c.get_editor_property, "bOnce")),
                "bLoop": str(call(c.get_editor_property, "bLoop")),
                "PlayRate": str(call(c.get_editor_property, "PlayRate")),
                "bBlockOnFinish": str(call(c.get_editor_property, "bBlockOnFinish")),
                "bApplyInitialStateOnBeginPlay": str(call(c.get_editor_property, "bApplyInitialStateOnBeginPlay")),
            })
        res[lbl] = {"comp_count": len(comps), "comps": comps,
                    "hidden": str(call(a.get_editor_property, "bHidden")),
                    "collision": str(call(a.get_actor_enable_collision))}
    report["actors"] = res
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VERIFY " + json.dumps(report, ensure_ascii=True)[:2600])
print("[VERIFY done]")