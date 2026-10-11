import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_overlay.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    cls = unreal.InteractionDetectorComponent
    cdo = unreal.get_default_object(cls)
    report["cdo_class"] = str(call(cdo.get_class().get_name))
    report["cdo_bUseOverlayHighlight"] = str(gp(cdo, "bUseOverlayHighlight"))
    report["cdo_FocusOverlayMaterial"] = str(gp(cdo, "FocusOverlayMaterial"))[:110]
    report["cdo_bUseStencilOutline"] = str(gp(cdo, "bUseStencilOutline"))
    mat = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/M_InteractFocusOverlay")
    report["mat"] = str(mat)[:80]
    if not isinstance(mat, str) and mat:
        report["mat_domain"] = str(gp(mat, "material_domain"))
        report["mat_blend"] = str(gp(mat, "blend_mode"))
        report["mat_shading"] = str(gp(mat, "shading_model"))
        report["mat_usage_nanite_flag"] = str([n for n in dir(unreal.MaterialUsage) if "NANITE" in n.upper()])
    # 可交互物的网格与 Nanite 状态
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    info = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not (lbl.startswith("men") or lbl.startswith("jiguandeng") or lbl.startswith("kongjianchuansuoqi") or lbl in ("jiguanqiang1", "jiguanqiang2")):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "StaticMeshComponent" not in str(call(c.get_class().get_name)):
                continue
            sm = gp(c, "StaticMesh")
            if sm is None:
                continue
            ns = gp(sm, "nanite_settings")
            en = gp(ns, "enabled") if ns is not None else None
            info[lbl] = {"mesh": str(sm)[:44], "nanite": str(en)}
            break
    report["interactables"] = dict(list(info.items())[:14])
    report["total_interactable_meshes"] = len(info)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###VERIFY " + json.dumps(report, ensure_ascii=True)[:1600])