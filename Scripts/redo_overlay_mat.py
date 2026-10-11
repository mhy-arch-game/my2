import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "redo_overlay_mat.json")
PATH = "/Game/MHY_ARCH_GAME/UI/M_InteractFocusOverlay"
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(PATH):
        report["deleted"] = str(call(unreal.EditorAssetLibrary.delete_asset, PATH))[:20]
    at = unreal.AssetToolsHelpers.get_asset_tools()
    mat = at.create_asset("M_InteractFocusOverlay", "/Game/MHY_ARCH_GAME/UI", unreal.Material, unreal.MaterialFactoryNew())
    call(mat.set_editor_property, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    call(mat.set_editor_property, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    call(mat.set_editor_property, "two_sided", False)
    # 关键修正：不透明度 = clamp(Fresnel + BaseFill, 0, 1)，BaseFill 默认 0.45
    # 只用 Fresnel 的话正对表面时几乎为 0，看起来"没有效果"。
    fres = call(mel.create_material_expression, mat, unreal.MaterialExpressionFresnel, -900, 40)
    call(fres.set_editor_property, "exponent", 3.0)
    call(fres.set_editor_property, "base_reflect_fraction", 0.05)
    fill = call(mel.create_material_expression, mat, unreal.MaterialExpressionScalarParameter, -1150, 220)
    call(fill.set_editor_property, "parameter_name", "FocusBaseFill")
    call(fill.set_editor_property, "default_value", 0.45)
    add = call(mel.create_material_expression, mat, unreal.MaterialExpressionAdd, -700, 140)
    clamp = call(mel.create_material_expression, mat, unreal.MaterialExpressionClamp, -520, 140)
    col = call(mel.create_material_expression, mat, unreal.MaterialExpressionVectorParameter, -1150, -160)
    call(col.set_editor_property, "parameter_name", "FocusColor")
    call(col.set_editor_property, "default_value", unreal.LinearColor(0.2, 0.7, 1.0, 1.0))
    inten = call(mel.create_material_expression, mat, unreal.MaterialExpressionScalarParameter, -1150, -20)
    call(inten.set_editor_property, "parameter_name", "FocusIntensity")
    call(inten.set_editor_property, "default_value", 2.5)
    m1 = call(mel.create_material_expression, mat, unreal.MaterialExpressionMultiply, -340, 60)
    m2 = call(mel.create_material_expression, mat, unreal.MaterialExpressionMultiply, -170, 0)
    ops = []
    ops.append(str(call(mel.connect_material_expressions, fres, "", add, "A"))[:20])
    ops.append(str(call(mel.connect_material_expressions, fill, "", add, "B"))[:20])
    ops.append(str(call(mel.connect_material_expressions, add, "", clamp, ""))[:20])
    ops.append(str(call(mel.connect_material_expressions, clamp, "", m1, "A"))[:20])
    ops.append(str(call(mel.connect_material_expressions, inten, "", m1, "B"))[:20])
    ops.append(str(call(mel.connect_material_expressions, m1, "", m2, "A"))[:20])
    ops.append(str(call(mel.connect_material_expressions, col, "", m2, "B"))[:20])
    ops.append(str(call(mel.connect_material_property, m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR))[:20])
    ops.append(str(call(mel.connect_material_property, clamp, "", unreal.MaterialProperty.MP_OPACITY))[:20])
    report["links"] = ops
    us = []
    for u in ("MATUSAGE_SKELETAL_MESH", "MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_NANITE"):
        if hasattr(unreal.MaterialUsage, u):
            us.append(u + ":" + str(call(mel.set_material_usage, mat, getattr(unreal.MaterialUsage, u)))[:12])
    report["usages"] = us
    report["expr_count"] = str(call(mel.get_num_material_expressions, mat))
    report["recompile"] = str(call(mel.recompile_material, mat))[:20]
    report["save"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, mat))[:20]
    report["verify"] = {"blend": str(gp(mat, "blend_mode")), "shading": str(gp(mat, "shading_model"))}

    # 全部可交互物清单：网格 / Nanite / 是否可聚焦（Interactable.bEnabled）
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        inter = None
        meshes = []
        for c in comps:
            cn = str(call(c.get_class().get_name))
            if "InteractableComponent" in cn:
                inter = str(gp(c, "bEnabled"))
            if "StaticMeshComponent" in cn or "SkeletalMeshComponent" in cn:
                sm = gp(c, "StaticMesh")
                en = None
                if sm is not None:
                    ns = gp(sm, "nanite_settings")
                    if ns is not None:
                        en = gp(ns, "enabled")
                meshes.append({"mesh": str(sm)[:34] if sm is not None else "?", "nanite": str(en)})
        if inter is None and not meshes:
            continue
        if not (lbl.startswith("men") or lbl.startswith("jiguandeng") or lbl.startswith("kongjianchuansuoqi") or lbl.startswith("jiguanqiang") or lbl.startswith("jiguankuai")):
            continue
        rows.append({"label": lbl, "enabled": inter, "meshes": meshes[:2],
                     "class": str(call(a.get_class().get_name))})
    report["interactables"] = rows[:30]
    report["interactable_count"] = len(rows)
    nan_on = [r["label"] for r in rows if any(m["nanite"] == "True" for m in r["meshes"])]
    report["nanite_on"] = nan_on[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###REDO " + json.dumps(report, ensure_ascii=True)[:300])