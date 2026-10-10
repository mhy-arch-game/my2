"""Reload the level from disk and dump the door hinge state.

Proves whether the hinge properties really persisted, and records the geometry
needed to reason about the hinge side (actor rotation, mesh relative transform,
frame neighbours).
"""
import json
import os

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "verify_hinge_result.json")

report = {"doors": [], "frames": []}


def log(m):
    unreal.log("[VERIFYHINGE] " + str(m))


def v(vv):
    return [round(float(vv.x), 3), round(float(vv.y), 3), round(float(vv.z), 3)]


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    door_class = bp.generated_class() if bp else None

    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()

    for a in actors:
        label = ""
        try:
            label = a.get_actor_label()
        except Exception:
            pass
        if label.startswith("menkuang"):
            entry = {"label": label, "class": a.get_class().get_name(),
                     "loc": v(a.get_actor_location())}
            report["frames"].append(entry)

    doors = [a for a in actors if door_class is not None and a.get_class() == door_class]
    report["door_count"] = len(doors)

    for d in doors:
        # unreal.Rotator has no .euler(); read the named fields instead.
        rr = d.get_actor_rotation()
        e = {"label": d.get_actor_label(),
             "loc": v(d.get_actor_location()),
             "rot": [round(float(rr.roll), 3), round(float(rr.pitch), 3), round(float(rr.yaw), 3)],
             "scale": v(d.get_actor_scale3d())}
        try:
            e["components"] = [c.get_name() for c in d.get_components_by_class(unreal.ActorComponent)]
            comp = d.get_component_by_class(unreal.InteractableComponent)
            if comp is None:
                e["error"] = "no InteractableComponent"
                report["doors"].append(e)
                continue
            e["bUseBuiltInToggle"] = comp.get_editor_property("bUseBuiltInToggle")
            e["bUseAxisRotation"] = comp.get_editor_property("bUseAxisRotation")
            e["RotationAxis"] = str(comp.get_editor_property("RotationAxis"))
            e["RotationPivot"] = str(comp.get_editor_property("RotationPivot"))
            e["OpenAngleDegrees"] = comp.get_editor_property("OpenAngleDegrees")
            e["ClosedRelativeTransform"] = str(comp.get_editor_property("ClosedRelativeTransform"))
            e["OpenRelativeTransform"] = str(comp.get_editor_property("OpenRelativeTransform"))
            e["ToggleComponentNames"] = [str(n) for n in (comp.get_editor_property("ToggleComponentNames") or [])]

            mesh = None
            for c in d.get_components_by_class(unreal.SceneComponent):
                if c.get_name() in e["ToggleComponentNames"]:
                    mesh = c
                    break
            if mesh is not None:
                rt = mesh.get_editor_property("relative_transform")
                e["mesh_relative_transform"] = str(rt)
                lmin, lmax = mesh.get_local_bounds()
                e["mesh_local_min"] = v(lmin)
                e["mesh_local_max"] = v(lmax)
                wt = mesh.get_world_transform()
                e["mesh_world_loc"] = v(wt.translation)
                # Where the authored pivot lands in the world.
                piv = comp.get_editor_property("RotationPivot")
                e["pivot_world"] = v(wt.transform_location(piv))
        except Exception as exc:
            e["error"] = repr(exc)
        report["doors"].append(e)

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    log("doors=%d frames=%d" % (report["door_count"], len(report["frames"])))


try:
    main()
except Exception as exc:
    import traceback
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
    log("FATAL " + repr(exc))
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
