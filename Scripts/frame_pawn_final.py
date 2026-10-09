"""Make the menkuang* frames ignore ECC_Pawn - corrected.

Do NOT read body_instance and write it back: get_editor_property returns a COPY, so
pushing it back silently reverts the response the setter just applied.
"""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_pawn_final.json")
APPLY = True
report = {"apply": APPLY}


def log(m):
    unreal.log("[PAWNFIX] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def pawn(comp):
    err, v = call(comp.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)
    return str(v) if err == "ok" else err


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

    before = {}
    changed = 0
    for a in frames:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            before[a.get_actor_label()] = pawn(c)
            if APPLY:
                err = call(c.set_collision_response_to_channel,
                           unreal.CollisionChannel.ECC_PAWN,
                           unreal.CollisionResponseType.ECR_IGNORE)[0]
                if err == "ok":
                    changed += 1
    report["pawn_before"] = before
    report["changed"] = changed

    # in-session read-back BEFORE saving
    now = {}
    for a in frames:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            now[a.get_actor_label()] = pawn(c)
    report["pawn_after_set"] = now

    if APPLY:
        report["save_level"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        disk = {}
        profiles = set()
        for a in actor_ss.get_all_level_actors():
            try:
                if not a.get_actor_label().startswith("menkuang"):
                    continue
            except Exception:
                continue
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                disk[a.get_actor_label()] = pawn(c)
                _, prof = call(c.get_collision_profile_name)
                profiles.add(str(prof))
        report["pawn_from_disk"] = disk
        report["profiles_from_disk"] = sorted(profiles)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s changed=%s" % (report.get("fatal"), report.get("changed")))
