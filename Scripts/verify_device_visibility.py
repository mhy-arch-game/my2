import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_device_visibility.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    bp = unreal.EditorAssetLibrary.load_asset("/Game/bclass_source/modern_swift_actor")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        cls = obj.get_class().get_name()
        if cls == "TimeEraComponent":
            report["template_both_eras"] = str(call(obj.get_editor_property, "bExistsInBothEras")[1])
        elif cls == "StaticMeshComponent":
            report["template_profile"] = str(call(obj.get_collision_profile_name)[1])

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    would_hide_ancient = []
    would_hide_modern = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        era = a.get_component_by_class(unreal.TimeEraComponent)
        r = {"label": lbl}
        if era is not None:
            both = call(era.get_editor_property, "bExistsInBothEras")[1]
            ev = str(call(era.get_editor_property, "Era")[1])
            r["both"] = str(both)
            r["Era"] = ev
            if not both:
                (would_hide_ancient if "MODERN" in ev else would_hide_modern).append(lbl)
        for m in a.get_components_by_class(unreal.StaticMeshComponent):
            r["profile"] = str(call(m.get_collision_profile_name)[1])
        rows.append(r)
    rows.sort(key=lambda x: int("".join(c for c in x["label"] if c.isdigit()) or 0))
    report["devices"] = rows
    report["hidden_if_era_ancient"] = would_hide_ancient
    report["hidden_if_era_modern"] = would_hide_modern
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY DEVICE VISIBILITY done]")
