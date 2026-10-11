import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_spawn.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return str(v)[:40]

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    starts, walls = [], {}
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        lbl = str(call(a.get_actor_label))
        if "PlayerStart" in cn or "PlayerStart" in lbl:
            starts.append({"label": lbl, "class": cn, "loc": v3(call(a.get_actor_location))})
        if lbl in ("jiguanqiang1", "jiguanqiang2"):
            d = {"loc": v3(call(a.get_actor_location))}
            o = unreal.Vector()
            e = unreal.Vector()
            d["bounds_inplace"] = str(call(a.get_actor_bounds, False, o, e))[:80]
            d["bounds_out"] = v3(o) + v3(e)
            r1 = call(a.get_actor_bounds, False)
            d["bounds_1arg"] = str(r1)[:160]
            meshes = [c for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []) if "StaticMeshComponent" in str(call(c.get_class().get_name))]
            if meshes:
                m = meshes[0]
                d["mesh_bounds"] = str(call(m.get_editor_property, "Bounds"))[:200]
                d["mesh_local_bounds"] = str(call(m.get_local_bounds))[:160]
                sm = call(m.get_editor_property, "StaticMesh")
                if not isinstance(sm, str) and sm:
                    d["mesh_extent"] = v3(call(sm.get_bounds).box_extent) if hasattr(call(sm, ), "box_extent") else str(call(sm.get_bounds))[:120]
            walls[lbl] = d
    report["player_starts"] = starts
    report["walls"] = walls
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###SPAWN " + json.dumps(report, ensure_ascii=True)[:2400])
print("[PROBE SPAWN done]")