"""Last attempt: obtain a real Blueprint/SCS context handle for delete_subobject."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "detector_removal_last.json")
report = {}


def log(m):
    unreal.log("[DETDEL8] " + str(m))


def call(fn, *args):
    try:
        return ("ok", fn(*args))
    except Exception as exc:
        return (repr(exc)[:200], None)


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
    report["doc_acquire_editor_element_handle"] = str(getattr(subsys.acquire_editor_element_handle, "__doc__", ""))[:500]
    report["doc_find_handle_for_object"] = str(getattr(subsys.find_handle_for_object, "__doc__", ""))[:500]

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    report["vars_before"] = bp_vars(subsys, flib, bp)

    det_h = None
    for h in handles:
        v, _ = var_of(subsys, flib, h)
        if v == "InteractionDetector":
            det_h = h
    if det_h is None:
        report["detector_found"] = False
        return

    report["attempts"] = []

    def attempt(label, ctx):
        if ctx is None:
            report["attempts"].append({"form": label, "err": "None ctx"})
            return False
        err, count = call(subsys.delete_subobject, ctx, det_h)
        now = bp_vars(subsys, flib, bp)
        report["attempts"].append({"form": label, "err": err, "count": count, "vars": now})
        return not any(v == "InteractionDetector" for v in now)

    # 1) handle acquired for the blueprint object itself
    for label, thunk in [
        ("acquire(bp)", lambda: subsys.acquire_editor_element_handle(bp)),
        ("find_handle_for_object(bp, bp)", lambda: subsys.find_handle_for_object(bp, bp)),
    ]:
        err, val = call(thunk)
        report["attempts"].append({"form": label + " -> ctx", "err": err, "count": "-"})
        if err == "ok" and val is not None:
            if attempt(label + " as ctx", val):
                report["worked_with"] = label
                break

    report["vars_after"] = bp_vars(subsys, flib, bp)
    report["compile_bp"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s worked=%s" % (report.get("fatal"), report.get("worked_with")))
