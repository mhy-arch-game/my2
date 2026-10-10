"""Delete the stray player-side InteractionDetector from the door Blueprint (and from
the level instances), and pin the hinge on the Blueprint component template.

NOTE: SubobjectDataBlueprintFunctionLibrary takes a SubobjectData (from
subsys.k2_find_subobject_data_from_handle), NOT a SubobjectDataHandle - passing the
handle raises "Failed to convert parameter 'data'".
"""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "door_detector_removal_result.json")

ROT_AXIS = (0.0, 0.0, 1.0)

report = {}


def log(m):
    unreal.log("[DETDEL] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return ("ERR " + repr(exc)[:150], None)


def describe(subsys, flib, handle):
    e = {}
    err, data = call(subsys.k2_find_subobject_data_from_handle, handle)
    if err != "ok":
        e["data_err"] = err
        return e, None
    for name in ("get_display_name", "get_variable_name", "get_class", "get_object",
                 "is_component", "is_inherited_component", "can_delete", "can_edit"):
        f = getattr(flib, name, None)
        if f is None:
            e[name] = "<absent>"
            continue
        err2, val = call(f, data)
        e[name] = str(val) if err2 == "ok" else err2
    return e, data


def main():
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    report["bp_handle_count"] = len(handles)
    entries = []
    datas = []
    for h in handles:
        e, d = describe(subsys, flib, h)
        entries.append(e)
        datas.append(d)
    report["bp_entries"] = entries

    det_i = None
    int_i = None
    mesh_i = None
    for i, e in enumerate(entries):
        cls = str(e.get("get_class", ""))
        if "InteractionDetector" in cls and det_i is None:
            det_i = i
        elif cls.endswith("InteractableComponent") and int_i is None:
            int_i = i
        elif cls.endswith("StaticMeshComponent") and mesh_i is None:
            mesh_i = i
    report["det_i"] = det_i
    report["int_i"] = int_i
    report["mesh_i"] = mesh_i

    # ---- hinge pivot from the template mesh bounds ------------------------
    pivot = [0.0, -21.828739, 0.0]
    if mesh_i is not None:
        err, mesh = call(flib.get_object, datas[mesh_i])
        if err == "ok" and mesh is not None:
            try:
                lmin, lmax = mesh.get_local_bounds()
                lo = [float(lmin.x), float(lmin.y), float(lmin.z)]
                hi = [float(lmax.x), float(lmax.y), float(lmax.z)]
                ext = [hi[k] - lo[k] for k in range(3)]
                idx = 0 if ext[0] >= ext[1] else 1
                pivot = [0.0, 0.0, 0.0]
                pivot[idx] = lo[idx]
                report["pivot_source"] = "template mesh bounds"
            except Exception as exc:
                report["pivot_source"] = "fallback (" + repr(exc)[:80] + ")"
    report["pivot"] = pivot

    # ---- hinge on the BP template ---------------------------------------
    if int_i is not None:
        err, tpl = call(flib.get_object, datas[int_i])
        report["int_template"] = err if err != "ok" else str(tpl)
        if err == "ok" and tpl is not None:
            try:
                tpl.set_editor_property("bUseAxisRotation", True)
                tpl.set_editor_property("RotationAxis", unreal.Vector(*ROT_AXIS))
                tpl.set_editor_property("RotationPivot", unreal.Vector(*pivot),
                                        unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                report["bp_hinge"] = str(tpl.get_editor_property("RotationPivot"))
            except Exception as exc:
                report["bp_hinge_err"] = repr(exc)[:140]

    # ---- delete the stale detector from the Blueprint --------------------
    if det_i is not None:
        h = handles[det_i]
        report["det_data"] = entries[det_i]
        report["del_delete_subobjects"] = call(subsys.delete_subobjects, [h])[0]
        left = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
        report["bp_handle_count_after"] = len(left)
        report["bp_classes_after"] = [describe(subsys, flib, x)[0].get("get_class", "?") for x in left]
        if report["bp_handle_count_after"] == len(handles):
            report["del_delete_subobject"] = call(subsys.delete_subobject, h)[0]
            left = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
            report["bp_handle_count_after2"] = len(left)
            report["bp_classes_after2"] = [describe(subsys, flib, x)[0].get("get_class", "?") for x in left]
    else:
        report["det_data"] = "InteractionDetector subobject not identified"

    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---- clean the level instances ---------------------------------------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors = [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]
    report["door_count"] = len(doors)
    inst = []
    for d in doors:
        e = {"label": d.get_actor_label()}
        e["detector_after_bp_edit"] = d.get_component_by_class(unreal.InteractionDetectorComponent) is not None
        comp = d.get_component_by_class(unreal.InteractableComponent)
        if comp is not None:
            try:
                comp.set_editor_property("bUseAxisRotation", True)
                comp.set_editor_property("RotationAxis", unreal.Vector(*ROT_AXIS))
                comp.set_editor_property("RotationPivot", unreal.Vector(*pivot),
                                         unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            except Exception as exc:
                e["write_err"] = repr(exc)[:120]
        inst.append(e)
    report["instances"] = inst
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
            "pivot": None if comp is None else str(comp.get_editor_property("RotationPivot")),
            "use_axis": None if comp is None else comp.get_editor_property("bUseAxisRotation"),
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
