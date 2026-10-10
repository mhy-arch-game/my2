"""Decisive data: collision complexity, detector settings, door/frame alignment."""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "final_diag_result.json")
report = {}


def log(m):
    unreal.log("[FINDIAG] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:130], None)


def v3(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def main():
    esml = unreal.EditorStaticMeshLibrary
    # ---- 1. collision complexity / primitive counts ----------------------
    meshes = {}
    for path in ("/Game/Fab/Modern_Door/Door", "/Game/Fab/Modern_Door/Door_Frame"):
        sm = unreal.EditorAssetLibrary.load_asset(path)
        e = {}
        for meth, args in (("get_collision_complexity", (sm, 0)),
                           ("get_simple_collision_count", (sm, 0)),
                           ("get_convex_collision_count", (sm, 0))):
            f = getattr(esml, meth, None)
            err, val = call(f, *args) if f else ("<absent>", None)
            e[meth] = str(val) if err == "ok" else err
        meshes[path.split("/")[-1]] = e
    report["meshes"] = meshes

    # ---- 2. detector settings on the real pawn ---------------------------
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = unreal.EditorAssetLibrary.load_asset(CHAR_BP)
    report["char_bp"] = None if bp is None else bp.get_name()
    detectors = []
    if bp is not None:
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            err, data = call(subsys.k2_find_subobject_data_from_handle, h)
            if err != "ok":
                continue
            err2, obj = call(flib.get_object, data)
            if err2 != "ok" or obj is None:
                continue
            if not isinstance(obj, unreal.InteractionDetectorComponent):
                continue
            e = {}
            for prop in ("pick_mode", "interaction_radius", "trace_distance", "b_require_facing",
                         "min_facing_cosine", "b_draw_debug", "b_pierce_occluders",
                         "update_interval", "interact_key", "b_register_interact_context",
                         "b_apply_focus_outline", "focus_stickiness_bonus", "probe_object_types"):
                err3, val = call(obj.get_editor_property, prop)
                e[prop] = str(val) if err3 == "ok" else err3
            err4, ia = call(obj.get_editor_property, "interact_action")
            e["interact_action"] = "<none>" if ia is None else ia.get_name()
            detectors.append(e)
    report["detectors"] = detectors

    # ---- 3. door / frame alignment across the level ----------------------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors, frames = {}, {}
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if "men" not in lbl and "active_door" not in lbl:
            continue
        entry = {
            "loc": v3(a.get_actor_location()),
            "yaw": round(float(a.get_actor_rotation().yaw), 1),
            "scale": v3(a.get_actor_scale3d()),
        }
        err, b = call(a.get_actor_bounds, False, False)
        if err == "ok":
            entry["bounds_extent"] = v3(b[1])
        if lbl.startswith("menkuang"):
            frames[lbl] = entry
        else:
            door = a
            comp = door.get_component_by_class(unreal.InteractableComponent)
            if comp is not None:
                entry["bEnabled"] = comp.get_editor_property("bEnabled")
                entry["bUseBuiltInToggle"] = comp.get_editor_property("bUseBuiltInToggle")
                entry["bDisableCollisionWhenOpen"] = comp.get_editor_property("bDisableCollisionWhenOpen")
                entry["bUseAxisRotation"] = comp.get_editor_property("bUseAxisRotation")
                entry["ToggleComponentNames"] = [str(n) for n in (comp.get_editor_property("ToggleComponentNames") or [])]
                piv = comp.get_editor_property("RotationPivot")
                entry["pivot"] = "{:.2f},{:.2f},{:.2f}".format(piv.x, piv.y, piv.z)
            doors[lbl] = entry
    report["doors"] = doors
    report["frames"] = frames

    # nearest frame for each door (index order men2..men17 <-> menkuang2..17)
    pairs = []
    for lbl, d in sorted(doors.items()):
        suffix = "".join(ch for ch in lbl if ch.isdigit())
        flbl = "menkuang" + suffix if suffix else "menkuang1"
        f = frames.get(flbl)
        row = {"door": lbl, "frame": flbl,
               "door_loc": d["loc"], "frame_loc": None if f is None else f["loc"],
               "door_yaw": d["yaw"], "frame_yaw": None if f is None else f["yaw"],
               "door_extent": d.get("bounds_extent"), "frame_extent": None if f is None else f.get("bounds_extent")}
        if f is not None:
            row["delta"] = [round(d["loc"][i] - f["loc"][i], 2) for i in range(3)]
        pairs.append(row)
    report["pairs"] = pairs


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
