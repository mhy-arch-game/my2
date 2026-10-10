import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_DOOR = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "inspect_doors.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:140], None)


def main():
    for n in ("InteractionDetectorComponent", "MotionWarpingComponent", "ClimbComponent",
              "MovementAudioComponent", "TimeShiftTravelComponent", "TimeShiftInputComponent",
              "SprintComponent", "InteractableComponent"):
        report["class_" + n] = hasattr(unreal, n)

    bp = lib.load_asset(BP_DOOR)
    report["door_bp"] = None if bp is None else bp.get_name()
    if bp is not None:
        subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        flib = unreal.SubobjectDataBlueprintFunctionLibrary
        comps = []
        tpl = {}
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            err, data = call(subsys.k2_find_subobject_data_from_handle, h)
            if err != "ok":
                continue
            err2, var = call(flib.get_variable_name, data)
            err3, obj = call(flib.get_object, data)
            comps.append(str(var) if err2 == "ok" else "?")
            if err3 == "ok" and obj is not None and isinstance(obj, unreal.InteractableComponent):
                for p in ("bEnabled", "InteractionPrompt", "bUseBuiltInToggle", "ToggleComponentNames",
                          "bUseAxisRotation", "RotationAxis", "OpenAngleDegrees", "RotationPivot",
                          "ToggleDuration", "bStartOpen", "bDisableCollisionWhenOpen", "bToggleLights"):
                    e, v = call(obj.get_editor_property, p)
                    tpl[p] = str(v) if e == "ok" else e
        report["door_bp_components"] = comps
        report["door_tpl"] = tpl

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)

    men, kuang = [], []
    hist = {}
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("menkuang"):
            kuang.append(lbl)
            continue
        if lbl.startswith("men") or lbl.startswith("active_door"):
            loc = a.get_actor_location()
            sc = a.get_actor_scale3d()
            e = {"label": lbl, "class": a.get_class().get_name()}
            e["loc"] = [round(float(loc.x), 1), round(float(loc.y), 1), round(float(loc.z), 1)]
            e["yaw"] = round(float(a.get_actor_rotation().yaw), 1)
            e["scale"] = [round(float(sc.x), 2), round(float(sc.y), 2), round(float(sc.z), 2)]
            e["has_interactable"] = a.get_component_by_class(unreal.InteractableComponent) is not None
            men.append(e)
            continue
        cn = a.get_class().get_name()
        hist[cn] = hist.get(cn, 0) + 1
    report["men"] = men
    report["menkuang_count"] = len(kuang)
    report["menkuang_sample"] = kuang[:5]
    report["class_histogram"] = dict(sorted(hist.items(), key=lambda kv: -kv[1])[:12])


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
