"""Probe collision enum names + verify the level is still intact."""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "enum_and_integrity.json")
report = {}


def log(m):
    unreal.log("[ENUMCHK] " + str(m))


def main():
    report["CollisionResponse_values"] = [v for v in dir(unreal.CollisionResponse) if not v.startswith("_")]
    report["CollisionChannel_values"] = [v for v in dir(unreal.CollisionChannel) if not v.startswith("_")]
    report["CollisionEnabled_values"] = [v for v in dir(unreal.CollisionEnabled) if not v.startswith("_")]

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    door_class = bp.generated_class() if bp else None

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["total_actors"] = len(actors)

    doors = []
    frames = 0
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("menkuang"):
            frames += 1
            continue
        if door_class is not None and a.get_class() == door_class:
            comp = a.get_component_by_class(unreal.InteractableComponent)
            piv = comp.get_editor_property("RotationPivot") if comp else None
            doors.append({
                "label": lbl,
                "use_axis": None if comp is None else comp.get_editor_property("bUseAxisRotation"),
                "pivot": None if piv is None else "{:.2f},{:.2f},{:.2f}".format(piv.x, piv.y, piv.z),
                "has_detector": a.get_component_by_class(unreal.InteractionDetectorComponent) is not None,
            })
    report["markers"] = {"marker": "active_door"}
    report["door_count"] = len(doors)
    report["frame_count"] = frames
    report["doors"] = doors


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
