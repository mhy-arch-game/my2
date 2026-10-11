import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_uicue.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:100] + ">"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return None

try:
    da = unreal.EditorAssetLibrary.load_asset(DA)
    cues = []
    for c in (gp(da, "Cues") or []):
        vs = []
        for v in (gp(c, "Voices") or []):
            vs.append(str(v)[:46])
        cues.append({"id": str(gp(c, "CueId")), "trigger": str(gp(c, "Trigger")),
                     "triggerId": str(gp(c, "TriggerId")), "text": str(gp(c, "Text")),
                     "voices": vs, "prox": str(gp(c, "ProximityDistance")),
                     "once": str(gp(c, "bOnce"))})
    report["cues"] = cues
    report["gap"] = str(gp(da, "QueueGapSeconds"))
    report["delims"] = str(gp(da, "SegmentDelimiters"))

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    vols = []
    for a in ss.get_all_level_actors():
        if "UiCueTriggerVolume" in str(call(a.get_class().get_name)):
            vols.append(str(call(a.get_actor_label)))
    report["trigger_volumes"] = vols
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VUICUE " + json.dumps(report, ensure_ascii=True)[:4200])
print("[VERIFY UICUE done]")