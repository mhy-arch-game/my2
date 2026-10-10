"""Probe mesh collision complexity and locate the real player-side detector."""
import json
import os
import traceback

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "collision_probe_result.json")
report = {}


def log(m):
    unreal.log("[COLPROBE] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:140], None)


def mesh_info(path, key):
    sm = unreal.EditorAssetLibrary.load_asset(path)
    if sm is None:
        report[key] = "<not found>"
        return
    e = {"asset": sm.get_name()}
    # anything whose name mentions collision
    e["_names"] = sorted([p for p in dir(sm) if "collision" in p.lower()])
    for prop in ("collision_trace_flag", "body_setup", "complex_collision", "simple_collision"):
        err, val = call(sm.get_editor_property, prop)
        e[prop] = str(val) if err == "ok" else err
    # BodySetup carries the real trace flag
    err, bs = call(sm.get_editor_property, "body_setup")
    if err == "ok" and bs is not None:
        e["body_setup_type"] = type(bs).__name__
        e["body_setup_props"] = sorted([p for p in dir(bs) if "collision" in p.lower() or "trace" in p.lower()])
        err2, tf = call(bs.get_editor_property, "collision_trace_flag")
        e["body_setup_trace_flag"] = str(tf) if err2 == "ok" else err2
    report[key] = e


def main():
    report["EditorStaticMeshLibrary_collision_methods"] = sorted(
        [m for m in dir(unreal.EditorStaticMeshLibrary) if "collision" in m.lower()])
    mesh_info("/Game/Fab/Modern_Door/Door", "Door")
    mesh_info("/Game/Fab/Modern_Door/Door_Frame", "Door_Frame")

    # ---- locate the player-side InteractionDetectorComponent --------------
    assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
    bps = [a for a in assets if "FirstPerson" in a or "Character" in a or "GameMode" in a
           or "Controller" in a]
    report["candidate_assets"] = sorted([str(a).split(".")[-1] for a in bps])
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    found = {}
    for path in bps:
        if not path.endswith("_C"):
            asset = unreal.EditorAssetLibrary.load_asset(path)
        else:
            continue
        if asset is None or not isinstance(asset, unreal.Blueprint):
            continue
        comps = []
        try:
            for h in list(subsys.k2_gather_subobject_data_for_blueprint(asset)):
                err, data = call(subsys.k2_find_subobject_data_from_handle, h)
                if err != "ok":
                    continue
                err2, obj = call(flib.get_object, data)
                if err2 == "ok" and obj is not None:
                    comps.append([obj.get_name(), obj.get_class().get_name()])
        except Exception as exc:
            comps.append(["ERR", repr(exc)[:90]])
        found[path] = comps
    report["bp_components"] = found


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
