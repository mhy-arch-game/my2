import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "spawn_invisible_walls.json")
DELTA_X = -300.0
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

def cname(c):
    return str(call(c.get_class().get_name))

def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return str(v)[:40]

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    targets, existing = {}, {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl in ("jiguanqiang1", "jiguanqiang2"):
            mesh = None
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "StaticMeshComponent" in cname(c):
                    mesh = c
            targets[lbl] = {"actor": a, "mesh": mesh}
        elif lbl.startswith("yinxingqiang"):
            existing[lbl] = a
    report["existing"] = list(existing.keys())

    created = {}
    for lbl in ("jiguanqiang1", "jiguanqiang2"):
        t = targets.get(lbl)
        new_lbl = "yinxingqiang" + lbl[-1]
        info = {}
        if not t or t["mesh"] is None:
            created[new_lbl] = {"err": "缺少墙体/网格"}
            continue
        a, m = t["actor"], t["mesh"]
        loc = call(a.get_actor_location)
        new_loc = unreal.Vector(float(loc.x) + DELTA_X, float(loc.y), float(loc.z))
        if new_lbl in existing:
            spawned = existing[new_lbl]
            call(spawned.set_actor_location, new_loc, False, False)
            info["mode"] = "复用已有"
        else:
            spawned = call(ss.spawn_actor_from_class, unreal.StaticMeshActor, new_loc, call(a.get_actor_rotation))
            info["mode"] = "新建"
        if isinstance(spawned, str):
            created[new_lbl] = {"err": "spawn 失败: " + spawned}
            continue
        smc = call(spawned.get_editor_property, "StaticMeshComponent")
        call(smc.set_editor_property, "StaticMesh", call(m.get_editor_property, "StaticMesh"))
        call(spawned.set_actor_scale3d, call(a.get_actor_scale3d))
        call(spawned.set_actor_label, new_lbl)
        if hasattr(smc, "set_hidden_in_game"):
            info["hide"] = str(call(smc.set_hidden_in_game, True))[:30]
        else:
            info["hide"] = "component 无 set_hidden_in_game"
        call(smc.set_collision_profile_name, "InvisibleWall")
        info.update({"label": str(call(spawned.get_actor_label)),
                     "loc": v3(call(spawned.get_actor_location)),
                     "mesh": str(call(smc.get_editor_property, "StaticMesh"))[:64],
                     "profile": str(call(smc.get_collision_profile_name)),
                     "collision": str(call(smc.get_collision_enabled)),
                     "component_visible": str(call(smc.get_editor_property, "bVisible")),
                     "actor_bHidden": str(call(spawned.get_editor_property, "bHidden"))})
        created[new_lbl] = info
    report["created"] = created
    report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:40]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###INVIS " + json.dumps(report, ensure_ascii=True)[:2000])
print("[SPAWN INVIS done]")