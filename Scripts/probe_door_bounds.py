"""Probe: how to read a door's mesh bounds from the editor Python API.

Read-only. Writes Scripts/probe_door_bounds.json.
"""
import json
import os

import unreal

MAP_PATH = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "probe_door_bounds.json")

report = {}


def log(msg):
    unreal.log("[PROBE] " + str(msg))


def probe_object(obj, key, extra_attrs=()):
    entry = {}
    for meth in ("get_local_bounds", "get_bounds", "get_bounding_box",
                 "get_bounding_box_extent", "get_actor_bounds"):
        fn = getattr(obj, meth, None)
        if fn is None:
            entry[meth] = "<absent>"
            continue
        try:
            entry[meth] = str(fn())
        except Exception as exc:
            entry[meth] = "ERR " + repr(exc)[:140]
    for attr in ("bounds", "local_bounds", "extended_bounds", "box_extent",
                 "origin", "static_mesh") + tuple(extra_attrs):
        try:
            entry["attr:" + attr] = str(getattr(obj, attr))
        except Exception as exc:
            entry["attr:" + attr] = "ERR " + repr(exc)[:80]
    entry["_methods_with_bound_or_extent"] = sorted(
        [m for m in dir(obj) if "bound" in m.lower() or "extent" in m.lower()])
    report[key] = entry


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["total_actors"] = len(actors)

    # Pick a door: any actor whose label is exactly "active_door", else "men2".
    door = None
    for wanted in ("active_door", "men2"):
        for a in actors:
            try:
                if a.get_actor_label() == wanted:
                    door = a
                    break
            except Exception:
                pass
        if door is not None:
            break

    if door is None:
        report["error"] = "no active_door instance found"
        return

    report["door"] = {
        "label": door.get_actor_label(),
        "class": door.get_class().get_name(),
        "transform": str(door.get_actor_transform()),
    }

    report["components"] = [[c.get_name(), c.get_class().get_name()]
                            for c in door.get_components_by_class(unreal.ActorComponent)]

    comp = door.get_component_by_class(unreal.InteractableComponent)
    if comp is None:
        report["error"] = "door has no InteractableComponent"
        return

    props = {}
    for p in ("bEnabled", "bUseBuiltInToggle", "ToggleComponentNames", "ToggleComponents",
              "ClosedRelativeTransform", "OpenRelativeTransform", "ToggleDuration",
              "bStartOpen", "bDisableCollisionWhenOpen", "bUseAxisRotation",
              "RotationAxis", "OpenAngleDegrees", "RotationPivot"):
        try:
            props[p] = str(comp.get_editor_property(p))
        except Exception as exc:
            props[p] = "ERR " + repr(exc)[:140]
    report["interactable"] = props

    # Resolve the component the interaction actually drives.
    names = [str(n) for n in (comp.get_editor_property("ToggleComponentNames") or [])]
    report["toggle_names"] = names
    by_name = {}
    for c in door.get_components_by_class(unreal.SceneComponent):
        by_name[c.get_name()] = c

    target = None
    for n in names:
        if n in by_name:
            target = by_name[n]
            break
    if target is None and names:
        report["toggle_resolution"] = "names present but none matched: " + str(names)
    report["resolved_toggle"] = None if target is None else [
        target.get_name(), target.get_class().get_name()]

    if target is not None:
        probe_object(target, "toggle_component")
        sm = None
        try:
            sm = target.get_editor_property("static_mesh")
        except Exception as exc:
            report["static_mesh_err"] = repr(exc)[:140]
        if sm is None:
            sm = getattr(target, "static_mesh", None)
        report["static_mesh_asset"] = None if sm is None else sm.get_name()
        if sm is not None:
            probe_object(sm, "static_mesh")

    report["StaticMesh_methods_with_bound"] = sorted(
        [m for m in dir(unreal.StaticMesh) if "bound" in m.lower() or "extent" in m.lower()])

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    log("wrote " + OUT)


main()
