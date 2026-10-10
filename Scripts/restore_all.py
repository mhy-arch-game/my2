"""Restore: character components + 17 active_door instances + frame collision profile."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
BP_DOOR = "/Game/bclass_source/active_door"
IA_INTERACT = "/Game/Input/Actions/IA_Interact"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "restore_result.json")
DOOR_LABELS = ["men%d" % i for i in range(1, 18)]

report = {"steps": {}}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:170], None)


def v3(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


# ---------------------------------------------------------------- step 1
COMPONENTS = [
    ("InteractionDetector", "InteractionDetectorComponent"),
    ("MotionWarping", "MotionWarpingComponent"),
    ("Climb", "ClimbComponent"),
    ("MovementAudio", "MovementAudioComponent"),
    ("TimeShiftTravel", "TimeShiftTravelComponent"),
    ("TimeShiftInput", "TimeShiftInputComponent"),
    ("Sprint", "SprintComponent"),
]


def restore_character():
    bp = lib.load_asset(CHAR_BP)
    if bp is None:
        return {"error": "character blueprint not found"}

    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    parent = None
    root_name = None
    existing = []
    for h in handles:
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        name = str(var) if err2 == "ok" else "?"
        existing.append(name)
        if name in ("CapsuleComponent", "DefaultSceneRoot"):
            parent = h
            root_name = name
    if parent is None:
        return {"error": "no root component handle found", "existing": existing}

    added = []
    for var_name, cls_name in COMPONENTS:
        if var_name in existing:
            added.append({"name": var_name, "note": "已存在，跳过"})
            continue
        cls = getattr(unreal, cls_name, None)
        if cls is None:
            added.append({"name": var_name, "error": "class not reflected"})
            continue
        params = unreal.AddNewSubobjectParams()
        call(params.set_editor_property, "parent_handle", parent)
        call(params.set_editor_property, "new_class", cls)
        call(params.set_editor_property, "blueprint_context", bp)
        err, new_handle = call(subsys.add_new_subobject, params)
        entry = {"name": var_name, "add": err}
        if err == "ok" and new_handle is not None:
            e2, nd = call(subsys.k2_find_subobject_data_from_handle, new_handle)
            if e2 == "ok":
                e3, tpl = call(flib.get_object, nd)
                if e3 == "ok" and tpl is not None:
                    entry["var"] = str(call(flib.get_variable_name, nd)[1])
                    # InteractionDetector: 还原先前的显式配置
                    if cls_name == "InteractionDetectorComponent":
                        ia = lib.load_asset(IA_INTERACT)
                        for prop, value in (("InteractAction", ia),
                                            ("PickMode", unreal.InteractionPickMode.LINE_TRACE),
                                            ("TraceDistance", 400.0),
                                            ("InteractionRadius", 250.0),
                                            ("MinFacingCosine", 0.0),
                                            ("bDrawDebug", True),
                                            ("FocusStickinessBonus", 150.0),
                                            ("bRegisterInteractContext", True)):
                            call(tpl.set_editor_property, prop, value)
                    entry["props"] = {}
                    for p in ("InteractAction", "PickMode", "TraceDistance", "InteractionRadius",
                              "bDrawDebug", "bRegisterInteractContext",
                              "SprintSpeed", "SprintKey", "bRegisterSprintContext"):
                        e4, v = call(tpl.get_editor_property, p)
                        if e4 == "ok":
                            entry["props"][p] = str(v)
        added.append(entry)

    report["steps"]["add_components"] = {"root": root_name, "before": existing, "results": added}
    report["steps"]["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["steps"]["save_bp"] = call(lib.save_loaded_asset, bp)[0]


# ---------------------------------------------------------------- step 2
def restore_doors():
    bp = lib.load_asset(BP_DOOR)
    if bp is None:
        return {"error": "door blueprint not found"}
    door_class = bp.generated_class()
    if door_class is None:
        return {"error": "door blueprint has no generated class"}

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {}
    for a in actor_ss.get_all_level_actors():
        try:
            by_label[a.get_actor_label()] = a
        except Exception:
            pass

    replaced, missing = [], []
    for label in DOOR_LABELS:
        src = by_label.get(label)
        if src is None:
            missing.append(label)
            continue
        e = {"label": label, "class_before": src.get_class().get_name()}
        try:
            if src.get_class() == door_class:
                e["note"] = "已经是 active_door_C，跳过"
                replaced.append(e)
                continue
            xform = src.get_actor_transform()
            new_actor = actor_ss.spawn_actor_from_class(door_class, src.get_actor_location(),
                                                        src.get_actor_rotation())
            new_actor.set_actor_transform(xform, False, False)
            new_actor.set_actor_label(label)
            actor_ss.destroy_actor(src)
            comp = new_actor.get_component_by_class(unreal.InteractableComponent)
            e["class_after"] = new_actor.get_class().get_name()
            e["prompt"] = str(comp.get_editor_property("InteractionPrompt")) if comp else None
            if comp is not None:
                piv = comp.get_editor_property("RotationPivot")
                e["use_axis"] = comp.get_editor_property("bUseAxisRotation")
                e["pivot"] = v3(piv)
            replaced.append(e)
        except Exception as exc:
            e["error"] = repr(exc)[:150]
            replaced.append(e)

    # frame collision profile: stop the frames blocking the player
    frames = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("menkuang"):
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            call(c.set_collision_profile_name, "IgnoreOnlyPawn")
            _, bi = call(c.get_editor_property, "body_instance")
            if bi is not None:
                call(c.set_editor_property, "body_instance", bi,
                     unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            _, pawn = call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)
            frames.append({"label": lbl, "pawn": str(pawn)})

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    save_err, saved = call(unreal.EditorLoadingAndSavingUtils.save_map, world, MAP)
    return {"replaced": replaced, "missing": missing, "frame_count": len(frames),
            "frame_pawn_sample": frames[0] if frames else None, "save": save_err}


try:
    restore_character()
except Exception as exc:
    report["steps"]["character_fatal"] = repr(exc)
    report["steps"]["character_tb"] = traceback.format_exc()

try:
    report["steps"]["doors"] = restore_doors()
except Exception as exc:
    report["steps"]["doors_fatal"] = repr(exc)
    report["steps"]["doors_tb"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
