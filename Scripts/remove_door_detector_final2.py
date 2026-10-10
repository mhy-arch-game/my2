"""Find the right context handle for deleting the InteractionDetector subobject.

delete_subobject(context_handle, subobject_to_delete, bp_context=None) -> int32
The delete TARGET is always the detector, only the context varies, so trying every
candidate cannot remove anything else (a wrong context returns 0).
"""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_final2.json")
report = {}


def log(m):
    unreal.log("[DETDEL7] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return (repr(exc)[:160], None)


def var_of(subsys, flib, h):
    err, data = call(subsys.k2_find_subobject_data_from_handle, h)
    if err != "ok":
        return None, None
    err2, v = call(flib.get_variable_name, data)
    return (str(v) if err2 == "ok" else None), data


def bp_vars(subsys, flib, bp):
    return [var_of(subsys, flib, h)[0] for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp))]


def main():
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    report["vars_before"] = bp_vars(subsys, flib, bp)

    det_h, det_d = None, None
    for h in handles:
        v, d = var_of(subsys, flib, h)
        if v == "InteractionDetector":
            det_h, det_d = h, d
    report["detector_found"] = det_h is not None
    if det_h is None:
        return

    # candidate contexts: every gathered handle + the detector's parent node
    contexts = [("h[%d]" % i, h) for i, h in enumerate(handles)]
    for name in ("get_parent_handle", "get_handle"):
        f = getattr(flib, name, None)
        if f is None:
            continue
        err, val = call(f, det_d)
        if err == "ok" and val is not None:
            contexts.append((name, val))

    report["attempts"] = []
    for label, ctx in contexts:
        if ctx is None:
            continue
        err, count = call(subsys.delete_subobject, ctx, det_h)
        now = bp_vars(subsys, flib, bp)
        report["attempts"].append({"ctx": label, "err": err, "count": count, "vars": now})
        if not any(v == "InteractionDetector" for v in now):
            report["worked_with"] = label
            report["deleted_count"] = count
            break

    report["vars_after"] = bp_vars(subsys, flib, bp)
    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors = [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]
    report["door_count"] = len(doors)
    report["with_detector_before_save"] = sum(
        1 for d in doors if d.get_component_by_class(unreal.InteractionDetectorComponent) is not None)
    report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    check = []
    for d in [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]:
        comp = d.get_component_by_class(unreal.InteractableComponent)
        check.append({
            "label": d.get_actor_label(),
            "has_detector": d.get_component_by_class(unreal.InteractionDetectorComponent) is not None,
            "use_axis": None if comp is None else comp.get_editor_property("bUseAxisRotation"),
            "pivot": None if comp is None else "{:.4f},{:.4f},{:.4f}".format(
                comp.get_editor_property("RotationPivot").x,
                comp.get_editor_property("RotationPivot").y,
                comp.get_editor_property("RotationPivot").z),
            "components": [c.get_name() for c in d.get_components_by_class(unreal.ActorComponent)],
        })
    report["after_reload"] = check


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s worked=%s" % (report.get("fatal"), report.get("worked_with")))
