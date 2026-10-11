import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_uicue_ids.json")
report = {"stamped": [], "skipped": [], "levels": {}}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:80]
def arr(v):
    try: return [x for x in v]
    except Exception: return []
try:
    da = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/DA_UiCueSet")
    ids = []
    for c in arr(da.get_editor_property("Cues")):
        t = str(c.get_editor_property("TriggerId"))
        if t and t != "None" and t not in ids:
            ids.append(t)
    report["triggerIds"] = ids
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl not in ids:
            continue
        want = "UiCue:" + lbl
        tags = [str(t) for t in arr(call(a.get_editor_property, "tags"))]
        if want in tags:
            report["skipped"].append({"label": lbl, "name": str(call(a.get_name)), "tags": tags})
            continue
        newtags = [unreal.Name(t) for t in tags] + [unreal.Name(want)]
        res = call(a.set_editor_property, "tags", newtags,
                   notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        after = [str(t) for t in arr(call(a.get_editor_property, "tags"))]
        report["stamped"].append({"label": lbl, "name": str(call(a.get_name)), "class": str(call(a.get_class().get_name)),
                                  "before": tags, "after": after, "setErr": str(res)[:60]})
    report["levels"]["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:40]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-700:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=True)
print("FIX UICUE IDS done")