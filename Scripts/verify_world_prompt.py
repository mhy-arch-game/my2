import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_world_prompt.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    cls = unreal.InteractionPromptComponent
    report["world_fields"] = sorted([n for n in dir(cls) if "world" in n.lower() or "screen" in n.lower() or "focused" in n.lower()])
    bp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
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
        report["defaults"] = {p: str(call(obj.get_editor_property, p)[1])
                              for p in ("bWorldSpacePrompt", "WorldPromptDrawSize", "WorldPromptHeightOffset",
                                        "WorldPromptFrontOffset", "bWorldPromptFaceCamera", "WorldPromptFacingYaw",
                                        "bUseBuiltInFallback", "bAutoBindDetector")}
        break
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY WORLD PROMPT done]")
