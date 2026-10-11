import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_playerstarts.json")
rep = {"playerstarts": [], "endofthegame": None, "external_actors": {}}
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
        cn = str(call(a.get_class().get_name))
        if "PlayerStart" in cn:
            rep["playerstarts"].append({"class": cn, "label": str(call(a.get_actor_label)),
                                        "name": str(call(a.get_name)),
                                        "loc": v3(call(a.get_actor_location)),
                                        "rot": v3(call(a.get_actor_rotation))})
        if "endofthegame" in cn.lower():
            rep["endofthegame"] = {"class": cn, "label": str(call(a.get_actor_label)),
                                   "loc": v3(call(a.get_actor_location))}
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("PROBE PS done")