import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "apply_audio_fallback.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    bp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "MovementAudioComponent":
            continue
        before = str(call(obj.get_editor_property, "bAutoFootstepByDistance")[1])
        call(obj.modify)
        e, r = call(obj.set_editor_property, "bAutoFootstepByDistance", True,
                    notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        rows.append({"comp": obj.get_name(), "before": before, "set": e,
                     "after": str(call(obj.get_editor_property, "bAutoFootstepByDistance")[1]),
                     "footstep_distance": str(call(obj.get_editor_property, "FootstepDistance")[1])})
    report["writes"] = rows
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[AUDIO FALLBACK done]")
