"""Delete the stray InteractionDetector. delete_subobject(s) needs an explicit context."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_result3.json")

report = {}


def log(m):
    unreal.log("[DETDEL3] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return ("ERR " + repr(exc)[:170], None)


def info(subsys, flib, handle):
    err, data = call(subsys.k2_find_subobject_data_from_handle, handle)
    if err != "ok":
        return None, None
    e = {}
    for name in ("get_display_name", "get_variable_name", "is_root_actor"):
        f = getattr(flib, name, None)
        if f is None:
            e[name] = "<absent>"
            continue
        err2, val = call(f, data)
        e[name] = str(val) if err2 == "ok" else err2
    return e, data


def bp_vars(subsys, flib, bp):
    out = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, _ = info(subsys, flib, h)
        out.append(str(e.get("get_variable_name", "?")) if e else "?")
    return out


def main():
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    report["vars_before"] = bp_vars(subsys, flib, bp)

    # ---- route 1: blueprint subobject deletion (with an explicit context) ----
    det_handle = None
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, _ = info(subsys, flib, h)
        if e and str(e.get("get_variable_name", "")) == "InteractionDetector":
            det_handle = h
            break
    report["bp_detector_found"] = det_handle is not None

    report["bp_delete_attempts"] = {}
    if det_handle is not None:
        report["bp_delete_attempts"]["delete_subobjects_ctx"] = call(
            subsys.delete_subobjects, [det_handle], bp)[0]
        report["vars_after_a"] = bp_vars(subsys, flib, bp)
        if any("InteractionDetector" in v for v in report["vars_after_a"]):
            report["bp_delete_attempts"]["delete_subobject_ctx"] = call(
                subsys.delete_subobject, det_handle, bp)[0]
            report["vars_after_b"] = bp_vars(subsys, flib, bp)

    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---- level ------------------------------------------------------------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors = [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]
    report["door_count"] = len(doors)
    report["instances_with_detector"] = sum(
        1 for d in doors if d.get_component_by_class(unreal.InteractionDetectorComponent) is not None)

    # ---- route 2 fallback: delete from each instance ----------------------
    if report["instances_with_detector"] > 0:
        cleaned = 0
        sample = None
        for d in doors:
            if d.get_component_by_class(unreal.InteractionDetectorComponent) is None:
                continue
            try:
                ihandles = list(subsys.k2_gather_subobject_data_for_instance(d))
            except Exception as exc:
                sample = "gather ERR " + repr(exc)[:120]
                break
            root = None
            det = None
            for ih in ihandles:
                e, idata = info(subsys, flib, ih)
                if e and str(e.get("is_root_actor", "")) == "True":
                    root = ih
                if e and str(e.get("get_variable_name", "")) == "InteractionDetector":
                    det = ih
            if root is None and ihandles:
                root = ihandles[0]
            if det is None:
                sample = "detector handle not found among " + str(len(ihandles))
                continue
            err = call(subsys.k2_delete_subobject_from_instance, root, det)[0]
            if sample is None:
                sample = err
            if d.get_component_by_class(unreal.InteractionDetectorComponent) is None:
                cleaned += 1
        report["instance_route_cleaned"] = cleaned
        report["instance_route_sample"] = sample

    report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    check = []
    for d in [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]:
        check.append({
            "label": d.get_actor_label(),
            "has_detector": d.get_component_by_class(unreal.InteractionDetectorComponent) is not None,
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
log("done fatal=%s" % report.get("fatal"))
