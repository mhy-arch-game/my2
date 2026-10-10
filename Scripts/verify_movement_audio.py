import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_movement_audio.json")
report = {}
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
RUN = "/Game/sound_resources/freesound_community-running-6358.freesound_community-running-6358"


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    a = unreal.EditorAssetLibrary.load_asset(RUN)
    report["running_blooping"] = str(call(a.get_editor_property, "bLooping")[1]) if a else "asset missing"
    bp = unreal.EditorAssetLibrary.load_asset(CHAR)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "MovementAudioComponent":
            continue
        report["DefaultSet"] = str(call(obj.get_editor_property, "DefaultSet")[1])[:260]
        report["StateMusic"] = str(call(obj.get_editor_property, "StateMusic")[1])[:400]
        report["bAutoFootstepByDistance"] = str(call(obj.get_editor_property, "bAutoFootstepByDistance")[1])
        break
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY MOVEMENT AUDIO done]")
