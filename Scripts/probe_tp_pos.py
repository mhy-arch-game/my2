import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_tp_pos.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:70]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
def r3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    chars = []
    tps = {}
    fixed = []
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        lbl = str(call(a.get_actor_label))
        loc = call(a.get_actor_location)
        if "FirstPersonCharacter" in cn:
            d = {"label": lbl, "class": cn, "loc": r3(loc)}
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "InteractionDetector" in str(call(c.get_class().get_name)):
                    before = gp(c, "bDrawDebug")
                    d["bDrawDebug_before"] = str(before)
                    if before is False:
                        call(c.set_editor_property, "bDrawDebug", True,
                             notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                        d["bDrawDebug_after"] = str(gp(c, "bDrawDebug"))
                        fixed.append(lbl)
            chars.append(d)
        if lbl.startswith("kongjianchuansuoqi"):
            tps[lbl] = r3(loc)
    report["characters"] = chars
    report["teleporters"] = tps
    report["debug_reverted"] = fixed
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
    # 距离：角色 vs 传送器 1 / 2
    def dist(a, b):
        return round(((a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2) ** 0.5, 1)
    for ch in chars:
        if "kongjianchuansuoqi1" in tps:
            ch["dist_to_tp1"] = dist(ch["loc"], tps["kongjianchuansuoqi1"])
        if "kongjianchuansuoqi2" in tps:
            ch["dist_to_tp2"] = dist(ch["loc"], tps["kongjianchuansuoqi2"])
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("PROBE POS done")