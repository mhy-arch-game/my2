"""Attach SprintComponent to BP_FirstPersonCharacter, using the real root component."""
import json
import os
import traceback

import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "sprint_attach_result.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:200], None)


def main():
    bp = lib.load_asset(CHAR_BP)
    if bp is None:
        report["fatal"] = "character blueprint not found"
        return

    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary

    handles = list(subsys.k2_gather_subobject_data_for_blueprint(bp))
    entries = []
    for h in handles:
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        var = None
        if err == "ok":
            err2, v = call(flib.get_variable_name, data)
            var = str(v) if err2 == "ok" else "?"
        entries.append((var, h))
    report["vars"] = [e[0] for e in entries]

    if any(v and "Sprint" in v for v in report["vars"]):
        report["result"] = "already attached"
        return

    # The character has no DefaultSceneRoot; its root component is the capsule.
    parent = None
    for want in ("CapsuleComponent", "DefaultSceneRoot"):
        for var, h in entries:
            if var == want:
                parent = h
                report["parent_used"] = want
                break
        if parent is not None:
            break
    if parent is None and entries:
        parent = entries[0][1]
        report["parent_used"] = "index 0 (root/context handle)"

    params = unreal.AddNewSubobjectParams()
    call(params.set_editor_property, "parent_handle", parent)
    call(params.set_editor_property, "new_class", unreal.SprintComponent)
    call(params.set_editor_property, "blueprint_context", bp)
    call(params.set_editor_property, "conform_transform_to_parent", True)

    err, new_handle = call(subsys.add_new_subobject, params)
    report["add_result"] = err
    report["new_handle"] = None if new_handle is None else str(new_handle)

    after = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        var = None
        if err == "ok":
            err2, v = call(flib.get_variable_name, data)
            var = str(v) if err2 == "ok" else "?"
        after.append(var)
    report["vars_after"] = after

    if any(v and "Sprint" in v for v in after):
        report["verified"] = True
        report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
        report["save"] = call(lib.save_loaded_asset, bp)[0]
    else:
        report["verified"] = False
        report["note"] = "没有加上去；已保持 BP 原样（未编译未保存）"


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[SPRINTATTACH] done")
