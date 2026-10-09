"""Read-only probe #2: dump the door Blueprint's subobject handles and accessors."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "probe_bp_scs2.json")

report = {}


def log(m):
    unreal.log("[SCS2] " + str(m))


def describe(d):
    out = {}
    for meth in ("get_display_name", "get_variable_name", "get_object", "get_component_template",
                 "get_class", "get_data", "get_outer", "get_full_name", "get_children",
                 "is_component", "is_root_component", "get_handle", "get_object_for_blueprint",
                 "get_blueprint", "is_valid", "get_editor_property"):
        f = getattr(d, meth, None)
        if f is None:
            out[meth] = "<absent>"
            continue
        try:
            out[meth] = str(f())
        except Exception as exc:
            out[meth] = "ERR " + repr(exc)[:110]
    out["_methods"] = sorted([m for m in dir(d) if not m.startswith("_")])
    return out


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    report["bp"] = None if bp is None else bp.get_name()
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    report["handle_type_methods"] = sorted(
        [m for m in dir(unreal.SubobjectDataHandle) if not m.startswith("_")])

    res = subsys.k2_gather_subobject_data_for_blueprint(bp)
    report["res_len"] = len(res)
    report["res_types"] = [type(x).__name__ for x in res]

    handles = []
    for item in res:
        try:
            handles.extend(list(item))
        except Exception:
            handles.append(item)
    report["handle_count"] = len(handles)

    items = []
    for h in handles:
        e = {}
        try:
            d = subsys.k2_find_subobject_data_from_handle(h)
            e["data"] = describe(d)
        except Exception as exc:
            e["data_err"] = repr(exc)
            e["tb"] = traceback.format_exc().split("\n")[-3:]
        items.append(e)
    report["items"] = items


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("done")
