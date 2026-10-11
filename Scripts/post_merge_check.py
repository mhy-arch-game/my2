import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "post_merge_check.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    da = unreal.EditorAssetLibrary.load_asset(DA)
    cues = list(call(da.get_editor_property, "Cues") or [])
    report["cue_count"] = len(cues)
    report["cue_ids"] = [str(gp(c, "CueId")) for c in cues]
    report["subtitle_offset"] = str(gp(da, "SubtitleBottomOffset"))
    report["gap"] = str(gp(da, "QueueGapSeconds"))
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    found = {"invisible": [], "proximity": [], "uicue_vol": [], "portal34": {}}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        comps = [str(call(c.get_class().get_name)) for c in list(call(a.get_components_by_class, unreal.ActorComponent) or [])]
        if lbl.startswith("yinxingqiang"):
            found["invisible"].append(lbl)
        if lbl in ("jiguanqiang1", "jiguanqiang2") and "ProximitySequenceComponent" in comps:
            found["proximity"].append(lbl)
        if "UiCueTriggerVolume" in comps:
            found["uicue_vol"].append(lbl)
        if lbl in ("kongjianchuansuoqi3", "kongjianchuansuoqi4"):
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "TimeEraPortal" in str(call(c.get_class().get_name)):
                    found["portal34"][lbl] = str(gp(c, "VerticalOffset"))
    report["level"] = found
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###POST " + json.dumps(report, ensure_ascii=True)[:1200])