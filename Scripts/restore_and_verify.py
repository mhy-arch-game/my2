import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "restore_and_verify.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "PlayerStart" in str(call(a.get_class().get_name)):
            call(a.set_actor_location, unreal.Vector(0.0, 0.0, 3182.0), False, False)
            report["player_start"] = str(call(a.get_actor_location))[:60]
    report["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
    da = unreal.EditorAssetLibrary.load_asset(DA)
    report["offset"] = str(call(da.get_editor_property, "SubtitleBottomOffset"))
    report["gap"] = str(call(da.get_editor_property, "QueueGapSeconds"))
    report["cue_count"] = str(len(call(da.get_editor_property, "Cues") or []))
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###RESTORE " + json.dumps(report, ensure_ascii=True)[:400])