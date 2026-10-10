"""Replace actors labelled with a given prefix by instances of a Blueprint class,
aligning the new instances' interaction parameters with a reference instance.

DRY RUN by default. Set APPLY = True to actually modify and save the level.
"""
import json
import os

import unreal

# ---------------------------------------------------------------- config
APPLY = True                        # <- flip to True only after review
MAP_PATH = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/bclass_source/active_door"
LABEL_PREFIXES = ["men"]            # scope: labels starting with these
EXCLUDE_PREFIXES = ["menkuang"]     # never touch these (door frames)
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "replace_doors_result.json")

# Instance parameters copied from the reference door onto every new instance.
INTERACTABLE_PROPS = [
    "bEnabled", "InteractionPrompt",
    "bUseBuiltInToggle", "ToggleComponentNames", "ToggleComponents",
    "ClosedRelativeTransform", "OpenRelativeTransform", "ToggleDuration",
    "bStartOpen", "bDisableCollisionWhenOpen",
    "bUseAxisRotation", "RotationAxis", "OpenAngleDegrees", "RotationPivot",
    "bToggleLights", "LightComponents", "LightComponentNames",
]

report = {"apply": APPLY, "reference": None, "replaced": [], "skipped": [], "failed": []}


def log(msg):
    unreal.log("[DOORREPLACE] " + str(msg))


def label_of(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return ""


def in_scope(label):
    if not label:
        return False
    for bad in EXCLUDE_PREFIXES:
        if label.startswith(bad):
            return False
    for good in LABEL_PREFIXES:
        if label.startswith(good):
            return True
    return False


def copy_interactable_props(src_actor, dst_actor):
    copied = []
    src = src_actor.get_component_by_class(unreal.InteractableComponent)
    dst = dst_actor.get_component_by_class(unreal.InteractableComponent)
    if src is None or dst is None:
        return copied
    for prop in INTERACTABLE_PROPS:
        try:
            value = src.get_editor_property(prop)
            dst.set_editor_property(prop, value)
            copied.append(prop)
        except Exception as exc:
            report["failed"].append({"prop": prop, "error": repr(exc)})
    return copied


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if bp is None:
        report["failed"].append("blueprint not found: " + BP_PATH)
        return
    door_class = bp.generated_class()
    if door_class is None:
        report["failed"].append("blueprint has no generated class (compile it)")
        return

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()

    # NOTE: a Blueprint-generated class is not a Python type, so isinstance()
    # raises TypeError. Compare UClass objects instead.
    reference = None
    for a in actors:
        if a.get_class() == door_class:
            reference = a
            break
    if reference is None:
        for a in actors:
            if a.is_a(door_class):
                reference = a
                break

    if reference is None:
        report["failed"].append("no existing active_door instance to use as reference")
        return

    report["reference"] = {
        "name": reference.get_name(),
        "label": label_of(reference),
        "transform": str(reference.get_actor_transform()),
    }

    targets = [a for a in actors if in_scope(label_of(a))]
    report["total_actors"] = len(actors)
    log("reference=%s, targets=%d" % (report["reference"]["label"], len(targets)))

    for a in targets:
        label = label_of(a)
        entry = {"label": label, "class": str(a.get_class().get_name())}
        try:
            xform = a.get_actor_transform()
            entry["transform"] = str(xform)
            if not APPLY:
                report["skipped"].append(entry)      # dry run
                continue

            new_actor = actor_ss.spawn_actor_from_class(
                door_class, a.get_actor_location(), a.get_actor_rotation())
            new_actor.set_actor_transform(xform, False, False)
            new_actor.set_actor_label(label)
            copied = copy_interactable_props(reference, new_actor)
            actor_ss.destroy_actor(a)
            entry["copied_props"] = copied
            report["replaced"].append(entry)
        except Exception as exc:
            entry["error"] = repr(exc)
            report["failed"].append(entry)

    if APPLY:
        unreal.EditorLoadingAndSavingUtils.save_current_level()
        log("level saved")

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    log("dry_run=%s targets=%d replaced=%d failed=%d" % (
        not APPLY, len(targets), len(report["replaced"]), len(report["failed"])))


main()
