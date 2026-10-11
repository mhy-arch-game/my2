import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "diag_scs.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:120]
def dump_obj(o):
    if o is None: return None
    for attr in ("get_name", "get_full_name"):
        v = getattr(o, attr, None)
        if v: return str(call(v))[:120]
    return str(o)[:120]
for path in ["/Game/bclass_source/end_of_the_game", "/Game/bclass_source/active_door"]:
    rep[path] = {}
    bp = unreal.EditorAssetLibrary.load_asset(path)
    if bp is None:
        rep[path]["err"] = "load failed"
        continue
    try:
        scs = call(bp.get_editor_property, "simple_construction_script")
        nodes = call(scs.get_all_nodes) if not isinstance(scs, str) else scs
        items = []
        if not isinstance(nodes, str):
            for n in nodes:
                items.append({"var": str(call(n.get_editor_property, "variable_name")),
                              "class": dump_obj(call(n.get_editor_property, "component_class")),
                              "is_root": str(call(n.get_editor_property, "is_root_component"))})
        rep[path]["scs_nodes"] = items
    except Exception as exc:
        rep[path]["scs_err"] = repr(exc)[:150]
    try:
        sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = sub.k2_gather_subobject_data_for_blueprint(bp)
        arr = []
        for h in handles:
            d = sub.k2_find_subobject_data_from_handle(h)
            t = call(d.to_tuple)
            arr.append(str(t)[:220])
        rep[path]["subobjects"] = arr
    except Exception as exc:
        rep[path]["sub_err"] = repr(exc)[:150]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("DIAG SCS done")