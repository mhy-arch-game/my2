"""Make the menkuang* door frames stop blocking the PLAYER (ECC_Pawn -> Ignore).

Only the Pawn response changes: the frame still blocks everything else, the pack asset
is untouched, and re-running with ECR_BLOCK reverts it.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_pawn_ignore3.json")
APPLY = True
report = {"apply": APPLY, "frames": []}


def log(m):
    unreal.log("[FRAMEPAWN3] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def read_pawn(comp):
    """Read the Pawn entry out of the component's BodyInstance collision responses."""
    err, bi = call(comp.get_editor_property, "body_instance")
    if err != "ok" or bi is None:
        return "ERR body_instance"
    arr = None
    for path in (("collision_responses",), ("response_array",)):
        err2, v = call(bi.get_editor_property, path[0])
        if err2 == "ok" and v is not None:
            arr = v
            break
    if arr is None:
        return "<no responses property>"
    try:
        for rc in arr:
            ch = rc.get_editor_property("channel")
            if str(ch) == "Pawn":
                return str(rc.get_editor_property("response"))
        return "<no Pawn entry>"
    except Exception as exc:
        return "read ERR " + repr(exc)[:90]


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    frames = []
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_actor_label().startswith("menkuang"):
                frames.append((a.get_actor_label(), a))
        except Exception:
            pass
    report["frame_count"] = len(frames)

    # diagnostics for one component
    _, first = frames[0]
    c0 = first.get_components_by_class(unreal.StaticMeshComponent)[0]
    err, bi = call(c0.get_editor_property, "body_instance")
    if err == "ok" and bi is not None:
        report["body_instance_props"] = sorted(
            [n for n in dir(bi) if "response" in n.lower() or "collision" in n.lower()])
    report["pawn_before"] = read_pawn(c0)

    changed = 0
    for lbl, a in frames:
        e = {"label": lbl}
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            e["pawn_before"] = read_pawn(c)
            if APPLY:
                err = call(c.set_collision_response_to_channel,
                           unreal.CollisionChannel.ECC_PAWN,
                           unreal.CollisionResponseType.ECR_IGNORE)[0]
                # Also push notify so the level save records the change.
                _, bi2 = call(c.get_editor_property, "body_instance")
                if bi2 is not None:
                    call(c.set_editor_property, "body_instance", bi2,
                         unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                e["set"] = err
                e["pawn_after"] = read_pawn(c)
                if err == "ok":
                    changed += 1
        report["frames"].append(e)
    report["changed"] = changed

    if APPLY:
        report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        check = []
        for a in actor_ss.get_all_level_actors():
            try:
                if not a.get_actor_label().startswith("menkuang"):
                    continue
            except Exception:
                continue
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                check.append({"label": a.get_actor_label(), "pawn": read_pawn(c)})
        report["after_reload"] = check


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s changed=%s" % (report.get("fatal"), report.get("changed")))
