"""Cross-process check: is SprintComponent really on the character BP now?"""
import json
import os
import traceback

import unreal

CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "sprint_verify.json")
report = {}


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    bp = unreal.EditorAssetLibrary.load_asset(CHAR_BP)
    report["bp"] = None if bp is None else bp.get_name()
    if bp is None:
        return

    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary

    names = []
    sprint_props = None
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        vs = str(var) if err2 == "ok" else "?"
        names.append(vs)
        if vs == "Sprint":
            err3, obj = call(flib.get_object, data)
            if err3 == "ok" and obj is not None:
                e = {}
                for prop in ("SprintSpeed", "bHoldToSprint", "bRegisterSprintContext",
                             "bIncludeRightShift", "bRequireForwardInput", "bIgnoreWhenCrouched"):
                    err4, v = call(obj.get_editor_property, prop)
                    e[prop] = str(v) if err4 == "ok" else err4
                err5, key = call(obj.get_editor_property, "SprintKey")
                e["SprintKey"] = str(key) if err5 == "ok" else err5
                sprint_props = e

    report["vars"] = names
    report["has_sprint"] = any("Sprint" in n for n in names)
    report["sprint_props"] = sprint_props


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[SPRINTVERIFY] done")
