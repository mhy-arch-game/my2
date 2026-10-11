import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
MAT_DIR = "/Game/MHY_ARCH_GAME/UI"
MAT_NAME = "M_InteractFocusOverlay"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "make_focus_overlay_mat.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    mel = unreal.MaterialEditingLibrary
    at = unreal.AssetToolsHelpers.get_asset_tools()
    path = MAT_DIR + "/" + MAT_NAME
    existing = unreal.EditorAssetLibrary.does_asset_exist(path)
    report["existed"] = existing
    if existing:
        mat = unreal.EditorAssetLibrary.load_asset(path)
    else:
        mat = at.create_asset(MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("material create failed")
    call(mat.set_editor_property, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    call(mat.set_editor_property, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    call(mat.set_editor_property, "two_sided", False)
    fres = call(mel.create_material_expression, mat, unreal.MaterialExpressionFresnel, -700, 0)
    call(fres.set_editor_property, "exponent", 4.0)
    call(fres.set_editor_property, "base_reflect_fraction", 0.05)
    col = call(mel.create_material_expression, mat, unreal.MaterialExpressionVectorParameter, -1000, -200)
    call(col.set_editor_property, "parameter_name", "FocusColor")
    call(col.set_editor_property, "default_value", unreal.LinearColor(0.25, 0.75, 1.0, 1.0))
    inten = call(mel.create_material_expression, mat, unreal.MaterialExpressionScalarParameter, -1000, 60)
    call(inten.set_editor_property, "parameter_name", "FocusIntensity")
    call(inten.set_editor_property, "default_value", 3.0)
    mul1 = call(mel.create_material_expression, mat, unreal.MaterialExpressionMultiply, -400, -120)
    mul2 = call(mel.create_material_expression, mat, unreal.MaterialExpressionMultiply, -180, -40)
    report["nodes"] = [str(fres), str(col), str(inten)]
    report["link1"] = str(call(mel.connect_material_expressions, fres, "", mul1, "A"))[:30]
    report["link2"] = str(call(mel.connect_material_expressions, col, "", mul1, "B"))[:30]
    report["link3"] = str(call(mel.connect_material_expressions, mul1, "", mul2, "A"))[:30]
    report["link4"] = str(call(mel.connect_material_expressions, inten, "", mul2, "B"))[:30]
    report["emit"] = str(call(mel.connect_material_property, mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR))[:30]
    report["opacity"] = str(call(mel.connect_material_property, fres, "", unreal.MaterialProperty.MP_OPACITY))[:30]
    usages = []
    for u in ("MATUSAGE_SKELETAL_MESH", "MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_STATIC_LIGHTING"):
        if hasattr(unreal.MaterialUsage, u):
            r = call(mel.set_material_usage, mat, getattr(unreal.MaterialUsage, u))
            usages.append(u + ":" + str(r)[:20])
    report["usages"] = usages
    report["recompile"] = str(call(mel.recompile_material, mat))[:20]
    report["save"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, mat))[:20]
    report["verify"] = {"path": path,
                        "domain": str(gp(mat, "material_domain")),
                        "blend": str(gp(mat, "blend_mode")),
                        "shading": str(gp(mat, "shading_model")),
                        "expr_count": str(call(mel.get_num_material_expressions, mat))}

    # 关卡里可交互物用没用 Nanite
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    nan = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not (lbl.startswith("men") or lbl.startswith("jiguandeng") or lbl.startswith("kongjianchuansuoqi") or lbl.startswith("jiguanqiang")):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "StaticMeshComponent" not in str(call(c.get_class().get_name)):
                continue
            sm = gp(c, "StaticMesh")
            if sm is None:
                continue
            ns = gp(sm, "nanite_settings")
            nan[lbl] = str(ns)[:90]
            break
    report["nanite"] = dict(list(nan.items())[:12])
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###MAT " + json.dumps(report, ensure_ascii=True)[:1500])