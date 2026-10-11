import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "nanite_off.json")
TARGETS = ["/Game/Fab/Modern_Door/Door",
           "/Game/models/tripo_convert_a27cf087-650e-439c-80cf-d55ede9a4be3"]
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:80]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    report["subsystem"] = str(sub)
    rows = []
    for p in TARGETS:
        sm = unreal.EditorAssetLibrary.load_asset(p)
        if sm is None:
            rows.append({"path": p, "err": "load failed"})
            continue
        info = {"path": p,
                "lods": str(call(sm.get_num_lods)),
                "tris_lod0": str(call(sm.get_num_triangles, 0)),
                "nanite_before": str(gp(gp(sm, "nanite_settings"), "enabled"))}
        ns = gp(sm, "nanite_settings")
        if ns is not None and gp(ns, "enabled") is True:
            call(ns.set_editor_property, "enabled", False)
            info["set_result"] = str(call(sub.set_nanite_settings, sm, ns, True))[:20]
            info["nanite_after"] = str(gp(gp(sm, "nanite_settings"), "enabled"))
            info["save"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, sm))[:20]
        rows.append(info)
    report["rows"] = rows
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("NANITE OFF done")