import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "restore_source.json")
report = {"meshes": [], "facts": {}}


def v3(v):
    try:
        return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]
    except Exception:
        return str(v)[:50]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:110], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)
    for a in actors:
        try:
            path = a.get_path_name()
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            report["meshes"].append({
                "actor": path, "label": lbl, "comp": c.get_name(),
                "profile": str(call(c.get_collision_profile_name)[1]),
                "pawn": str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)[1]),
            })
    # key facts, to prove which save this really is
    f = {}
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl == "jumping_area":
            gz = a.get_component_by_class(unreal.GravityZoneComponent)
            if gz:
                f["jumping_area.TargetJumpHeight"] = str(call(gz.get_editor_property, "TargetJumpHeight")[1])
                f["jumping_area.GravityScaleInside"] = str(call(gz.get_editor_property, "GravityScaleInside")[1])
        if lbl in ("kongjianchuansuoqi1", "kongjianchuansuoqi2"):
            era = a.get_component_by_class(unreal.TimeEraComponent)
            portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
            if era:
                f[lbl + ".Era"] = str(call(era.get_editor_property, "Era")[1])
            if portal:
                f[lbl + ".TargetMode"] = str(call(portal.get_editor_property, "TargetMode")[1])
                f[lbl + ".VerticalOffset"] = str(call(portal.get_editor_property, "VerticalOffset")[1])
        if lbl in ("men1", "men18", "men25"):
            ic = a.get_component_by_class(unreal.InteractableComponent)
            if ic:
                f[lbl + ".ToggleComponentNames"] = str(call(ic.get_editor_property, "ToggleComponentNames")[1])
                f[lbl + ".RotationPivot"] = str(call(ic.get_editor_property, "RotationPivot")[1])
                f[lbl + ".bUseAxisRotation"] = str(call(ic.get_editor_property, "bUseAxisRotation")[1])
    report["facts"] = f
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[DUMP PROFILES done]")
