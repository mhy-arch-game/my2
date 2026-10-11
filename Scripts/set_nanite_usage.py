import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "set_nanite_usage.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    mel = unreal.MaterialEditingLibrary
    mat = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/M_InteractFocusOverlay")
    report["nanite_usage"] = str(call(mel.set_material_usage, mat, unreal.MaterialUsage.MATUSAGE_NANITE))[:20]
    report["recompile"] = str(call(mel.recompile_material, mat))[:20]
    report["save"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, mat))[:20]
    report["expr_count"] = str(call(mel.get_num_material_expressions, mat))
    report["blend"] = str(gp(mat, "blend_mode"))
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###NANITE " + json.dumps(report, ensure_ascii=True)[:500])