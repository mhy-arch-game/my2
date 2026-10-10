"""Measure the door frame's simple collision and verify the new detector properties."""
import json
import os
import traceback

import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_collision_result.json")
report = {}


def log(m):
    unreal.log("[FRAMECOL] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    esml = unreal.EditorStaticMeshLibrary
    out = {}
    for path in ("/Game/Fab/Modern_Door/Door", "/Game/Fab/Modern_Door/Door_Frame"):
        sm = unreal.EditorAssetLibrary.load_asset(path)
        e = {"asset": None if sm is None else sm.get_name()}
        for meth in ("get_collision_complexity", "get_simple_collision_count",
                     "get_convex_collision_count"):
            f = getattr(esml, meth, None)
            if f is None:
                e[meth] = "<absent>"
                continue
            err, val = call(f, sm)
            e[meth] = str(val) if err == "ok" else err
        err, bs = call(sm.get_editor_property, "body_setup")
        if err == "ok" and bs is not None:
            for prop in ("collision_trace_flag", "collision_rep_type", "agg_geom"):
                err2, v = call(bs.get_editor_property, prop)
                e["body_" + prop] = str(v) if err2 == "ok" else "ERR"
        err3, lb = call(sm.get_lod_for_collision) if hasattr(sm, "get_lod_for_collision") else ("n/a", None)
        out[path.split("/")[-1]] = e
    report["meshes"] = out

    # verify the new detector properties are reflected
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(CHAR_BP)
    props = {}
    if bp is not None:
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            err, data = call(subsys.k2_find_subobject_data_from_handle, h)
            if err != "ok":
                continue
            err2, obj = call(flib.get_object, data)
            if err2 != "ok" or obj is None or not isinstance(obj, unreal.InteractionDetectorComponent):
                continue
            for prop in ("bPierceOccluders", "bFallbackToOverlap", "bRequireFacing", "bDrawDebug",
                         "PickMode", "TraceDistance", "InteractionRadius", "MinFacingCosine"):
                err3, v = call(obj.get_editor_property, prop)
                props[prop] = str(v) if err3 == "ok" else err3
            # python-style names as a cross-check
            for prop in ("pierce_occluders", "fallback_to_overlap"):
                err3, v = call(obj.get_editor_property, prop)
                props["py:" + prop] = str(v) if err3 == "ok" else err3
    report["detector_props"] = props


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
