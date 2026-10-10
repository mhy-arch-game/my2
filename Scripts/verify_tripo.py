import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_tripo.json")
BP = "/Game/bclass_source/modern_swift_actor"
MAP = "/Game/FirstPerson/firstvision"
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def v3(v):
    try:
        return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]
    except Exception:
        return str(v)[:50]


try:
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "StaticMeshComponent":
            continue
        e4, mesh = call(obj.get_editor_property, "StaticMesh")
        e5, lb = call(obj.get_local_bounds)
        report["template_switcher"] = {
            "mesh": (None if mesh is None else str(mesh))[-120:] if e4 == "ok" else str(e4),
            "local_min": v3(lb[0]) if (e5 == "ok" and lb) else str(e5),
            "local_max": v3(lb[1]) if (e5 == "ok" and lb and len(lb) > 1) else "",
        }
        break

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dev = []
    null_meshes = []
    tripo_users = {}
    total = 0
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            total += 1
            e, m = call(c.get_editor_property, "StaticMesh")
            mp = None if m is None else str(m)
            if e != "ok" or m is None:
                null_meshes.append([lbl, c.get_name()])
            elif "tripo" in mp.lower():
                tripo_users.setdefault(mp.split(".")[-1], []).append(lbl)
            if lbl.startswith("kongjianchuansuoqi"):
                dev.append({"label": lbl, "mesh": (mp or "None")[-56:]})
    report["level_mesh_component_total"] = total
    report["level_null_mesh_count"] = len(null_meshes)
    report["level_null_mesh_sample"] = null_meshes[:25]
    report["tripo_mesh_users"] = {k: v[:14] for k, v in tripo_users.items()}
    dev.sort(key=lambda r: int("".join(ch for ch in r["label"] if ch.isdigit()) or 0))
    report["devices"] = dev
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY TRIPO2 done]")
