"""Convert every remaining non-active_door men* actor to active_door_C (same treatment)."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_DOOR = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "extra_doors_result.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:160], None)


def v3(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def main():
    dbp = lib.load_asset(BP_DOOR)
    if dbp is None:
        report["error"] = "door blueprint not found"
        return
    door_class = dbp.generated_class()
    if door_class is None:
        report["error"] = "door blueprint has no generated class"
        return

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    targets, already = [], 0
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("men") or lbl.startswith("menkuang"):
            continue
        if a.get_class() == door_class:
            already += 1
            continue
        targets.append(a)

    report["already_doors"] = already
    report["to_convert"] = len(targets)

    converted = []
    for src in targets:
        e = {"label": src.get_actor_label()}
        try:
            e["class_before"] = src.get_class().get_name()
            xform = src.get_actor_transform()
            new_actor = actor_ss.spawn_actor_from_class(door_class, src.get_actor_location(),
                                                        src.get_actor_rotation())
            new_actor.set_actor_transform(xform, False, False)
            new_actor.set_actor_label(e["label"])
            actor_ss.destroy_actor(src)
            comp = new_actor.get_component_by_class(unreal.InteractableComponent)
            e["class_after"] = new_actor.get_class().get_name()
            if comp is not None:
                piv = comp.get_editor_property("RotationPivot")
                e["prompt"] = str(comp.get_editor_property("InteractionPrompt"))
                e["use_axis"] = comp.get_editor_property("bUseAxisRotation")
                e["pivot"] = v3(piv)
            converted.append(e)
        except Exception as exc:
            e["error"] = repr(exc)[:150]
            converted.append(e)
    report["converted"] = converted

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    err, _ = call(unreal.EditorLoadingAndSavingUtils.save_map, world, MAP)
    report["save"] = err


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
