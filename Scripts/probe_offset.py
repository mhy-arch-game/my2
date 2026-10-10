import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_offset.json")
report = {}
lib = unreal.EditorAssetLibrary
P = ("TargetMode", "VerticalOffset", "TeleportOffset", "bPlaceOnGround", "GroundTraceDistance",
     "GroundClearance", "bMatchCounterpartYaw", "bSwitchEra", "PortalCooldown", "CounterpartId",
     "bAutoDetectEra", "OwnerEra", "bRequireCounterpartInOtherEra", "bAutoUseInteractableOnOwner",
     "bSuppressBuiltInToggle", "bRegisterAsAnchor", "TransitionDelay", "bDisableInteractableWhileLocked")


def v3(v):
    return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    # character capsule
    cbp = lib.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(cbp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        if obj.get_class().get_name() == "CapsuleComponent":
            report["capsule"] = {"name": obj.get_name(),
                                 "half_height": str(call(obj.get_editor_property, "CapsuleHalfHeight")[1]),
                                 "radius": str(call(obj.get_editor_property, "CapsuleRadius")[1]),
                                 "rel_loc": str(call(obj.get_editor_property, "RelativeLocation")[1])}

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
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        era = a.get_component_by_class(unreal.TimeEraComponent)
        row = {"label": lbl, "class": a.get_class().get_name(), "loc": v3(a.get_actor_location()),
               "yaw": round(float(a.get_actor_rotation().yaw), 3)}
        e, ev = call(era.get_editor_property, "Era") if era else ("none", None)
        row["era"] = str(ev) if e == "ok" else str(e)
        if portal is not None:
            props = {}
            for p in P:
                e2, v = call(portal.get_editor_property, p)
                if e2 == "ok":
                    props[p] = str(v)
            row["portal"] = props
            e3, cp = call(portal.get_editor_property, "CounterpartActor")
            if e3 == "ok" and cp is not None:
                row["counterpart_label"] = call(cp.get_actor_label)[1]
        # every scene component, world + relative
        comps = []
        for c in a.get_components_by_class(unreal.SceneComponent):
            comps.append({"name": c.get_name(), "class": c.get_class().get_name(),
                          "world": v3(c.get_component_location()),
                          "rel": str(call(c.get_editor_property, "RelativeLocation")[1]),
                          "rel_rot": str(call(c.get_editor_property, "RelativeRotation")[1])})
        row["scene_components"] = comps
        rows.append(row)
    rows.sort(key=lambda r: int("".join(ch for ch in r["label"] if ch.isdigit()) or 0))
    report["actors"] = rows
    report["count"] = len(rows)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[OFFSET PROBE saved]")
