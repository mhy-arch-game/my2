import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_era.json")
report = {}
ERA_P = ("Era", "bExistsInBothEras", "bGateVisibility", "bGateCollision", "bGateTick")
PORTAL_P = ("bSwitchEra", "bAutoDetectEra", "OwnerEra", "TargetMode", "VerticalOffset", "bRegisterAsAnchor", "CounterpartId")


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:110], None)


try:
    # subsystem class defaults / config
    sub = unreal.load_class(None, "/Script/MHY_ARCH_GAME.TimeShiftSubsystem")
    report["subsystem_class"] = str(sub)
    cdo = unreal.get_default_object(sub) if sub else None
    if cdo:
        for p in ("InitialEra", "CooldownDuration"):
            report["subsys_" + p] = str(call(cdo.get_editor_property, p)[1])

    # blueprint template
    bp = unreal.EditorAssetLibrary.load_asset("/Game/bclass_source/modern_swift_actor")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    tmpl = {}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        cls = obj.get_class().get_name()
        if cls == "TimeEraComponent":
            tmpl["TimeEra"] = {p: str(call(obj.get_editor_property, p)[1]) for p in ERA_P}
        elif cls == "TimeEraPortalComponent":
            tmpl["Portal"] = {p: str(call(obj.get_editor_property, p)[1]) for p in PORTAL_P}
        elif cls == "StaticMeshComponent":
            tmpl["Mesh"] = {"bHiddenInGame": str(call(obj.get_editor_property, "bHiddenInGame")[1]),
                            "bVisible": str(call(obj.get_editor_property, "bVisible")[1]),
                            "profile": str(call(obj.get_collision_profile_name)[1])}
    report["template"] = tmpl

    # level devices
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    era_actors = {"Ancient": 0, "Modern": 0, "both": 0, "none": 0}
    for a in actor_ss.get_all_level_actors():
        era = a.get_component_by_class(unreal.TimeEraComponent)
        if era is not None:
            e, v = call(era.get_editor_property, "bExistsInBothEras")
            if e == "ok" and v:
                era_actors["both"] += 1
            else:
                e2, ev = call(era.get_editor_property, "Era")
                key = "Ancient" if "ANCIENT" in str(ev) else ("Modern" if "MODERN" in str(ev) else "none")
                era_actors[key] = era_actors.get(key, 0) + 1
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        row = {"label": lbl, "actor_hidden": str(call(a.is_hidden)[1]),
               "actor_class": a.get_class().get_name()}
        if era is not None:
            row["era"] = {p: str(call(era.get_editor_property, p)[1]) for p in ERA_P}
        else:
            row["era"] = "NONE"
        p = a.get_component_by_class(unreal.TimeEraPortalComponent)
        if p is not None:
            row["portal"] = {k: str(call(p.get_editor_property, k)[1]) for k in PORTAL_P}
            row["owner_era_resolved"] = str(call(p.get_editor_property, "OwnerEra")[1])
        for m in a.get_components_by_class(unreal.StaticMeshComponent):
            row["mesh"] = {"comp": m.get_name(),
                           "bHiddenInGame": str(call(m.get_editor_property, "bHiddenInGame")[1]),
                           "visible_in_editor": str(call(m.is_visible)[1])}
        rows.append(row)
    rows.sort(key=lambda x: int("".join(c for c in x["label"] if c.isdigit()) or 0))
    report["devices"] = rows
    report["level_era_component_counts"] = era_actors
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE ERA done]")
