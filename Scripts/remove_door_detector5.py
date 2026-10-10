"""Learn the exact parameter types of DeleteSubobject(s) and test on the real asset."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_result5.json")
report = {}


def log(m):
    unreal.log("[DETDEL5] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return (traceback.format_exc().strip().splitlines()[-1][:400], None)


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

    # full argument-type diagnostics
    report["sig_delete_subobject"] = str(getattr(subsys.delete_subobject, "__doc__", ""))[:600]
    report["sig_delete_subobjects"] = str(getattr(subsys.delete_subobjects, "__doc__", ""))[:600]

    det_h, det_d = None, None
    for h in handles:
        v, d = var_of(subsys, flib, h)
        if v == "InteractionDetector":
            det_h, det_d = h, d
            break
    ctx_handle = handles[0]
    err_h, ctx_data = call(subsys.k2_find_subobject_data_from_handle, ctx_handle)
    # a handle derived from the context SubobjectData (if the lib exposes get_handle)
    ctx_alt = None
    if ctx_data is not None:
        e2, v2 = call(flib.get_handle, ctx_data)
        ctx_alt = v2 if e2 == "ok" else None
    report["ctx_alt_type"] = type(ctx_alt).__name__ if ctx_alt is not None else "None"

    report["attempts"] = []
    for label, fn, args in [
        ("delete_subobject(det_h, ctx_h)", subsys.delete_subobject, (det_h, ctx_handle)),
        ("delete_subobject(det_h, ctx_alt)", subsys.delete_subobject, (det_h, ctx_alt)),
        ("delete_subobject(ctx_h, det_h)", subsys.delete_subobject, (ctx_handle, det_h)),
        ("delete_subobject(det_d, ctx_h)", subsys.delete_subobject, (det_d, ctx_handle)),
        ("delete_subobject(det_d, ctx_d)", subsys.delete_subobject, (det_d, ctx_data)),
        ("delete_subobjects([det_h], ctx_alt)", subsys.delete_subobjects, ([det_h], ctx_alt)),
    ]:
        if args[1] is None:
            report["attempts"].append({"form": label, "err": "skipped (None arg)"})
            continue
        err = call(fn, *args)[0]
        now = bp_vars(subsys, flib, bp)
        report["attempts"].append({"form": label, "err": err, "vars": now})
        if not any(v == "InteractionDetector" for v in now):
            report["worked"] = label
            break

    report["vars_after_calls"] = bp_vars(subsys, flib, bp)
    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ground truth: unload the asset and read it back from disk
    report["unload"] = call(unreal.EditorAssetLibrary.unload_asset, BP_PATH)[0]
    bp2 = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    report["reloaded"] = None if bp2 is None else bp2.get_name()
    if bp2 is not None:
        h2 = list(subsys.k2_gather_subobject_data_for_blueprint(bp2))
        report["vars_from_disk"] = [var_of(subsys, flib, x)[0] for x in h2]


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s worked=%s" % (report.get("fatal"), report.get("worked")))
