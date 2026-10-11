import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "place_in_overlap.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "PlayerStart" in str(call(a.get_class().get_name)):
            call(a.set_actor_location, unreal.Vector(-7850.0, 615.0, 2500.0), False, False)
            report["player_start"] = str(call(a.get_actor_location))[:60]
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###PLACE " + json.dumps(report, ensure_ascii=True)[:300])