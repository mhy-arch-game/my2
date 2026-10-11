import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "move_playerstart.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "PlayerStart" not in str(call(a.get_class().get_name)):
            continue
        loc = call(a.get_actor_location)
        report["old"] = [round(float(loc.x), 1), round(float(loc.y), 1), round(float(loc.z), 1)]
        call(a.set_actor_location, unreal.Vector(-760.0, -3310.0, 250.0), False, False)
        loc2 = call(a.get_actor_location)
        report["new"] = [round(float(loc2.x), 1), round(float(loc2.y), 1), round(float(loc2.z), 1)]
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:60]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###MOVE " + json.dumps(report, ensure_ascii=True)[:600])
print("[MOVE done]")