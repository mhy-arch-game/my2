import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_wall_slave_enabled.json")
WALLS = ["jiguanqiang0", "jiguanqiang1", "jiguanqiang3", "jiguanqiang4"]
report = {"walls": []}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl not in WALLS:
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        if ic is None:
            report["walls"].append({"label": lbl, "error": "no interactable"})
            continue
        call(ic.modify)
        before = str(call(ic.get_editor_property, "bEnabled")[1])
        w = call(ic.set_editor_property, "bEnabled", False)[0]
        report["walls"].append({"label": lbl, "before_bEnabled": before, "write": w,
                                "after_bEnabled": str(call(ic.get_editor_property, "bEnabled")[1])})
    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX WALL SLAVE ENABLED done]")
