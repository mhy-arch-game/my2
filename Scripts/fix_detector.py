"""Set the InteractionDetector config (and report input wiring of all restored components)."""
import json, os, traceback
import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
IA_INTERACT = "/Game/Input/Actions/IA_Interact"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "fix_detector_result.json")
report = {}
lib = unreal.EditorAssetLibrary

WATCH = ["InteractionDetector", "MotionWarping", "Climb", "MovementAudio",
         "TimeShiftTravel", "TimeShiftInput", "Sprint"]


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:160], None)


def main():
    bp = lib.load_asset(CHAR_BP)
    if bp is None:
        report["error"] = "character bp not found"
        return
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary

    ia = lib.load_asset(IA_INTERACT)
    report["ia_interact"] = None if ia is None else ia.get_name()

    templates = {}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        name = str(var) if err2 == "ok" else "?"
        if name not in WATCH:
            continue
        err3, obj = call(flib.get_object, data)
        if err3 == "ok" and obj is not None:
            templates[name] = obj

    report["found"] = sorted(templates.keys())

    # --- the detector is the one that MUST be configured ---
    det = templates.get("InteractionDetector")
    applied = {}
    if det is not None:
        for prop, value in (("InteractAction", ia),
                            ("PickMode", unreal.InteractionPickMode.LINE_TRACE),
                            ("TraceDistance", 400.0),
                            ("InteractionRadius", 250.0),
                            ("MinFacingCosine", 0.0),
                            ("bDrawDebug", True),
                            ("FocusStickinessBonus", 150.0),
                            ("bRegisterInteractContext", True)):
            applied[prop] = call(det.set_editor_property, prop, value,
                                 unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        # push the (already-updated) struct back doesn't apply here; properties are plain
        rb = {}
        for p in ("InteractAction", "PickMode", "TraceDistance", "InteractionRadius",
                  "MinFacingCosine", "bDrawDebug", "bRegisterInteractContext", "FocusStickinessBonus"):
            e, v = call(det.get_editor_property, p)
            rb[p] = str(v) if e == "ok" else e
        report["detector_applied"] = applied
        report["detector_readback"] = rb
    else:
        report["detector_applied"] = "InteractionDetector template not found"

    # --- report input wiring of every restored component ---
    wiring = {}
    for name, obj in templates.items():
        items = {}
        for p in ("InteractAction", "ClimbAction", "SwitchKey", "SprintAction", "SprintKey",
                  "bRegisterInteractContext", "bRegisterSprintContext"):
            e, v = call(obj.get_editor_property, p)
            if e == "ok":
                items[p] = str(v)
        wiring[name] = items
    report["wiring"] = wiring

    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save"] = call(lib.save_loaded_asset, bp)[0]


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
