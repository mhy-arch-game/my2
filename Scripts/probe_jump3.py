import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jump3.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
def r3(v):
    try: return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception: return str(v)[:40]
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    zones = []
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("jumping_area"):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "GravityZone" not in str(call(c.get_class().get_name)):
                continue
            wl = call(c.get_world_location)
            ext = gp(c, "BoxExtent")
            rel = gp(c, "RelativeLocation")
            mesh = None
            for cand in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "StaticMeshComponent" in str(call(cand.get_class().get_name)):
                    mesh = cand
            zones.append({"label": lbl,
                          "actor_loc": r3(call(a.get_actor_location)),
                          "comp_world": r3(wl),
                          "comp_rel": r3(rel) if rel is not None else "?",
                          "ext": r3(ext),
                          "min": [round(float(wl.x) - float(ext.x), 1), round(float(wl.y) - float(ext.y), 1), round(float(wl.z) - float(ext.z), 1)],
                          "max": [round(float(wl.x) + float(ext.x), 1), round(float(wl.y) + float(ext.y), 1), round(float(wl.z) + float(ext.z), 1)],
                          "mesh_profile": str(call(mesh.get_collision_profile_name)) if mesh else "?",
                          "mesh_col": str(call(mesh.get_collision_enabled)) if mesh else "?"})
    report["zones"] = zones
    p = (-7850.0, 615.0, 900.0)
    inside = []
    for z in zones:
        ok = all(z["min"][i] <= p[i] <= z["max"][i] for i in range(3))
        inside.append({"zone": z["label"], "pawn_inside": ok,
                       "z_range": [z["min"][2], z["max"][2]], "y_range": [z["min"][1], z["max"][1]]})
    report["spawn_check"] = inside
    report["spawn_point"] = list(p)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###JUMP3 " + json.dumps(report, ensure_ascii=True)[:2600])