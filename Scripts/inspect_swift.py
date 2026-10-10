"""Inspect kongjianchuansong actors and the swift_actor blueprints."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "inspect_swift.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def v3(v):
    return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]


def bp_components(path):
    bp = lib.load_asset(path)
    if bp is None:
        return {"error": "not found"}
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    out = {"bp": bp.get_name(), "components": [], "templates": {}}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        err3, obj = call(flib.get_object, data)
        name = str(var) if err2 == "ok" else "?"
        cls = None
        if err3 == "ok" and obj is not None:
            cls = obj.get_class().get_name()
        out["components"].append([name, cls])
        if err3 != "ok" or obj is None:
            continue
        # dump the interesting props per component kind
        props = {}
        for p in ("Era", "bExistsInBothEras", "TargetMode", "VerticalOffset", "CounterpartId",
                  "CounterpartActor", "bAutoDetectEra", "OwnerEra", "InteractionPrompt",
                  "bAutoUseInteractableOnOwner", "bSuppressBuiltInToggle", "TeleportOffset",
                  "bPlaceOnGround", "bMatchCounterpartYaw", "PortalCooldown",
                  "bUseBuiltInToggle", "ToggleComponentNames", "bEnabled"):
            e, v = call(obj.get_editor_property, p)
            if e == "ok":
                props[p] = str(v)
        if props:
            out["templates"][name] = props
    return out


def main():
    report["modern_swift_actor"] = bp_components("/Game/bclass_source/modern_swift_actor")
    report["ancient_swift_actor"] = bp_components("/Game/bclass_source/ancient_swift_actor")

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)

    hits = []
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if "kongjian" not in lbl.lower() and "chuansong" not in lbl.lower():
            continue
        e = {"label": lbl, "class": a.get_class().get_name()}
        e["loc"] = v3(a.get_actor_location())
        e["rot"] = [round(float(a.get_actor_rotation().roll), 1),
                    round(float(a.get_actor_rotation().pitch), 1),
                    round(float(a.get_actor_rotation().yaw), 1)]
        e["scale"] = v3(a.get_actor_scale3d())
        comps = []
        for c in a.get_components_by_class(unreal.ActorComponent):
            comps.append(c.get_name() + ":" + c.get_class().get_name())
        e["components"] = comps
        hits.append(e)
    report["kongjian_actors"] = hits
    report["kongjian_count"] = len(hits)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
