import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_tp1.json")
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
    hits = {}
    tp = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        low = lbl.lower()
        if ("chuan" in low) or ("jiguan" in low):
            d = {"class": str(call(a.get_class().get_name)), "loc": r3(call(a.get_actor_location))}
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                cn = str(call(c.get_class().get_name))
                if "TimeEraComponent" in cn:
                    d["era"] = str(gp(c, "Era"))
                if "TimeEraPortal" in cn:
                    d["vo"] = str(gp(c, "VerticalOffset"))
                    d["tele"] = str(gp(c, "TeleportOffset"))[:40]
                    d["counterpart"] = str(gp(c, "CounterpartActor"))[:40] if gp(c, "CounterpartActor") else "None"
            hits[lbl] = d
        if lbl.startswith("kongjianchuansuoqi"):
            loc = call(a.get_actor_location)
            near = []
            for b in ss.get_all_level_actors():
                if b == a:
                    continue
                bl = call(b.get_actor_location)
                if abs(float(bl.x)-float(loc.x)) < 250 and abs(float(bl.y)-float(loc.y)) < 250 and abs(float(bl.z)-float(loc.z)) < 400:
                    near.append(str(call(b.get_actor_label)) + "@" + str(gp(b, "bHidden")))
            tp[lbl] = {"loc": r3(loc), "nearby": near[:6]}
    report["name_matches"] = hits
    report["teleporters"] = tp
    # 角色蓝图上 InteractionDetector 的 bDrawDebug 默认值：临时生成一个实例来读
    cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
    if cls:
        tmp = call(ss.spawn_actor_from_class, cls, unreal.Vector(0, 0, -20000))
        if not isinstance(tmp, str) and tmp:
            for c in list(call(tmp.get_components_by_class, unreal.ActorComponent) or []):
                if "InteractionDetector" in str(call(c.get_class().get_name)):
                    report["bp_class_bDrawDebug"] = str(gp(c, "bDrawDebug"))
            call(ss.destroy_actor, tmp)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("PROBE TP1 done")