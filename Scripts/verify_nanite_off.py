import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_nanite_off.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:70]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    ids = {}
    mat = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/M_InteractFocusOverlay")
    report["material_expr"] = str(call(unreal.MaterialEditingLibrary.get_num_material_expressions, mat))
    report["material_blend"] = str(gp(mat, "blend_mode"))
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        has_inter = False
        for c in comps:
            if "InteractableComponent" in str(call(c.get_class().get_name)):
                if gp(c, "bEnabled") is True:
                    has_inter = True
        if not has_inter:
            continue
        for c in comps:
            if "MeshComponent" not in str(call(c.get_class().get_name)):
                continue
            sm = gp(c, "StaticMesh")
            if sm is None:
                continue
            ns = gp(sm, "nanite_settings")
            en = gp(ns, "enabled") if ns is not None else None
            key = str(sm)[:60]
            if key not in ids:
                ids[key] = {"nanite": str(en), "count": 0}
            ids[key]["count"] = ids[key]["count"] + 1
    report["enabled_interactable_meshes"] = ids
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("VERIFY done")