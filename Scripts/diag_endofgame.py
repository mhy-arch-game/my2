import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "diag_endofgame.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:110]
PATH = "/Game/bclass_source/end_of_the_game"
try:
    bp = unreal.EditorAssetLibrary.load_asset(PATH)
    rep["bp"] = str(call(bp.get_name))
    rep["status_before"] = str(call(bp.get_editor_property, "status"))
    # 蓝图变量
    try:
        vs = bp.get_editor_property("new_variables")
        rep["variables"] = [str(v.get_editor_property("var_name")) for v in vs]
    except Exception as exc:
        rep["variables_err"] = repr(exc)[:90]
    # SCS 子对象（这次直接迭代返回值，不取 [0]）
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    try:
        handles = sub.k2_gather_subobject_data_for_blueprint(bp)
        rep["handle_type"] = str(type(handles))[:60]
        items = []
        for h in handles:
            d = sub.k2_find_subobject_data_from_handle(h)
            items.append({"disp": str(call(d.get_display_name)),
                          "var": str(call(d.get_variable_name)),
                          "obj": str(call(d.get_object))[:90],
                          "tmpl": str(call(d.get_component_template))[:90]})
        rep["scs"] = items
    except Exception as exc:
        rep["scs_err"] = repr(exc)[:150]
    # 编译一次看错误
    call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)
    rep["status_after"] = str(call(bp.get_editor_property, "status"))
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("DIAG done")