"""Fresh-process verification of the restore."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
BP_DOOR = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "verify_restore.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    # ---- character BP from disk ----
    bp = lib.load_asset(CHAR_BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    names, det, extra = [], None, {}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        name = str(var) if err2 == "ok" else "?"
        names.append(name)
        err3, obj = call(flib.get_object, data)
        if err3 != "ok" or obj is None:
            continue
        if name == "InteractionDetector":
            d = {}
            for p in ("InteractAction", "PickMode", "TraceDistance", "InteractionRadius",
                      "bDrawDebug", "bRegisterInteractContext"):
                e, v = call(obj.get_editor_property, p)
                d[p] = str(v) if e == "ok" else e
            det = d
        if name in ("Climb", "TimeShiftInput", "MovementAudio"):
            extra[name] = sorted([n for n in dir(obj)
                                  if ("action" in n.lower() or "key" in n.lower())
                                  and not n.startswith("_")])
    report["char_components"] = names
    report["detector_from_disk"] = det
    report["wiring_props"] = extra

    # ---- level ----
    dbp = lib.load_asset(BP_DOOR)
    door_class = dbp.generated_class() if dbp else None
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    doors, frames, prof = 0, 0, {}
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("menkuang"):
            frames += 1
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                e, p = call(c.get_collision_profile_name)
                key = str(p) if e == "ok" else "?"
                prof[key] = prof.get(key, 0) + 1
            continue
        if door_class is not None and a.get_class() == door_class:
            doors += 1
    report["actor_count"] = len(actor_ss.get_all_level_actors())
    report["door_count"] = doors
    report["frame_count"] = frames
    report["frame_profiles"] = prof


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
