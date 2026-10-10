import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_char_move.json")
report = {}
lib = unreal.EditorAssetLibrary
def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:150], None)
def dump(obj, props):
    out = {}
    for p in props:
        e, v = call(obj.get_editor_property, p)
        if e == "ok":
            out[p] = str(v)
    return out
P = ("JumpZVelocity", "GravityScale", "MaxWalkSpeed", "MaxWalkSpeedCrouched", "AirControl",
     "bApplyGravityWhileJumping", "JumpMaxHoldTime", "JumpMaxCount", "FallingLateralFriction")
try:
    bp = lib.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e2, var = call(flib.get_variable_name, d)
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        if obj.get_class().get_name() == "CharacterMovementComponent":
            report["scs_" + str(var)] = dump(obj, P)
    cls = bp.generated_class()
    cdo = unreal.get_default_object(cls)
    report["cdo"] = dump(cdo, ("JumpMaxHoldTime", "JumpMaxCount"))
    e, cm = call(cdo.get_editor_property, "CharacterMovement")
    if e == "ok" and cm is not None:
        report["cdo_CharacterMovement"] = dump(cm, P)
    # the actual spawned character in the level, if any
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    hits = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if "FirstPersonCharacter" not in a.get_class().get_name():
            continue
        c = a.get_component_by_class(unreal.CharacterMovementComponent)
        rec = {"label": lbl, "class": a.get_class().get_name()}
        if c is not None:
            rec["movement"] = dump(c, P)
        hits.append(rec)
    report["level_characters"] = hits
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[CHAR-MOVE saved]")
