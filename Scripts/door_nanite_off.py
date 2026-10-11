import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "door_nanite_off.json")
P = "/Game/Fab/Modern_Door/Door"
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:90]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    sm = unreal.EditorAssetLibrary.load_asset(P)
    ns = gp(sm, "nanite_settings")
    call(ns.set_editor_property, "enabled", False)
    call(sm.set_editor_property, "nanite_settings", ns)
    report["nanite_now"] = str(gp(gp(sm, "nanite_settings"), "enabled"))
    report["dirty"] = str(call(unreal.EditorAssetLibrary.does_asset_exist, P))
    report["save_asset"] = str(call(unreal.EditorAssetLibrary.save_asset, P, False))[:20]
    pkg = call(unreal.EditorAssetLibrary.find_asset_data, P)
    report["save_loaded"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, sm))[:20]
    if pkg is not None:
        report["save_dir"] = str(call(unreal.EditorAssetLibrary.save_directory, "/Game/Fab/Modern_Door", False, True))[:20]
    sm2 = unreal.EditorAssetLibrary.load_asset(P)
    report["nanite_reloaded"] = str(gp(gp(sm2, "nanite_settings"), "enabled"))
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("DOOR NANITE OFF done")