import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_invisible2.json")
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
    return {"loc": str(call(a.get_actor_location))[:58],
            "scale": str(call(a.get_actor_scale3d))[:46],
            "mesh": str(gp(m, "StaticMesh"))[:58] if m else "?",
            "profile": str(call(m.get_collision_profile_name)) if m else "?",
            "col": str(call(m.get_collision_enabled)) if m else "?",
            "bHiddenInGame": str(gp(m, "bHiddenInGame")) if m else "?",
            "bVisible": str(gp(m, "bVisible")) if m else "?",
            "actor_bHidden": str(gp(a, "bHidden"))}

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    found = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl.startswith("yinxingqiang") or lbl in ("jiguanqiang1", "jiguanqiang2"):
            found[lbl] = brief(a)
    report["after"] = found
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VINVIS2 " + json.dumps(report, ensure_ascii=True)[:2400])
print("[VERIFY2 done]")