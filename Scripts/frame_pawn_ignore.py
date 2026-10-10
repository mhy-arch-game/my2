"""Stop the menkuang* door frames from blocking the PLAYER.

Only the ECC_Pawn response is changed to Ignore: the frame still blocks everything
else, the pack asset is untouched, and the change is one script away from reverting.
Motivation: the door and frame sit on/inside a combined building mesh, and the frame
is the only doorway-local collider - if it seals the opening, no amount of swinging
the door helps.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_pawn_ignore_result.json")
APPLY = True
report = {"apply": APPLY, "frames": [], "failed": []}


def log(m):
    unreal.log("[FRAMEPAWN] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def pawn_response(comp):
    """Read the Pawn response through the BodyInstance (component getter may be absent)."""
    err, bi = call(comp.get_editor_property, "body_instance")
    if err != "ok" or bi is None:
        return "ERR body_instance"
    g = getattr(bi, "get_collision_response_to_channel", None)
    if g is None:
        return "<no getter>"
    err2, v = call(g, unreal.CollisionChannel.ECC_PAWN)
    return str(v) if err2 == "ok" else err2


def set_pawn_ignore(comp):
    """Set it through the component API and the BodyInstance, then push the struct back."""
    results = {}
    results["component_setter"] = call(
        comp.set_collision_response_to_channel,
        unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponse.ECR_IGNORE)[0]

    err, bi = call(comp.get_editor_property, "body_instance")
    if err == "ok" and bi is not None:
        s = getattr(bi, "set_collision_response_to_channel", None)
        if s is not None:
            results["body_setter"] = call(
                s, unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponse.ECR_IGNORE)[0]
            e2, _ = call(comp.set_editor_property, "body_instance", bi,
                         unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            results["body_pushback"] = e2
    return results


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    frames = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("menkuang"):
            frames.append((lbl, a))
    report["frame_count"] = len(frames)

    for lbl, a in frames:
        e = {"label": lbl}
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
        e["mesh_count"] = len(comps)
        for c in comps:
            e["pawn_before"] = pawn_response(c)
            if APPLY:
                e["setters"] = set_pawn_ignore(c)
                e["pawn_after_set"] = pawn_response(c)
        report["frames"].append(e)

    if APPLY:
        report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

        # ground truth: reload from disk and read again
        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        check = []
        for a in actor_ss.get_all_level_actors():
            try:
                lbl = a.get_actor_label()
            except Exception:
                continue
            if not lbl.startswith("menkuang"):
                continue
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                check.append({"label": lbl, "pawn_response": pawn_response(c)})
        report["after_reload"] = check


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
