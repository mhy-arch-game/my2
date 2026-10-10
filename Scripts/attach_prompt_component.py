import json, os, traceback
import unreal
CHAR_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "attach_prompt_component.json")
report = {}
lib = unreal.EditorAssetLibrary
WANT = "InteractionPrompt"


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:180], None)


try:
    bp = lib.load_asset(CHAR_BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary

    entries = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        var = None
        if e == "ok":
            e2, v = call(flib.get_variable_name, d)
            var = str(v) if e2 == "ok" else "?"
        entries.append((var, h))
    report["vars_before"] = [e[0] for e in entries]

    if any(v and WANT in v for v in report["vars_before"]):
        report["result"] = "already attached"
    else:
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
            report["parent_used"] = "index 0"
        params = unreal.AddNewSubobjectParams()
        call(params.set_editor_property, "parent_handle", parent)
        call(params.set_editor_property, "new_class", unreal.InteractionPromptComponent)
        call(params.set_editor_property, "blueprint_context", bp)
        call(params.set_editor_property, "conform_transform_to_parent", False)
        err, handle = call(subsys.add_new_subobject, params)
        report["add_result"] = err
        report["new_handle"] = None if handle is None else str(handle)

    after = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        var = None
        if e == "ok":
            e2, v = call(flib.get_variable_name, d)
            var = str(v) if e2 == "ok" else "?"
        after.append(var)
    report["vars_after"] = after

    if any(v and WANT in v for v in after):
        report["verified"] = True
        report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
        report["save"] = call(lib.save_loaded_asset, bp)[0]
    else:
        report["verified"] = False
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[ATTACH PROMPT done]")
