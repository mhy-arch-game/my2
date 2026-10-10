import json, os, traceback
import unreal
BP = "/Game/bclass_source/modern_swift_actor"
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_device_visibility.json")
report = {}
PROFILE = "BlockAllDynamic"


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    # ---------- 1) blueprint template ----------
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    tpl = {"era": None, "meshes": []}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        cls = obj.get_class().get_name()
        if cls == "TimeEraComponent":
            before = str(call(obj.get_editor_property, "bExistsInBothEras")[1])
            call(obj.modify)
            e, _ = call(obj.set_editor_property, "bExistsInBothEras", True,
                        notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
            tpl["era"] = {"before": before, "set": e,
                          "after": str(call(obj.get_editor_property, "bExistsInBothEras")[1])}
        elif cls == "StaticMeshComponent":
            before = str(call(obj.get_collision_profile_name)[1])
            call(obj.modify)
            call(obj.set_collision_profile_name, PROFILE)
            tpl["meshes"].append({"comp": obj.get_name(), "before": before,
                                  "after": str(call(obj.get_collision_profile_name)[1])})
    report["template"] = tpl
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---------- 2) level instances (plain setter + save_current_level) ----------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        era = a.get_component_by_class(unreal.TimeEraComponent)
        row = {"label": lbl}
        if era is not None:
            call(era.modify)
            call(era.set_editor_property, "bExistsInBothEras", True)
            row["both_after"] = str(call(era.get_editor_property, "bExistsInBothEras")[1])
        for m in a.get_components_by_class(unreal.StaticMeshComponent):
            call(m.modify)
            call(m.set_collision_profile_name, PROFILE)
            row["profile_after"] = str(call(m.get_collision_profile_name)[1])
        rows.append(row)
    report["instances"] = rows
    report["device_count"] = len(rows)
    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX DEVICE VISIBILITY done]")
