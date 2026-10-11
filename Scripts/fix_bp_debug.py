import json, os, traceback
import unreal
BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_bp_debug.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    scs = gp(bp, "SimpleConstructionScript")
    nodes = list(call(scs.get_all_nodes) or []) if scs is not None else []
    report["node_count"] = len(nodes)
    vals = {}
    fixed = []
    for n in nodes:
        t = gp(n, "ComponentTemplate")
        if t is None:
            continue
        cn = str(call(t.get_class().get_name))
        v = gp(t, "bDrawDebug")
        if v is not None:
            vals[cn] = str(v)
            if v is True:
                call(t.set_editor_property, "bDrawDebug", False)
                fixed.append(cn)
    report["template_bDrawDebug"] = vals
    report["fixed_templates"] = fixed
    if fixed:
        report["compile"] = str(call(unreal.BlueprintEditorLibrary.compile_blueprint, bp))[:20]
        report["save"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, bp))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###BP " + json.dumps(report, ensure_ascii=True)[:800])