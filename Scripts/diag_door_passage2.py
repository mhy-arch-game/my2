"""Diagnose the doorway: what blocks the crosshair from each face, and what blocks passage.

Kismet trace helpers return either a HitResult or (bool, HitResult) - normalize both.
Nothing is saved; the door panel collision is toggled in memory only, then restored.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "door_diag_result2.json")
report = {}
world = None


def log(m):
    unreal.log("[DIAG2] " + str(m))


def v3(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def unwrap(res):
    """Kismet returns either the payload or (bool, payload)."""
    if isinstance(res, tuple):
        for item in res:
            if not isinstance(item, bool):
                return item
        return None
    return res


def hit_brief(hit):
    if hit is None:
        return None
    return {
        "actor": None if hit.actor is None else hit.actor.get_actor_label(),
        "comp": None if getattr(hit, "component", None) is None else hit.component.get_name(),
        "t": round(float(hit.time), 3),
        "dist": round(float(hit.distance), 1),
        "impact": v3(hit.impact_point),
        "normal": v3(hit.impact_normal),
    }


def trace(start, end, ignores=()):
    err, res = call(unreal.SystemLibrary.line_trace_single, world, start, end,
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, list(ignores),
                    unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    return hit_brief(unwrap(res))


def trace_multi(start, end, ignores=()):
    err, res = call(unreal.SystemLibrary.line_trace_multi, world, start, end,
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, list(ignores),
                    unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    hits = unwrap(res)
    if hits is None:
        return []
    return [hit_brief(h) for h in hits]


def overlap(center, radius, half_height, ignores=()):
    types = [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY3]
    err, res = call(unreal.SystemLibrary.box_overlap_actors, world, center,
                    unreal.Vector(radius, radius, half_height), unreal.Rotator(0, 0, 0),
                    types, False, list(ignores), unreal.DrawDebugTrace.NONE, True)
    if err != "ok":
        return {"err": err}
    hits = unwrap(res)
    seen = []
    for h in (hits or []):
        lbl = None if h.actor is None else h.actor.get_actor_label()
        if lbl not in seen:
            seen.append(lbl)
    return seen


def main():
    global world
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    report["world"] = None if world is None else world.get_name()

    by_label = {}
    for a in actor_ss.get_all_level_actors():
        try:
            by_label[a.get_actor_label()] = a
        except Exception:
            pass

    door = by_label.get("men2")
    frame = by_label.get("menkuang2")
    if door is None:
        report["fatal"] = "men2 not found"
        return
    report["door"] = door.get_actor_label()
    report["frame"] = None if frame is None else frame.get_actor_label()

    loc = door.get_actor_location()
    rot = door.get_actor_rotation()
    fwd = unreal.MathLibrary.get_forward_vector(rot)     # door local +X (thickness axis)
    up = unreal.Vector(0.0, 0.0, 1.0)
    eye = loc + up * 160.0

    # floor under the door
    report["floor_below"] = trace(eye, eye - up * 500.0, [door])
    report["floor_below_ignoring_all"] = trace(eye, eye - up * 500.0, [])

    # crosshair ray from both faces (door panel ignored -> what is left in the way?)
    report["front_ignore_door"] = trace(eye - fwd * 300.0, eye + fwd * 300.0, [door])
    report["back_ignore_door"] = trace(eye + fwd * 300.0, eye - fwd * 300.0, [door])
    report["front_multi_ignore_door"] = trace_multi(eye - fwd * 300.0, eye + fwd * 300.0, [door])
    report["back_multi_ignore_door"] = trace_multi(eye + fwd * 300.0, eye - fwd * 300.0, [door])
    report["front_multi_all"] = trace_multi(eye - fwd * 300.0, eye + fwd * 300.0, [])
    report["back_multi_all"] = trace_multi(eye + fwd * 300.0, eye - fwd * 300.0, [])

    # what does the crosshair hit on the door itself, per face?
    report["front_hit_door"] = trace(eye - fwd * 300.0, eye + fwd * 300.0, [])
    report["back_hit_door"] = trace(eye + fwd * 300.0, eye - fwd * 300.0, [])

    # ---- passage test: door panel collision off IN MEMORY ------------------
    door_mesh = None
    for c in door.get_components_by_class(unreal.StaticMeshComponent):
        door_mesh = c
    saved = None
    if door_mesh is not None:
        saved = door_mesh.get_collision_enabled()
        door_mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    try:
        report["capsule_at_door_center_no_door"] = overlap(loc + up * 100.0, 42.0, 96.0, [door])
        report["multi_no_door"] = trace_multi(eye - fwd * 300.0, eye + fwd * 300.0, [])
        if frame is not None:
            floc = frame.get_actor_location()
            fwd2 = unreal.MathLibrary.get_forward_vector(frame.get_actor_rotation())
            report["capsule_at_frame_center_no_door"] = overlap(floc + up * 100.0, 42.0, 96.0, [door])
    finally:
        if door_mesh is not None and saved is not None:
            door_mesh.set_collision_enabled(saved)
    report["door_collision_restored"] = None if saved is None else str(saved)

    # frame-only: ignore the door AND trace through the frame plane
    report["frame_slab_test"] = trace(eye - fwd * 400.0, eye + fwd * 400.0, [door])
    report["frame_mesh_has_simple_collision"] = None
    if frame is not None:
        fm = None
        for c in frame.get_components_by_class(unreal.StaticMeshComponent):
            fm = c
        if fm is not None:
            err, sm = call(fm.get_editor_property, "static_mesh")
            if err == "ok" and sm is not None:
                report["frame_mesh"] = sm.get_path_name()
                report["frame_mesh_complex_collision"] = str(sm.get_editor_property("complex_collision"))


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
