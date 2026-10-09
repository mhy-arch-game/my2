"""Diagnose: (1) interaction detection on the door's back side, (2) why the door
frame blocks passage.

Read-only for the assets: the only mutation is turning DoorMesh collision off IN
MEMORY to isolate what else blocks the doorway; nothing is saved.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "door_diag_result.json")

CHAR_BPS = ["/Game/FirstPerson/BP_FirstPersonCharacter",
            "/Game/FirstPerson/BP_FirstPersonPlayerController",
            "/Game/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMECharacter"]

report = {}
world = None


def log(m):
    unreal.log("[DIAG] " + str(m))


def v3(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:130], None)


def vec_any(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def dump_mesh_components(actor):
    out = []
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        e = {"name": c.get_name()}
        err, sm = call(c.get_editor_property, "static_mesh")
        e["static_mesh"] = "<none>" if sm is None else sm.get_path_name()
        for meth in ("get_collision_enabled", "get_collision_profile_name",
                     "get_collision_object_type"):
            f = getattr(c, meth, None)
            if f is None:
                e[meth] = "<absent>"
                continue
            err2, val = call(f)
            e[meth] = str(val) if err2 == "ok" else err2
        wt = c.get_world_transform()
        e["world_loc"] = v3(wt.translation)
        err3, lb = call(c.get_local_bounds)
        if err3 == "ok":
            e["local_min"] = v3(lb[0])
            e["local_max"] = v3(lb[1])
        out.append(e)
    return out


def cast_ray(start, end, actors_to_ignore):
    err, hit = call(unreal.SystemLibrary.line_trace_single, world, start, end,
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, actors_to_ignore,
                    unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    return {
        "blocking": None if hit.actor is None else hit.actor.get_actor_label(),
        "impact": vec_any(hit.impact_point) if hit.actor is not None else None,
        "normal": vec_any(hit.impact_normal) if hit.actor is not None else None,
    }


def multi_trace(start, end, actors_to_ignore):
    err, hits = call(unreal.SystemLibrary.line_trace_multi, world, start, end,
                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, actors_to_ignore,
                     unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    return [{"actor": None if h.actor is None else h.actor.get_actor_label(),
             "t": round(float(h.time), 3)} for h in hits]


def capsule_overlap(center, radius, half_height, actors_to_ignore):
    types = [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY3]
    err, hits = call(unreal.SystemLibrary.box_overlap_actors, world, center,
                     unreal.Vector(radius, radius, half_height), unreal.Rotator(0, 0, 0),
                     types, False, actors_to_ignore, unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    seen = []
    for h in hits:
        lbl = None if h.actor is None else h.actor.get_actor_label()
        if lbl not in seen:
            seen.append(lbl)
    return seen


def main():
    global world
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    world = actor_ss.get_editor_world() if hasattr(actor_ss, "get_editor_world") else None
    if world is None:
        world = unreal.EditorLevelLibrary.get_editor_world()
    report["world"] = None if world is None else world.get_name()

    by_label = {}
    for a in actors:
        try:
            by_label[a.get_actor_label()] = a
        except Exception:
            pass

    # ---- 1. character-side detector settings -----------------------------
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for path in CHAR_BPS:
        bp = unreal.EditorAssetLibrary.load_asset(path)
        if bp is None:
            report.setdefault("char_bps", {})[path] = "<not found>"
            continue
        found = []
        try:
            for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
                err, data = call(subsys.k2_find_subobject_data_from_handle, h)
                if err != "ok":
                    continue
                err2, obj = call(flib.get_object, data)
                if err2 != "ok" or obj is None:
                    continue
                if not isinstance(obj, unreal.InteractionDetectorComponent):
                    continue
                e = {"component": obj.get_name()}
                for p in ("pick_mode", "interaction_radius", "trace_distance",
                          "b_require_facing", "min_facing_cosine", "b_draw_debug",
                          "b_pierce_occluders", "update_interval", "interact_key",
                          "b_register_interact_context", "b_apply_focus_outline",
                          "focus_stickiness_bonus"):
                    err3, val = call(obj.get_editor_property, p)
                    e[p] = str(val) if err3 == "ok" else err3
                err4, ia = call(obj.get_editor_property, "interact_action")
                e["interact_action"] = "<none>" if ia is None else ia.get_path_name()
                found.append(e)
        except Exception as exc:
            found.append({"err": repr(exc)[:130]})
        report.setdefault("char_bps", {})[path] = found

    # ---- 2. door / frame collision + bounds ------------------------------
    geo = {}
    for lbl in ("active_door", "men2", "men3", "menkuang1", "menkuang2", "menkuang3"):
        a = by_label.get(lbl)
        if a is None:
            geo[lbl] = "<absent>"
            continue
        err, bounds = call(a.get_actor_bounds, False, True)
        e = {
            "class": a.get_class().get_name(),
            "loc": v3(a.get_actor_location()),
            "rot": [round(float(a.get_actor_rotation().roll), 1),
                    round(float(a.get_actor_rotation().pitch), 1),
                    round(float(a.get_actor_rotation().yaw), 1)],
            "scale": v3(a.get_actor_scale3d()),
            "actor_bounds": None if err != "ok" else {"origin": v3(bounds[0]), "extent": v3(bounds[1])},
            "meshes": dump_mesh_components(a),
        }
        geo[lbl] = e
    report["geo"] = geo

    # ---- 3. what the crosshair hits from each face -----------------------
    door = by_label.get("men2") or by_label.get("active_door")
    if door is not None:
        loc = door.get_actor_location()
        fwd = unreal.MathLibrary.get_forward_vector(door.get_actor_rotation())
        up = unreal.Vector(0, 0, 1)
        eye = loc + up * 160.0
        ignores = [door]

        report["probe_door"] = door.get_actor_label()
        # from the front and from the back, at the door's centre height
        report["trace_front_to_back"] = cast_ray(eye - fwd * 300.0, eye + fwd * 300.0, ignores)
        report["trace_back_to_front"] = cast_ray(eye + fwd * 300.0, eye - fwd * 300.0, ignores)
        report["multi_front"] = multi_trace(eye - fwd * 300.0, eye + fwd * 300.0, ignores)
        # trace wide enough that the frame, if it is solid, must appear
        report["trace_front_wide"] = cast_ray(eye - fwd * 300.0, eye + fwd * 300.0, [])

        # ---- 4. is the doorway passable with the door panel out of the way?
        door_mesh = None
        for c in door.get_components_by_class(unreal.StaticMeshComponent):
            door_mesh = c
        saved = None
        if door_mesh is not None:
            saved = door_mesh.get_collision_enabled()
            door_mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        try:
            report["capsule_at_door_center"] = capsule_overlap(
                door.get_actor_location(), 42.0, 96.0, [door])
            report["trace_without_door"] = multi_trace(
                eye - fwd * 300.0, eye + fwd * 300.0, [])
        finally:
            if door_mesh is not None and saved is not None:
                door_mesh.set_collision_enabled(saved)
        report["door_mesh_collision_restored"] = None if saved is None else str(saved)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
