import json, os, traceback
import unreal
BP = "/Game/bclass_source/modern_swift_actor"
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "apply_portal_collision.json")
PROFILE = "IgnoreOnlyPawn"
report = {"profile": PROFILE}
lib = unreal.EditorAssetLibrary


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:150], None)


def state(c):
    out = {}
    out["profile"] = str(call(c.get_collision_profile_name)[1])
    out["pawn"] = str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)[1])
    out["vis"] = str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_VISIBILITY)[1])
    return out


try:
    # ---- 1) blueprint SCS template (new instances inherit it) ----
    bp = lib.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    tpl = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "StaticMeshComponent":
            continue
        row = {"comp": obj.get_name(), "before": state(obj)}
        call(obj.modify)
        row["set"] = call(obj.set_collision_profile_name, PROFILE)[0]
        row["after"] = state(obj)
        tpl.append(row)
    report["template"] = tpl
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(lib.save_loaded_asset, bp)[0]

    # ---- 2) level instances (proven path: plain setter + save_current_level) ----
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    inst = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            row = {"label": lbl, "comp": c.get_name(), "before": state(c)}
            row["set"] = call(c.set_collision_profile_name, PROFILE)[0]
            inst.append(row)
    report["instances_written"] = len(inst)
    report["instances_after"] = [dict(r, after=state(a)) for r, a in zip(inst, [])] if False else None
    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PORTAL COLLISION saved]")
