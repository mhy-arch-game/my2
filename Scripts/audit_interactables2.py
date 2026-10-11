import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "audit_interactables.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:70]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    real = []
    for a in ss.get_all_level_actors():
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        inter = None
        for c in comps:
            if "InteractableComponent" in str(call(c.get_class().get_name)):
                inter = c
        if inter is None:
            continue
        meshes = []
        for c in comps:
            cn = str(call(c.get_class().get_name))
            if "MeshComponent" not in cn:
                continue
            sm = gp(c, "StaticMesh")
            nan = None
            if sm is not None:
                ns = gp(sm, "nanite_settings")
                if ns is not None:
                    nan = gp(ns, "enabled")
            meshes.append({"mesh": str(sm)[:40], "nanite": str(nan)})
        real.append({"label": str(call(a.get_actor_label)),
                     "class": str(call(a.get_class().get_name)),
                     "enabled": str(gp(inter, "bEnabled")),
                     "meshes": meshes})
    report["total"] = len(real)
    on = [r for r in real if r["enabled"] == "True"]
    report["enabled_true"] = on
    report["enabled_count"] = len(on)
    report["enabled_nanite"] = [r["label"] for r in on if any(m["nanite"] == "True" for m in r["meshes"])]
    report["enabled_no_mesh"] = [r["label"] for r in on if len(r["meshes"]) == 0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("AUDIT done")