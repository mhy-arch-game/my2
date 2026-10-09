"""Delete the stray InteractionDetector from the door Blueprint and clean the level.

Identify subobjects by get_variable_name (get_class is not a library function; it is a
UObject descriptor and raises on a SubobjectData).
"""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_result2.json")

ROT_AXIS = (0.0, 0.0, 1.0)
report = {}


def log(m):
    unreal.log("[DETDEL2] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return ("ERR " + repr(exc)[:160], None)


def info(subsys, flib, handle):
    err, data = call(subsys.k2_find_subobject_data_from_handle, handle)
    if err != "ok":
        return None, None, err
    e = {}
    for name in ("get_display_name", "get_variable_name", "get_object", "is_component", "can_delete"):
        f = getattr(flib, name, None)
        if f is None:
            e[name] = "<absent>"
            continue
        err2, val = call(f, data)
        e[name] = str(val) if err2 == "ok" else err2
    return e, data, "ok"


def main():
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    entries = []
    datas = []
    for h in handles:
        e, d, err = info(subsys, flib, h)
        entries.append(e or {"err": err})
        datas.append(d)
    report["bp_entries"] = entries

    def find(var):
        for i, e in enumerate(entries):
            if str(e.get("get_variable_name", "")) == var:
                return i
        return None

    det_i = find("InteractionDetector")
    int_i = find("Interactable")
    mesh_i = find("DoorMesh")
    report["indices"] = {"detector": det_i, "interactable": int_i, "mesh": mesh_i}

    # ---- pivot from the DoorMesh template bounds --------------------------
    pivot = [0.0, -21.828739, 0.0]
    if mesh_i is not None:
        err, mesh = call(flib.get_object, datas[mesh_i])
        if err == "ok" and mesh is not None:
            lmin, lmax = mesh.get_local_bounds()
            lo = [float(lmin.x), float(lmin.y), float(lmin.z)]
            hi = [float(lmax.x), float(lmax.y), float(lmax.z)]
            ext = [hi[k] - lo[k] for k in range(3)]
            idx = 0 if ext[0] >= ext[1] else 1
            pivot = [0.0, 0.0, 0.0]
            pivot[idx] = lo[idx]
            report["pivot_source"] = "DoorMesh template bounds"
            report["bounds"] = {"min": lo, "max": hi, "extent": ext}
    report["pivot"] = pivot

    # ---- hinge on the BP component template ------------------------------
    if int_i is not None:
        err, tpl = call(flib.get_object, datas[int_i])
        report["interactable_template"] = err if err != "ok" else str(tpl)
        if err == "ok" and tpl is not None:
            try:
                tpl.set_editor_property("bUseAxisRotation", True)
                tpl.set_editor_property("RotationAxis", unreal.Vector(*ROT_AXIS))
                tpl.set_editor_property("RotationPivot", unreal.Vector(*pivot),
                                        unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                report["bp_hinge_readback"] = str(tpl.get_editor_property("RotationPivot"))
            except Exception as exc:
                report["bp_hinge_err"] = repr(exc)[:160]

    # ---- delete the detector subobject -----------------------------------
    if det_i is not None:
        h = handles[det_i]
        report["delete_attempts"] = {}
        report["delete_attempts"]["delete_subobjects"] = call(subsys.delete_subobjects, [h])[0]
        left = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
        report["handles_after_1"] = len(left)
        report["vars_after_1"] = [str(info(subsys, flib, x)[0].get("get_variable_name", "?")) for x in left]
        if any("InteractionDetector" in v for v in report["vars_after_1"]):
            report["delete_attempts"]["delete_subobject"] = call(subsys.delete_subobject, h)[0]
            left = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
            report["handles_after_2"] = len(left)
            report["vars_after_2"] = [str(info(subsys, flib, x)[0].get("get_variable_name", "?")) for x in left]
    else:
        report["delete_attempts"] = "detector subobject not found"

    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---- level: check whether instances dropped the component ------------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors = [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]
    report["door_count"] = len(doors)
    report["instance_detector_after_bp_edit"] = sum(
        1 for d in doors if d.get_component_by_class(unreal.InteractionDetectorComponent) is not None)

    # per-instance removal, if the blueprint edit did not propagate
    removed = 0
    if report["instance_detector_after_bp_edit"] > 0:
        report["instance_delete_tries"] = {}
        for d in doors:
            if d.get_component_by_class(unreal.InteractionDetectorComponent) is None:
                continue
            try:
                ihandles = list(subsys.k2_gather_subobject_data_for_instance(d))
            except Exception as exc:
                report["instance_delete_tries"]["gather_err"] = repr(exc)[:140]
                break
            for ih in ihandles:
                e, idata, _ = info(subsys, flib, ih)
                if e and str(e.get("get_variable_name", "")) == "InteractionDetector":
                    report["instance_delete_tries"]["delete_from_instance_a"] = call(
                        subsys.k2_delete_subobject_from_instance, ih, d)[0]
                    if d.get_component_by_class(unreal.InteractionDetectorComponent) is not None:
                        report["instance_delete_tries"]["delete_from_instance_b"] = call(
                            subsys.k2_delete_subobject_from_instance, d, ih)[0]
                    if d.get_component_by_class(unreal.InteractionDetectorComponent) is None:
                        removed += 1
                    break
    report["instances_cleaned"] = removed

    # ---- hinge on every instance + save ----------------------------------
    for d in doors:
        comp = d.get_component_by_class(unreal.InteractableComponent)
        if comp is not None:
            comp.set_editor_property("bUseAxisRotation", True)
            comp.set_editor_property("RotationAxis", unreal.Vector(*ROT_AXIS))
            comp.set_editor_property("RotationPivot", unreal.Vector(*pivot),
                                     unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

    # ---- reload + verify -------------------------------------------------
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
log("done fatal=%s" % report.get("fatal"))
