"""Verify the frame Pawn response actually changed and persisted."""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "frame_pawn_verify.json")
report = {}


def log(m):
    unreal.log("[PAWNCHK] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def pawn_response(comp, bi):
    """Try every plausible way to read the ECC_Pawn response."""
    out = {}
    for label, fn in (
        ("comp.get_collision_response_to_channel", getattr(comp, "get_collision_response_to_channel", None)),
        ("bi.get_collision_response_to_channel", getattr(bi, "get_collision_response_to_channel", None)),
    ):
        if fn is None:
            out[label] = "<absent>"
            continue
        err, v = call(fn, unreal.CollisionChannel.ECC_PAWN)
        out[label] = str(v) if err == "ok" else err
    # walk the FCollisionResponse struct
    err, resp = call(bi.get_editor_property, "collision_responses")
    if err == "ok" and resp is not None:
        out["resp_type"] = type(resp).__name__
        out["resp_props"] = sorted([n for n in dir(resp) if not n.startswith("_")])
        arr = getattr(resp, "response_array", None)
        if arr is None:
            err2, arr = call(resp.get_editor_property, "response_array")
        if arr is not None:
            try:
                for rc in arr:
                    if str(rc.get_editor_property("channel")) == "Pawn":
                        out["pawn_entry"] = str(rc.get_editor_property("response"))
            except Exception as exc:
                out["pawn_entry"] = "ERR " + repr(exc)[:90]
    return out


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("menkuang"):
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            err, bi = call(c.get_editor_property, "body_instance")
            row = {"label": lbl, "profile": None}
            err2, prof = call(c.get_collision_profile_name)
            row["profile"] = str(prof) if err2 == "ok" else err2
            if err == "ok" and bi is not None:
                row.update(pawn_response(c, bi))
            rows.append(row)
    report["rows"] = rows

    # a door for comparison (should still block the pawn)
    door = None
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_actor_label() == "men2":
                door = a
        except Exception:
            pass
    if door is not None:
        c = door.get_components_by_class(unreal.StaticMeshComponent)
        if c:
            err, prof = call(c[0].get_collision_profile_name)
            report["door_profile"] = str(prof) if err == "ok" else err


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
