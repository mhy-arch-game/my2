import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_bgmusic.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    cls = unreal.BackgroundMusicSettings
    cdo = unreal.get_default_object(cls)
    report["settings_defaults"] = {
        "Music": str(call(cdo.get_editor_property, "Music")[1])[:90],
        "Volume": str(call(cdo.get_editor_property, "Volume")[1]),
        "Speed": str(call(cdo.get_editor_property, "Speed")[1]),
        "bAutoPlay": str(call(cdo.get_editor_property, "bAutoPlay")[1]),
        "bLoop": str(call(cdo.get_editor_property, "bLoop")[1]),
    }
    sub = unreal.BackgroundMusicSubsystem
    report["subsystem_api"] = sorted([n for n in dir(sub) if any(k in n.lower() for k in
        ("volume", "speed", "play", "stop", "debug"))])
    sw = unreal.EditorAssetLibrary.load_asset("/Game/sound_resources/background")
    report["soundwave"] = {"class": sw.get_class().get_name() if sw else "MISSING",
                           "bLooping": str(call(sw.get_editor_property, "bLooping")[1]) if sw else "-",
                           "Duration": str(call(sw.get_editor_property, "Duration")[1]) if sw else "-"}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY BGMUSIC done]")
