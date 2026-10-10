"""Find the working signature for deleting a Blueprint SCS subobject, then delete it."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_result4.json")

report = {}


def log(m):
    unreal.log("[DETDEL4] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return ("ERR " + repr(exc)[:180], None)


def var_of(subsys, flib, handle):
    err, data = call(subsys.k2_find_subobject_data_from_handle, handle)
    if err != "ok":
        return None, None
    err2, val = call(flib.get_variable_name, data)
    return (str(val) if err2 == "ok" else None), data


def bp_vars(subsys, flib, bp):
    out = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        v, _ = var_of(subsys, flib, h)
        out.append(v)
    return out


def main():
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    ctx = handles[0]                      # root/CDO context handle
    report["vars_before"] = bp_vars(subsys, flib, bp)

    det_h = None
    det_d = None
    for h in handles:
        v, d = var_of(subsys, flib, h)
        if v == "InteractionDetector":
            det_h, det_d = h, d
            break
    report["detector_found"] = det_h is not None

    forms = [
        ("subobjects_handles_ctx", lambda: subsys.delete_subobjects([det_h], ctx)),
        ("subobjects_data_ctx", lambda: subsys.delete_subobjects([det_d], ctx)),
        ("subobjects_uarray_ctx", lambda: subsys.delete_subobjects(unreal.Array(unreal.SubobjectData, [det_d]), ctx)),
        ("subobject_handle_ctx", lambda: subsys.delete_subobject(det_h, ctx)),
        ("subobject_data_ctx", lambda: subsys.delete_subobject(det_d, ctx)),
        ("subobject_handle", lambda: subsys.delete_subobject(det_h)),
        ("subobjects_handles", lambda: subsys.delete_subobjects([det_h])),
    ]
    report["attempts"] = []
    for label, thunk in forms:
        err = call(thunk)[0]
        now = bp_vars(subsys, flib, bp)
        report["attempts"].append({"form": label, "err": err, "vars": now})
        if not any(v == "InteractionDetector" for v in now):
            report["worked"] = label
            break

    report["vars_after"] = bp_vars(subsys, flib, bp)
    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors = [a for a in actor_ss.get_all_level_actors() if a.get_class() == bp.generated_class()]
    report["door_count"] = len(doors)
    report["with_detector"] = sum(
        1 for d in doors if d.get_component_by_class(unreal.InteractionDetectorComponent) is not None)
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
log("done fatal=%s worked=%s" % (report.get("fatal"), report.get("worked")))
