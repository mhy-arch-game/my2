"""What actually overlaps each doorway? Lists every actor whose bounds intersect a
door's bounds, so a solid wall slab behind/around the door shows up."""
import json
import os
import traceback

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "doorway_overlap_result.json")
report = {}


def log(m):
    unreal.log("[OVERLAP] " + str(m))


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:130], None)


def aabb(actor):
    err, b = call(actor.get_actor_bounds, False, False)
    if err != "ok":
        return None
    o, e = b[0], b[1]
    return ([float(o.x) - float(e.x), float(o.y) - float(e.y), float(o.z) - float(e.z)],
            [float(o.x) + float(e.x), float(o.y) + float(e.y), float(o.z) + float(e.z)])


def overlap(a, b, pad=5.0):
    return all(a[0][i] - pad <= b[1][i] and b[0][i] - pad <= a[1][i] for i in range(3))


def mesh_of(actor):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        err, sm = call(c.get_editor_property, "static_mesh")
        if err == "ok" and sm is not None:
            return sm.get_name()
    return None


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()

    boxes = {}
    for a in actors:
        try:
            boxes[a.get_actor_label()] = (a, aabb(a))
        except Exception:
            pass
    report["total"] = len(boxes)

    for probe in ("men2", "men3", "active_door"):
        entry = boxes.get(probe)
        if entry is None:
            report[probe] = "<absent>"
            continue
        door, dbox = entry
        if dbox is None:
            report[probe] = "<no bounds>"
            continue
        hits = []
        for lbl, (a, box) in boxes.items():
            if a is door or box is None:
                continue
            if overlap(dbox, box):
                hits.append({
                    "label": lbl,
                    "class": a.get_class().get_name(),
                    "mesh": mesh_of(a),
                    "min": [round(v, 1) for v in box[0]],
                    "max": [round(v, 1) for v in box[1]],
                    "extent": [round((box[1][i] - box[0][i]) / 2.0, 1) for i in range(3)],
                })
        hits.sort(key=lambda h: -max(h["extent"]))
        report[probe] = {
            "door_min": [round(v, 1) for v in dbox[0]],
            "door_max": [round(v, 1) for v in dbox[1]],
            "overlapping": hits,
        }


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done fatal=%s" % report.get("fatal"))
