import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_invisible.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return "<no-prop>"

def cname(c):
    return str(call(c.get_class().get_name))

def meshcomp(a):
    for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
        if "StaticMeshComponent" in cname(c):
            return c
    return None

def brief(a):
    m = meshcomp(a)
    return {"loc": str(call(a.get_actor_location))[:60],
            "rot": str(call(a.get_actor_rotation))[:70],
            "scale": str(call(a.get_actor_scale3d))[:50],
            "mesh": str(gp(m, "StaticMesh"))[:60] if m else "?",
            "profile": str(call(m.get_collision_profile_name)) if m else "?",
            "col": str(call(m.get_collision_enabled)) if m else "?",
            "bHiddenInGame": str(gp(m, "bHiddenInGame")) if m else "?",
            "bVisible": str(gp(m, "bVisible")) if m else "?",
            "actor_bHidden": str(gp(a, "bHidden")),
            "mobility": str(gp(m, "Mobility")) if m else "?"}

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    found, fixed = {}, []
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl.startswith("yinxingqiang") or lbl in ("jiguanqiang1", "jiguanqiang2"):
            found[lbl] = brief(a)
            if lbl.startswith("yinxingqiang"):
                m = meshcomp(a)
                if m is not None and str(gp(m, "bHiddenInGame")) != "True":
                    try:
                        m.set_editor_property("bHiddenInGame", True)
                        fixed.append(lbl)
                    except Exception as exc:
                        fixed.append(lbl + ":ERR " + repr(exc)[:60])
    report["before"] = found
    report["fixed"] = fixed
    if fixed:
        report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:40]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VINVIS " + json.dumps(report, ensure_ascii=True)[:2400])
print("[VERIFY INVIS done]")