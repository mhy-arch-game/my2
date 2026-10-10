# -*- coding: utf-8 -*-
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "probe_gravity.json")
report = {}
lib = unreal.EditorAssetLibrary

GZ_PROPS = ("GravityScaleInside", "bAffectPawnsOnly", "bScaleJumpVelocity",
            "bDrawDebug", "bDrawOnScreenDebug", "BoxExtent", "bHiddenInGame",
            "bAutoActivate", "bAffectPawnsOnly")
BP_PROPS = ("Era", "InteractionPrompt", "TargetMode", "VerticalOffset",
            "bUseBuiltInToggle", "ToggleComponentNames", "bEnabled")


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:140], None)


def v3(v):
    return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]


def dump_props(obj, props):
    out = {}
    for p in props:
        e, v = call(obj.get_editor_property, p)
        if e == "ok":
            try:
                out[p] = str(v)
            except Exception:
                out[p] = "<unprintable>"
    return out


def bp_components(path):
    bp = lib.load_asset(path)
    if bp is None:
        return {"error": "load_asset returned None"}
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    out = {"bp": bp.get_name(), "components": [], "props": {}}
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        err3, obj = call(flib.get_object, data)
        name = str(var) if err2 == "ok" else "?"
        cls = obj.get_class().get_name() if (err3 == "ok" and obj is not None) else "?"
        out["components"].append([name, cls])
        if obj is None:
            continue
        if "Gravity" in cls:
            out["props"][name] = dump_props(obj, GZ_PROPS)
        elif any(k in cls for k in ("Interactable", "TimeEra", "Revealable", "Reveal")):
            out["props"][name] = dump_props(obj, BP_PROPS)
    return out


def main():
    for p in ("jumping_area", "reveal_wall", "active_light", "modern_swift_actor"):
        report[p] = bp_components("/Game/bclass_source/" + p)

    # character blueprint: does the player carry a gravity zone?
    report["char"] = bp_components("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["level_actor_count"] = len(actors)
    gz = []
    for a in actors:
        comps = a.get_components_by_class(unreal.GravityZoneComponent)
        if not comps:
            continue
        for c in comps:
            e = {"label": a.get_actor_label(), "actor_class": a.get_class().get_name(),
                 "loc": v3(a.get_actor_location()), "scale": v3(a.get_actor_scale3d()),
                 "comp": c.get_name()}
            e.update(dump_props(c, GZ_PROPS))
            gz.append(e)
    report["level_gravity_zones"] = gz
    report["level_gravity_zone_count"] = len(gz)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[GRAVITY-PROBE saved]")
