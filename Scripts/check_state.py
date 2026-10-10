"""What survived the 13:24 event? Sprint component / door hinges / frame profile / WP."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
BP_DOOR = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "state_check.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:140], None)


def main():
    # --- 1. character blueprint: is the Sprint component still there? ---
    bp = lib.load_asset(CHAR_BP)
    report["char_bp"] = None if bp is None else bp.get_name()
    if bp is not None:
        subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        flib = unreal.SubobjectDataBlueprintFunctionLibrary
        names = []
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            err, data = call(subsys.k2_find_subobject_data_from_handle, h)
            if err != "ok":
                continue
            err2, v = call(flib.get_variable_name, data)
            names.append(str(v) if err2 == "ok" else "?")
        report["char_components"] = names
        report["has_sprint_component"] = any(n and "Sprint" in n for n in names)
        report["has_interaction_detector"] = any(n and "InteractionDetector" in n for n in names)

    # --- 2. level state ---
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)

    door_class = None
    dbp = lib.load_asset(BP_DOOR)
    if dbp is not None:
        door_class = dbp.generated_class()
    report["door_bp_loaded"] = door_class is not None

    doors, frames, frames_profile = 0, 0, {}
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("menkuang"):
            frames += 1
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                err, prof = call(c.get_collision_profile_name)
                frames_profile[str(prof) if err == "ok" else "?"] = frames_profile.get(str(prof) if err == "ok" else "?", 0) + 1
        elif door_class is not None and a.get_class() == door_class:
            doors += 1
    report["door_count"] = doors
    report["frame_count"] = frames
    report["frame_profiles"] = frames_profile

    # door hinge sample
    sample = []
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl in ("men2", "active_door"):
            comp = a.get_component_by_class(unreal.InteractableComponent)
            if comp is not None:
                piv = comp.get_editor_property("RotationPivot")
                sample.append({"label": lbl,
                               "use_axis": comp.get_editor_property("bUseAxisRotation"),
                               "pivot": "{:.2f},{:.2f},{:.2f}".format(piv.x, piv.y, piv.z)})
    report["door_sample"] = sample


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc); report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
