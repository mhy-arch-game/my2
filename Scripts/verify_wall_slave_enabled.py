import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_wall_slave_enabled.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("jiguanqiang"):
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        rc = a.get_component_by_class(unreal.InteractionOperationReceiverComponent)
        rows.append({"label": lbl,
                     "bEnabled": str(call(ic.get_editor_property, "bEnabled")[1]) if ic else "no interactable",
                     "receiver_bEnabled": str(call(rc.get_editor_property, "bEnabled")[1]) if rc else "no receiver",
                     "channel": str(call(rc.get_editor_property, "Channel")[1]) if rc else "-"})
    rows.sort(key=lambda r: r["label"])
    report["walls_from_disk"] = rows
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY WALL SLAVE done]")
