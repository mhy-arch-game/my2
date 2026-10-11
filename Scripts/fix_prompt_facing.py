import json, os, traceback
import unreal
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_prompt_facing.json")
report = {}
FIELDS = ("bWorldSpacePrompt", "bWorldPromptFaceCamera", "WorldPromptFacingYaw",
          "WorldPromptDrawSize", "WorldPromptHeightOffset", "WorldPromptFrontOffset", "WidgetClass")


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    bp = unreal.EditorAssetLibrary.load_asset(CHAR)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e != "ok":
            continue
        e2, obj = call(flib.get_object, d)
        e3, var = call(flib.get_variable_name, d)
        if obj is None or str(var) != "InteractionPrompt":
            continue
        before = {p: str(call(obj.get_editor_property, p)[1]) for p in FIELDS}
        call(obj.modify)
        w = {}
        w["WorldPromptFacingYaw"] = call(obj.set_editor_property, "WorldPromptFacingYaw", 0.0,
                                        notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        w["bWorldPromptFaceCamera"] = call(obj.set_editor_property, "bWorldPromptFaceCamera", True,
                                           notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        w["bWorldSpacePrompt"] = call(obj.set_editor_property, "bWorldSpacePrompt", True,
                                      notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        after = {p: str(call(obj.get_editor_property, p)[1]) for p in FIELDS}
        report["before"] = before
        report["write"] = w
        report["after"] = after
        break
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX PROMPT FACING done]")
