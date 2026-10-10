"""Find how to express 'Ignore' for a collision response in this Python build, then apply."""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_pawn_ignore2.json")
APPLY = True
report = {"apply": APPLY}


def log(m):
    unreal.log("[FRAMEPAWN2] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def response_reader(comp):
    err, bi = call(comp.get_editor_property, "body_instance")
    if err != "ok" or bi is None:
        return None, "ERR " + err
    return bi, None


def main():
    # ---- 1. locate an 'Ignore' enum value -------------------------------
    names = [n for n in dir(unreal) if "esponse" in n or "ECR" in n]
    report["candidate_names"] = names
    found = {}
    for n in names:
        obj = getattr(unreal, n, None)
        if obj is None:
            continue
        vals = [v for v in dir(obj) if "IGNORE" in v.upper() or "OVERLAP" in v.upper() or "BLOCK" in v.upper()]
        if vals:
            found[n] = vals
    report["enum_with_values"] = found

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    frames = [(a.get_actor_label(), a) for a in actor_ss.get_all_level_actors()
              if a.get_actor_label().startswith("menkuang")]
    report["frame_count"] = len(frames)
    if not frames:
        return
    _, first = frames[0]
    comp = first.get_components_by_class(unreal.StaticMeshComponent)[0]

    # ---- 2. what can read the Pawn response? ----------------------------
    bi, bi_err = response_reader(comp)
    report["body_instance"] = bi_err if bi_err else type(bi).__name__
    reader = None
    if bi is not None:
        reader = getattr(bi, "get_collision_response_to_channel", None)
    report["has_response_reader"] = reader is not None
    if reader is not None:
        err, v = call(reader, unreal.CollisionChannel.ECC_PAWN)
        report["pawn_response_before"] = str(v) if err == "ok" else err

    # ---- 3. which setter form works? ------------------------------------
    forms = []
    for label, resp in [
        ("int 0", 0),
        ("int 1", 1),
    ]:
        forms.append((label, resp))
    enum_obj = getattr(unreal, "CollisionResponse", None)
    for label, resp in forms:
        err, _ = call(comp.set_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN, resp)
        ok = "ok"
        if err == "ok" and reader is not None:
            _, v = call(reader, unreal.CollisionChannel.ECC_PAWN)
            ok = "ok -> " + str(v)
        report.setdefault("setter_tries", []).append({"form": label, "result": err if err != "ok" else ok})

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
