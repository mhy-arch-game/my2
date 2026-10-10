import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "diag_bgmusic.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    for path in ("/Script/MHY_ARCH_GAME.BackgroundMusicSettings",
                 "/Script/MHY_ARCH_GAME.BackgroundMusicSubsystem",
                 "/Script/MHY_ARCH_GAME.UiCueSubsystem"):
        report[path] = str(call(unreal.load_class, None, path)[1])
    report["has_subsystem_attr"] = hasattr(unreal, "BackgroundMusicSubsystem")
    report["dir_background"] = sorted([n for n in dir(unreal) if "Background" in n or "Music" in n])[:20]
    report["dir_uicue"] = sorted([n for n in dir(unreal) if "Cue" in n])[:20]

    cls = call(unreal.load_class, None, "/Script/MHY_ARCH_GAME.BackgroundMusicSettings")[1]
    if cls is not None:
        cdo = unreal.get_default_object(cls)
        report["settings"] = {p: str(call(cdo.get_editor_property, p)[1])[:80]
                              for p in ("Music", "Volume", "Speed", "bAutoPlay", "bLoop")}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[DIAG BGMUSIC done]")
