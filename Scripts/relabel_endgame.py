import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "relabel_endgame.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:90]
def v3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "end_of_the_game" in str(call(a.get_class().get_name)).lower():
            rep["before"] = str(call(a.get_actor_label))
            call(a.set_actor_label, "end_of_the_game")
            rep["after"] = str(call(a.get_actor_label))
            rep["loc"] = v3(call(a.get_actor_location))
    rep["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
    rep["save_dirty"] = str(call(unreal.EditorLoadingAndSavingUtils.save_dirty_packages, True, True))[:20]
except Exception as exc:
    rep["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("RELABEL done")