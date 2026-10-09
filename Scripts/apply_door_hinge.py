"""Turn every active_door into a hinged door.

Hinge = the LEFT edge of the door mesh's own bounding box, so the panel swings
about its bearing instead of pivoting around the mesh origin.

The width axis is inferred from the mesh bounds (the door is a thin panel: its
thickness is the smallest horizontal extent, its width the larger one), because
this door's width happens to run along Y, not X.

DRY RUN by default; set APPLY = True to write and save the level.
"""
import json
import os

import unreal

# ---------------------------------------------------------------- config
APPLY = True
MAP_PATH = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "door_hinge_result.json")

# Hinge axis in the door mesh's local space. (0,0,1) = vertical bearing.
ROTATION_AXIS = (0.0, 0.0, 1.0)

# Which edge of the bounding box becomes the hinge:
#   "min" -> 左边缘 (the lower edge along the width axis)
#   "max" -> 右边缘
HINGE_EDGE = "min"

# Horizontal axis carrying the door width: "auto" picks the wider of X/Y.
WIDTH_AXIS = "auto"

report = {"apply": APPLY, "hinge_edge": HINGE_EDGE, "width_axis": WIDTH_AXIS,
          "rotation_axis": ROTATION_AXIS, "doors": [], "failed": []}


def log(msg):
    unreal.log("[DOORHINGE] " + str(msg))


def v3(v):
    return [round(float(v.x), 4), round(float(v.y), 4), round(float(v.z), 4)]


def local_bounds(component):
    """(min, max) of a scene component in its own local space."""
    lmin, lmax = component.get_local_bounds()
    return lmin, lmax


def compute_pivot(component):
    lmin, lmax = local_bounds(component)
    # unreal.Vector is not subscriptable - copy into plain lists first.
    lo = [float(lmin.x), float(lmin.y), float(lmin.z)]
    hi = [float(lmax.x), float(lmax.y), float(lmax.z)]
    ext = [hi[i] - lo[i] for i in range(3)]

    axis = WIDTH_AXIS
    if axis == "auto":
        # Door = thin panel: width is the larger of the two horizontal extents.
        axis = "x" if ext[0] >= ext[1] else "y"
    idx = 0 if axis == "x" else 1

    pivot = [0.0, 0.0, 0.0]
    pivot[idx] = lo[idx] if HINGE_EDGE == "min" else hi[idx]
    return pivot, ext, lo, hi, axis


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if bp is None:
        report["failed"].append("blueprint not found: " + BP_PATH)
        return
    door_class = bp.generated_class()
    if door_class is None:
        report["failed"].append("blueprint has no generated class")
        return

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["total_actors"] = len(actors)

    # NOTE: a Blueprint-generated class is not a Python type, so isinstance()
    # raises TypeError. Compare UClass objects instead.
    doors = [a for a in actors if a.get_class() == door_class]
    report["door_count"] = len(doors)

    for door in doors:
        entry = {"label": door.get_actor_label()}
        try:
            comp = door.get_component_by_class(unreal.InteractableComponent)
            if comp is None:
                entry["error"] = "no InteractableComponent"
                report["failed"].append(entry)
                continue

            names = [str(n) for n in (comp.get_editor_property("ToggleComponentNames") or [])]
            mesh = None
            for c in door.get_components_by_class(unreal.SceneComponent):
                if c.get_name() in names:
                    mesh = c
                    break
            if mesh is None:
                entry["error"] = "toggle component not found: " + str(names)
                report["failed"].append(entry)
                continue

            pivot, ext, lmin, lmax, axis = compute_pivot(mesh)

            entry["mesh"] = mesh.get_name()
            entry["bounds_min"] = lmin
            entry["bounds_max"] = lmax
            entry["extent"] = [round(e, 4) for e in ext]
            entry["width_axis"] = axis
            entry["before"] = {
                "bUseAxisRotation": comp.get_editor_property("bUseAxisRotation"),
                "RotationAxis": str(comp.get_editor_property("RotationAxis")),
                "OpenAngleDegrees": comp.get_editor_property("OpenAngleDegrees"),
                "RotationPivot": str(comp.get_editor_property("RotationPivot")),
            }

            if not APPLY:
                entry["planned_pivot"] = [round(p, 4) for p in pivot]
                report["doors"].append(entry)
                continue

            comp.set_editor_property("bUseAxisRotation", True)
            comp.set_editor_property("RotationAxis", unreal.Vector(*ROTATION_AXIS))
            comp.set_editor_property("RotationPivot", unreal.Vector(*pivot))

            # Re-read to prove the write landed.
            entry["after"] = {
                "bUseAxisRotation": comp.get_editor_property("bUseAxisRotation"),
                "RotationAxis": str(comp.get_editor_property("RotationAxis")),
                "OpenAngleDegrees": comp.get_editor_property("OpenAngleDegrees"),
                "RotationPivot": str(comp.get_editor_property("RotationPivot")),
            }
            entry["pivot"] = [round(p, 4) for p in pivot]
            report["doors"].append(entry)
        except Exception as exc:
            entry["error"] = repr(exc)
            report["failed"].append(entry)

    if APPLY and report["doors"]:
        report["level_saved"] = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    log("dry_run=%s doors=%d failed=%d" % (not APPLY, len(report["doors"]), len(report["failed"])))


main()
