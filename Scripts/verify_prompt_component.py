import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_prompt_component.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    cls = unreal.InteractionPromptComponent
    report["class_dir"] = sorted([n for n in dir(cls) if "prompt" in n.lower()])
    report["fallback_exists"] = hasattr(unreal, "InteractionPromptFallbackWidget")
    report["abstract_base_exists"] = hasattr(unreal, "InteractionPromptWidget")
    bp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    names = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e != "ok":
            continue
        e2, obj = call(flib.get_object, d)
        e3, var = call(flib.get_variable_name, d)
        nm = str(var) if e3 == "ok" else "?"
        names.append(nm)
        if nm == "InteractionPrompt" and obj is not None:
            report["component_props"] = {
                p: str(call(obj.get_editor_property, p)[1])
                for p in ("bUseBuiltInFallback", "bAutoBindDetector", "ScreenOffsetY", "ZOrder",
                          "DefaultPromptText", "WidgetClass")
            }
    report["character_components"] = names
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY PROMPT done]")
