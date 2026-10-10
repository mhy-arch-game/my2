import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "audit_material_chain.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def tex_info(v):
    try:
        if v is None:
            return "None"
        s = str(v)
        p = s.split("'")[1] if "'" in s else s
        return {"path": p, "exists": unreal.EditorAssetLibrary.does_asset_exist(p)}
    except Exception as exc:
        return "err " + repr(exc)[:60]


try:
    # 1) what material does the portal mesh use?
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/models/tripo_convert_a27cf087-650e-439c-80cf-d55ede9a4be3")
    slots = []
    e, sm = call(mesh.get_editor_property, "StaticMaterials")
    if e == "ok" and sm:
        for s in sm:
            e2, mi = call(s.get_editor_property, "MaterialInterface")
            slots.append(tex_info(mi))
    report["mesh_material_slots"] = slots

    bp = unreal.EditorAssetLibrary.load_asset("/Game/bclass_source/modern_swift_actor")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "StaticMeshComponent":
            continue
        e4, ov = call(obj.get_editor_property, "OverrideMaterials")
        report["component_override_materials"] = [tex_info(x) for x in ov] if (e4 == "ok" and ov) else str(ov)
        break

    # 2) walk the material instance chain: texture parameters + whether they resolve
    for p in ("/Game/models/tripo_mat_a27cf087",
              "/Game/models/tripo_node_a320d423-2e83-446b-9838-6b061e41b321_material_002"):
        mi = unreal.EditorAssetLibrary.load_asset(p)
        entry = {"class": mi.get_class().get_name() if mi else "MISSING"}
        if mi is not None:
            e, parent = call(mi.get_editor_property, "Parent")
            entry["parent"] = tex_info(parent)
            e2, tps = call(mi.get_editor_property, "TextureParameterValues")
            tpv = []
            if e2 == "ok" and tps:
                for t in tps:
                    e3, name = call(t.get_editor_property, "ParameterInfo")
                    e4, val = call(t.get_editor_property, "ParameterValue")
                    tpv.append({"name": str(name)[:40], "value": tex_info(val)})
            entry["texture_params"] = tpv
            e5, sps = call(mi.get_editor_property, "ScalarParameterValues")
            entry["scalar_param_count"] = len(sps) if (e5 == "ok" and sps) else 0
            e6, refs = call(unreal.AssetRegistryHelpers.get_asset_registry().get_referencers, p)
            entry["referencers"] = [str(x) for x in refs] if (e6 == "ok" and refs is not None) else str(e6)
        report[p] = entry
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[AUDIT MATERIAL CHAIN done]")
