import json, os, traceback, datetime
import unreal
MAP = "/Game/FirstPerson/firstvision"
BP = "/Game/bclass_source/jumping_area"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "apply_gravity_preset.json")
H = 1125.0
K = 0.32
report = {"target_height": H, "new_scale": K}
lib = unreal.EditorAssetLibrary
def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:160], None)
def setp(o, name, val):
    e, r = call(o.set_editor_property, name, val, notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    return [name, e, ("" if e == "ok" else str(r)[:60])]
try:
    ue = "E:/BaiduNetdiskDownload/my2/Content/FirstPerson/firstvision.umap"
    report["mtime_before"] = str(datetime.datetime.fromtimestamp(os.path.getmtime(ue))) if os.path.exists(ue) else "n/a"
    bp = lib.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    w = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "GravityZoneComponent":
            continue
        call(obj.modify)
        w.append(setp(obj, "TargetJumpHeight", H))
        w.append(setp(obj, "GravityScaleInside", K))
    report["template"] = w
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(lib.save_loaded_asset, bp)[0]

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    wl = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if "jumping_area" not in lbl.lower():
            continue
        call(a.modify)
        for c in a.get_components_by_class(unreal.GravityZoneComponent):
            call(c.modify)
            wl.append([lbl, setp(c, "TargetJumpHeight", H), setp(c, "GravityScaleInside", K)])
    report["level"] = wl
    try:
        w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        report["save_map"] = str(unreal.EditorLoadingAndSavingUtils.save_map(w, MAP))
    except Exception as exc:
        report["save_map_err"] = repr(exc)[:160]
    report["mtime_after"] = str(datetime.datetime.fromtimestamp(os.path.getmtime(ue))) if os.path.exists(ue) else "n/a"
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[APPLY saved]")
