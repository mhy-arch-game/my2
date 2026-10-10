"""Read the meshes' simple collision primitives from BodySetup.AggGeom, plus the capsule size."""
import json
import os
import traceback

import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "agggeom_result.json")
report = {}


def log(m):
    unreal.log("[AGG] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def agg_of(path):
    sm = unreal.EditorAssetLibrary.load_asset(path)
    out = {"asset": None if sm is None else sm.get_name(), "meshes": []}
    err, bs = call(sm.get_editor_property, "body_setup")
    if err != "ok" or bs is None:
        out["body_setup"] = err
        return out
    err2, agg = call(bs.get_editor_property, "agg_geom")
    if err2 != "ok":
        out["agg_geom"] = err2
        return out
    for name in ("box_elems", "convex_elems", "sphere_elems", "sphyl_elems"):
        err3, arr = call(getattr, agg, name)
        if err3 != "ok":
            out[name] = err3
            continue
        try:
            out[name] = len(arr)
            if name == "box_elems":
                boxes = []
                for b in arr:
                    boxes.append({
                        "center": [round(float(b.center.x), 3), round(float(b.center.y), 3), round(float(b.center.z), 3)],
                        "extent": [round(float(b.box_extent.x), 3), round(float(b.box_extent.y), 3), round(float(b.box_extent.z), 3)],
                    })
                out["box_details"] = boxes
        except Exception as exc:
            out[name] = "len ERR " + repr(exc)[:80]
    return out


def main():
    report["Door"] = agg_of("/Game/Fab/Modern_Door/Door")
    report["Door_Frame"] = agg_of("/Game/Fab/Modern_Door/Door_Frame")

    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(CHAR_BP)
    caps = []
    if bp is not None:
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            err, data = call(subsys.k2_find_subobject_data_from_handle, h)
            if err != "ok":
                continue
            err2, obj = call(flib.get_object, data)
            if err2 != "ok" or obj is None:
                continue
            if isinstance(obj, unreal.CapsuleComponent):
                e = {"name": obj.get_name()}
                for meth in ("get_scaled_capsule_radius", "get_scaled_capsule_half_height",
                             "get_unscaled_capsule_radius", "get_unscaled_capsule_half_height"):
                    f = getattr(obj, meth, None)
                    err3, v = call(f) if f else ("<absent>", None)
                    e[meth] = str(v) if err3 == "ok" else err3
                caps.append(e)
    report["capsules"] = caps


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
