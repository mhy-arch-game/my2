"""Set IgnoreOnlyPawn on the frames AND mark the change properly.

save_current_level() was a no-op last time (file mtime unchanged), i.e. the write did
not dirty the level. Re-push the BodyInstance with notify ALWAYS after the change - the
same trick that made the hinge pivot survive a reload.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_profile2.json")
PROFILE = "IgnoreOnlyPawn"


def log(m):
    unreal.log("[FRAMEPROF2] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def state(comp):
    out = {}
    err, prof = call(comp.get_collision_profile_name)
    out["profile"] = str(prof) if err == "ok" else err
    err2, v = call(comp.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)
    out["pawn"] = str(v) if err2 == "ok" else err2
    return out


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    frames = []
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_actor_label().startswith("menkuang"):
                frames.append(a)
        except Exception:
            pass
    report["frame_count"] = len(frames)

    # what mark-dirty APIs exist?
    report["level_lib_dirty"] = [m for m in dir(unreal.EditorLevelLibrary) if "dirty" in m.lower()]
    report["eas_dirty"] = [m for m in dir(unreal.EditorLoadingAndSavingUtils) if "dirty" in m.lower() or "save" in m.lower()]

    changed = 0
    for a in frames:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if call(c.set_collision_profile_name, PROFILE)[0] == "ok":
                changed += 1
            # push the (already updated) BodyInstance back with a hard notify
            _, bi = call(c.get_editor_property, "body_instance")
            if bi is not None:
                call(c.set_editor_property, "body_instance", bi,
                     unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    report["changed"] = changed
    first = frames[0].get_components_by_class(unreal.StaticMeshComponent)[0]
    report["after_set"] = state(first)

    # try to force a dirty flag on the actor + component
    report["modify_actor"] = call(frames[0].modify)[0]
    report["modify_comp"] = call(first.modify)[0]

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    err, saved = call(unreal.EditorLoadingAndSavingUtils.save_map, world, MAP_PATH)
    report["save_map"] = err if err != "ok" else str(saved)

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            if not a.get_actor_label().startswith("menkuang"):
                continue
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            s = state(c)
            rows.append(s)
    report["from_disk"] = rows


report = {}
try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
