import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_state.json")
report = {}
P = ("GravityScaleInside", "TargetJumpHeight", "bScaleJumpVelocity", "bAffectPawnsOnly")


def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return str(v)[:40]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:110], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)

    dist = {}
    meshes = 0
    jumping_labels = []
    gravity_actors = []
    doors = []
    devices = 0
    frames = 0
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            meshes += 1
            p = str(call(c.get_collision_profile_name)[1])
            dist[p] = dist.get(p, 0) + 1
        if "jumping" in lbl.lower():
            jumping_labels.append([lbl, a.get_class().get_name(), v3(a.get_actor_location())])
        for gz in a.get_components_by_class(unreal.GravityZoneComponent):
            entry = {"label": lbl, "actor": a.get_class().get_name(), "loc": v3(a.get_actor_location()),
                     "comp": gz.get_name()}
            for pr in P:
                entry[pr] = str(call(gz.get_editor_property, pr)[1])
            e, b = call(gz.get_editor_property, "BoxExtent")
            entry["BoxExtent"] = v3(b) if e == "ok" else str(e)
            gravity_actors.append(entry)
        if lbl.startswith("kongjianchuansuoqi"):
            devices += 1
        if lbl.startswith("menkuang"):
            frames += 1
        if lbl.startswith("men") and lbl[3:].isdigit():
            ic = a.get_component_by_class(unreal.InteractableComponent)
            if ic:
                doors.append({"label": lbl,
                              "useAxis": str(call(ic.get_editor_property, "bUseAxisRotation")[1]),
                              "pivot": str(call(ic.get_editor_property, "RotationPivot")[1])[:70],
                              "toggle": str(call(ic.get_editor_property, "ToggleComponentNames")[1])})
    report["mesh_component_count"] = meshes
    report["profile_distribution"] = dist
    report["jumping_labels"] = jumping_labels
    report["gravity_zones"] = gravity_actors
    report["gravity_zone_count"] = len(gravity_actors)
    report["door_count"] = len(doors)
    report["doors_bad"] = [d for d in doors if d["useAxis"] != "True" or "DoorMesh" not in d["toggle"]]
    report["doors_sample"] = doors[:3] + doors[-3:]
    report["frame_count"] = frames
    report["device_count"] = devices
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY STATE done]")
