import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_plan_a.json")
report = {}
RESTORED = ["/Game/models/毛绒熊3d模型",
            "/Game/models/毛绒熊3d模型_basecolor",
            "/Game/models/毛绒熊3d模型_normal",
            "/Game/models/fireplace",
            "/Game/models/fireplace_basecolor",
            "/Game/models/b9acd5b7_b130_41b9_b7e3_14ed88825340"]


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
        return {"path": p.split(".")[-1], "exists": unreal.EditorAssetLibrary.does_asset_exist(p)}
    except Exception as exc:
        return "err " + repr(exc)[:50]


try:
    # 1) restored assets load + class + material slots
    for p in RESTORED:
        a = unreal.EditorAssetLibrary.load_asset(p)
        e = {"class": a.get_class().get_name() if a else "MISSING"}
        if a is not None and e["class"] == "StaticMesh":
            e2, sm = call(a.get_editor_property, "StaticMaterials")
            slots = []
            if e2 == "ok" and sm:
                for s in sm:
                    e3, mi = call(s.get_editor_property, "MaterialInterface")
                    slots.append(tex_info(mi))
            e["material_slots"] = slots
        report[p] = e

    # 2) does the orphan tripo MI re-resolve its texture params now?
    mi = unreal.EditorAssetLibrary.load_asset("/Game/models/tripo_node_a320d423-2e83-446b-9838-6b061e41b321_material_002")
    e, tps = call(mi.get_editor_property, "TextureParameterValues")
    params = []
    if e == "ok" and tps:
        for t in tps:
            e2, v = call(t.get_editor_property, "ParameterValue")
            params.append(tex_info(v))
    report["orphan_MI_texture_params"] = params

    # 3) acceptance fingerprints
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    nulls = 0
    devices = 0
    switcher = None
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith("kongjianchuansuoqi"):
            devices += 1
            if lbl == "kongjianchuansuoqi1":
                c = a.get_component_by_class(unreal.StaticMeshComponent)
                e4, lb = call(c.get_local_bounds)
                if e4 == "ok" and lb:
                    switcher = [round(float(lb[1].x - lb[0].x), 1), round(float(lb[1].y - lb[0].y), 1),
                                round(float(lb[1].z - lb[0].z), 1)]
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            e5, m = call(c.get_editor_property, "StaticMesh")
            if e5 == "ok" and m is None:
                nulls += 1
    report["fingerprint"] = {"null_mesh_slots": nulls, "device_count": devices, "switcher_size": switcher}
    # portal material chain still intact?
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/models/tripo_convert_a27cf087-650e-439c-80cf-d55ede9a4be3")
    e6, sm2 = call(mesh.get_editor_property, "StaticMaterials")
    mat = "?"
    if e6 == "ok" and sm2:
        e7, mi2 = call(sm2[0].get_editor_property, "MaterialInterface")
        mat = tex_info(mi2)
    report["portal_mesh_material"] = mat
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY PLAN A done]")
