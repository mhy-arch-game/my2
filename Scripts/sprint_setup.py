"""1) create IA_Sprint  2) try attaching SprintComponent to BP_FirstPersonCharacter."""
import json
import os
import traceback

import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "sprint_setup_result.json")
report = {}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:170], None)


def main():
    # ---- 1. Input Action ------------------------------------------------
    package = "/Game/Input/Actions"
    full = package + "/IA_Sprint"
    if lib.does_asset_exist(full):
        report["ia_sprint"] = "already exists"
    else:
        factory = None
        for name in ("InputActionFactory", "AssetFactory"):
            cls = getattr(unreal, name, None)
            if cls is None:
                continue
            err, f = call(cls)
            if err == "ok" and f is not None:
                factory = f
                report["ia_factory"] = name
                break
        err, ia = call(asset_tools.create_asset, "IA_Sprint", package, unreal.InputAction, factory)
        report["ia_create"] = err
        if err == "ok" and ia is not None:
            call(ia.set_editor_property, "value_type", unreal.InputActionValueType.BOOLEAN)
            call(lib.save_loaded_asset, ia)
            report["ia_sprint"] = full

    # ---- 2. SprintComponent reflection ----------------------------------
    cls = getattr(unreal, "SprintComponent", None)
    report["sprint_class"] = cls is not None
    if cls is not None:
        err, obj = call(unreal.new_object, cls)
        if err == "ok" and obj is not None:
            e = {}
            for prop in ("SprintSpeed", "NormalSpeedOverride", "SprintKey", "bIncludeRightShift",
                         "bHoldToSprint", "bRegisterSprintContext", "bRequireForwardInput",
                         "bIgnoreWhenCrouched", "bSprintEnabled"):
                err2, v = call(obj.get_editor_property, prop)
                e[prop] = str(v) if err2 == "ok" else err2
            report["sprint_defaults"] = e
            err3, s = call(obj.get_sprint_debug_string)
            report["debug_string"] = str(s) if err3 == "ok" else err3

    # ---- 3. try adding the component to the character BP ----------------
    bp = lib.load_asset(CHAR_BP)
    report["char_bp"] = None if bp is None else bp.get_name()
    if bp is None:
        return

    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    report["add_new_subobject_doc"] = str(getattr(subsys.add_new_subobject, "__doc__", ""))[:400]
    report["params_props"] = sorted([n for n in dir(unreal.AddNewSubobjectParams) if not n.startswith("_")])

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    root_handle = None
    names = []
    for h in handles:
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        vs = str(var) if err2 == "ok" else "?"
        names.append(vs)
        if vs == "DefaultSceneRoot":
            root_handle = h
    report["existing_vars"] = names
    report["already_has_sprint"] = any("Sprint" in n for n in names)

    if report["already_has_sprint"]:
        report["add_component"] = "already present"
        return

    if root_handle is None:
        report["add_component"] = "DefaultSceneRoot handle not found"
        return

    params = unreal.AddNewSubobjectParams()
    for prop, value in (("parent_handle", root_handle),
                        ("new_class", unreal.SprintComponent),
                        ("blueprint_context", bp)):
        err = call(params.set_editor_property, prop, value)[0]
        report.setdefault("param_sets", {})[prop] = err

    err, new_handle = call(subsys.add_new_subobject, params)
    report["add_component"] = err if err == "ok" else err
    report["new_handle"] = None if new_handle is None else str(new_handle)

    # verify before saving anything
    after = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        after.append(str(var) if err2 == "ok" else "?")
    report["vars_after"] = after
    added = any("Sprint" in n for n in after)
    report["verified_added"] = added

    if added:
        report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
        report["save"] = call(lib.save_loaded_asset, bp)[0]
    else:
        report["note"] = "组件没有真正加上去，因此没有编译/保存，BP 保持原样"


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[SPRINTSETUP] done")
