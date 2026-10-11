import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_debug_flags.json")
report = {"drawdebug_on": [], "debug_props": {}}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:70] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            v = gp(c, "bDrawDebug")
            if v is True:
                report["drawdebug_on"].append(lbl + " / " + str(call(c.get_class().get_name)))
            if lbl in ("jiguanqiang1", "jiguanqiang2") and "ProximitySequence" in str(call(c.get_class().get_name)):
                report["debug_props"][lbl] = {"DebugLogRange": str(gp(c, "DebugLogRange")),
                                              "bDebugLogSlowZone": str(gp(c, "bDebugLogSlowZone")),
                                              "bSlowPlayerNearby": str(gp(c, "bSlowPlayerNearby"))}
        if "FirstPersonCharacter" in str(call(a.get_class().get_name)):
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "InteractionDetector" in str(call(c.get_class().get_name)):
                    report["debug_props"]["character_detector:" + lbl] = {"bDrawDebug": str(gp(c, "bDrawDebug"))}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###DEBUG " + json.dumps(report, ensure_ascii=True)[:1200])