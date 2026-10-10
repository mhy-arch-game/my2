"""Probe the Python API for editing a Blueprint's component list (SCS). Read-only."""
import json
import os
import traceback

import unreal

BP_PATH = "/Game/bclass_source/active_door"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "probe_bp_scs.json")

report = {}


def log(m):
    unreal.log("[PROBESCS] " + str(m))


def main():
    report["SubobjectDataSubsystem_methods"] = sorted(
        [m for m in dir(unreal.SubobjectDataSubsystem) if not m.startswith("_")])
    report["SubobjectData_methods"] = sorted(
        [m for m in dir(unreal.SubobjectData) if not m.startswith("_")])
    report["SubobjectDataHandle_methods"] = sorted(
        [m for m in dir(unreal.SubobjectDataHandle) if not m.startswith("_")])
    report["BlueprintEditorLibrary_methods"] = sorted(
        [m for m in dir(unreal.BlueprintEditorLibrary) if not m.startswith("_")])

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    report["bp"] = None if bp is None else bp.get_name()
    if bp is None:
        return

    gc = bp.generated_class()
    report["generated_class"] = None if gc is None else gc.get_name()
    if gc is not None:
        cdo = unreal.get_default_object(gc)
        report["cdo"] = None if cdo is None else cdo.get_name()
        if cdo is not None:
            report["cdo_components"] = [
                [c.get_name(), c.get_class().get_name()]
                for c in cdo.get_components_by_class(unreal.ActorComponent)]

    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    report["subsys"] = None if subsys is None else type(subsys).__name__
    if subsys is None:
        return

    try:
        res = subsys.k2_gather_subobject_data_for_blueprint(bp)
        handles = res[0]
        report["handle_count"] = len(handles)
        items = []
        for h in handles:
            d = subsys.k2_find_subobject_data_from_handle(h)
            items.append({
                "display": str(d.get_display_name()),
                "var": str(d.get_variable_name()),
                "object": str(d.get_object()),
                "template": str(d.get_component_template()),
            })
        report["items"] = items
    except Exception as exc:
        report["gather_error"] = repr(exc)
        report["gather_traceback"] = traceback.format_exc()


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
log("probe done")
