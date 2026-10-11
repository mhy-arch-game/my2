import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "check_frames.json")
report = {}
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    for p in ("/Game/Fab/Modern_Door/Door_Frame", "/Game/Fab/Modern_Door/modern_door"):
        sm = unreal.EditorAssetLibrary.load_asset(p)
        ns = gp(sm, "nanite_settings")
        report[p] = {"nanite": str(gp(ns, "enabled")) if ns is not None else "?",
                     "tris": str(sm.get_num_triangles(0)),
                     "lods": str(sm.get_num_lods())}
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("CHECK done")