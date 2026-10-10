import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
BP = "/Game/bclass_source/jumping_area"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_gravity_preset.json")
report = {}
lib = unreal.EditorAssetLibrary
P = ("GravityScaleInside", "bScaleJumpVelocity", "TargetJumpHeight", "bAffectPawnsOnly", "BoxExtent")
def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)
def dump(o):
    out = {}
    for p in P:
        e, v = call(o.get_editor_property, p)
        if e == "ok":
            out[p] = str(v)[:60]
    return out
try:
    bp = lib.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    tmpl = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "GravityZoneComponent":
            continue
        tmpl.append(dump(obj))
    report["template"] = tmpl
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    lv = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.GravityZoneComponent):
            d = dump(c)
            d["label"] = lbl
            lv.append(d)
    report["level"] = lv
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY-GRAVITY saved]")
