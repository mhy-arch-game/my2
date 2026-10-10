import json, os, traceback
import unreal
BP = "/Game/bclass_source/modern_swift_actor"
TRIPO = "/Game/models/tripo_convert_a27cf087-650e-439c-80cf-d55ede9a4be3"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "assign_tripo.json")
report = {"tripo": TRIPO}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


def v3(v):
    try:
        return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]
    except Exception:
        return str(v)[:40]


try:
    mesh = unreal.EditorAssetLibrary.load_asset(TRIPO)
    report["mesh_class"] = mesh.get_class().get_name() if mesh else "MISSING"
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
        before = str(call(obj.get_editor_property, "StaticMesh")[1])[-70:]
        call(obj.modify)
        e, _ = call(obj.set_editor_property, "StaticMesh", mesh,
                    notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        e5, lb = call(obj.get_local_bounds)
        report["switcher"] = {
            "before": before, "set": e,
            "after": str(call(obj.get_editor_property, "StaticMesh")[1])[-70:],
            "local_min": v3(lb[0]) if (e5 == "ok" and lb) else str(e5),
            "local_max": v3(lb[1]) if (e5 == "ok" and lb and len(lb) > 1) else "",
        }
        break
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[ASSIGN TRIPO done]")
