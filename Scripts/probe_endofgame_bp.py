import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_endofgame_bp.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:100]
def dump(path):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    if bp is None:
        return {"path": path, "err": "load failed"}
    gc = call(bp.generated_class)
    d = {"path": path, "bp": str(call(bp.get_name)), "generated": str(call(gc.get_name)) if gc else None,
         "parent_chain": []}
    ad = call(unreal.EditorAssetLibrary.find_asset_data, path)
    if not isinstance(ad, str):
        d["parent_tag"] = str(call(ad.get_tag_value, "ParentClass"))
        d["native_parent"] = str(call(ad.get_tag_value, "NativeParentClass"))
    if gc is not None:
        cdo = call(unreal.get_default_object, gc)
        d["cdo_components"] = [[str(call(x.get_name)), str(call(x.get_class().get_name))]
                               for x in (call(cdo.get_components_by_class, unreal.ActorComponent) or [])] if cdo else None
    try:
        sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        res = sub.k2_gather_subobject_data_for_blueprint(bp)
        items = []
        for h in res[0]:
            dd = sub.k2_find_subobject_data_from_handle(h)
            items.append({"disp": str(call(dd.get_display_name)), "var": str(call(dd.get_variable_name)),
                          "obj": str(call(dd.get_object))[:80], "tmpl": str(call(dd.get_component_template))[:80]})
        d["scs"] = items
    except Exception as exc:
        d["scs_err"] = repr(exc)
    return d
for p in ["/Game/bclass_source/end_of_the_game", "/Game/bclass_source/active_door", "/Game/bclass_source/modern_swift_actor"]:
    try:
        rep[p] = dump(p)
    except Exception as exc:
        rep[p] = {"err": repr(exc)}
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("PROBE ENDOFGAME BP done")