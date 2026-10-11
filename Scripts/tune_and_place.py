import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "tune_and_place.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
try:
    da = unreal.EditorAssetLibrary.load_asset(DA)
    call(da.set_editor_property, "SubtitleBottomOffset", 30.0, notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    report["offset"] = str(call(da.get_editor_property, "SubtitleBottomOffset"))
    report["save_da"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, da))[:20]
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "PlayerStart" in str(call(a.get_class().get_name)):
            call(a.set_actor_location, unreal.Vector(-7850.0, 615.0, 900.0), False, False)
            report["player_start"] = str(call(a.get_actor_location))[:60]
    report["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###TUNE " + json.dumps(report, ensure_ascii=True)[:400])