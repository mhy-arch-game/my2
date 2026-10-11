import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_uicue_ids.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:80]
def arr(v):
    try: return [x for x in v]
    except Exception: return []
# 完全按发行版里 GetMatchedActorKey 的编译结果复刻：对象名 -> Actor Tag -> UiCue:Tag（没有编辑器标签）
def cook_safe_key(tid, a):
    if a["name"] == tid: return "对象名"
    if tid in a["tags"]: return "Actor Tag"
    if ("UiCue:" + tid) in a["tags"]: return "Actor Tag UiCue:" + tid
    return ""
try:
    da = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/DA_UiCueSet")
    cues = []
    for c in arr(da.get_editor_property("Cues")):
        cues.append({"CueId": str(c.get_editor_property("CueId")),
                     "Trigger": str(c.get_editor_property("Trigger")),
                     "TriggerId": str(c.get_editor_property("TriggerId"))})
    report["cues"] = cues
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = []
    for a in ss.get_all_level_actors():
        actors.append({"name": str(call(a.get_name)), "label": str(call(a.get_actor_label)),
                       "tags": [str(t) for t in arr(call(a.get_editor_property, "tags"))]})
    rows = []
    ok = 0
    for c in cues:
        tid = c["TriggerId"]
        if tid in ("None", ""):
            rows.append({"CueId": c["CueId"], "TriggerId": "-", "cookSafe": "N/A (GameStart)", "labelOnly": "-"})
            ok += 1
            continue
        cs = [a for a in actors if cook_safe_key(tid, a)]
        lo = [a for a in actors if a["label"] == tid and not cook_safe_key(tid, a)]
        if cs: ok += 1
        rows.append({"CueId": c["CueId"], "TriggerId": tid,
                     "cookSafe": (cook_safe_key(tid, cs[0]) + " @ " + cs[0]["name"]) if cs else "UNRESOLVED",
                     "labelOnly": (lo[0]["name"] if lo else "-"),
                     "candidates": len(cs)})
    report["rows"] = rows
    report["okCount"] = ok
    report["cueCount"] = len(cues)
    report["actorCount"] = len(actors)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-700:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=True)
print("VERIFY UICUE IDS done")