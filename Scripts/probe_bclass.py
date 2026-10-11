import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_bclass.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:90]
try:
    a = unreal.EditorAssetLibrary.load_asset("/Game/bclass_source/end_of_the_game")
    rep["loaded"] = str(a)[:160]
    rep["class"] = str(call(a.get_class().get_name)) if a else None
    if a is not None:
        rep["parent"] = str(call(a.get_editor_property, "parent_class"))[:120]
    import os as _os
    rep["files"] = []
    for root, dirs, files in _os.walk("E:/BaiduNetdiskDownload/my2/Content/bclass_source"):
        for f in files:
            rep["files"].append(_os.path.join(root, f).replace("E:/BaiduNetdiskDownload/my2/Content/", ""))
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("PROBE BCLASS done")