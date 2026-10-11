import json, os, traceback
import unreal
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "revert_prompt_position.json")
report = {}


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
        call(obj.modify)
        report["before"] = {p: str(call(obj.get_editor_property, p)[1])[:70]
                            for p in ("WorldPromptHeightOffset", "WorldPromptDrawSize", "WorldPromptFrontOffset")}
        # 回退到之前的已知可见值
        report["h"] = call(obj.set_editor_property, "WorldPromptHeightOffset", 30.0,
                           notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        report["s"] = call(obj.set_editor_property, "WorldPromptDrawSize", unreal.Vector2D(360.0, 120.0),
                           notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        report["after"] = {p: str(call(obj.get_editor_property, p)[1])[:70]
                           for p in ("WorldPromptHeightOffset", "WorldPromptDrawSize", "WorldPromptFrontOffset")}
        break
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[REVERT PROMPT POSITION done]")
