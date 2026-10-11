import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jump4.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return "<no:" + n + ">"
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    zones = []
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if lbl.startswith("jumping_area"):
            zones.append({"label": lbl,
                          "editor_only": str(gp(a, "bIsEditorOnlyActor")),
                          "hidden_ed": str(call(a.is_hidden_ed)),
                          "owner_level": str(call(a.get_outer))[:60]})
        if not lbl.startswith("jumping_area") and "GravityZone" not in "".join(str(call(c.get_class().get_name)) for c in list(call(a.get_components_by_class, unreal.ActorComponent) or [])):
            continue
    report["zones"] = zones
    cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
    cdo = unreal.get_default_object(cls) if cls else None
    if cdo:
        caps = gp(cdo, "CapsuleComponent")
        if not isinstance(caps, str) and caps:
            report["capsule"] = {"overlap_events": str(gp(caps, "bGenerateOverlapEvents")),
                                 "profile": str(call(caps.get_collision_profile_name)),
                                 "col": str(call(caps.get_collision_enabled)),
                                 "obj_type": str(gp(caps, "CollisionObjectType")) if gp(caps, "CollisionObjectType") is not None else "?",
                                 "extent": str(gp(caps, "CapsuleHalfHeight"))[:20]}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###JUMP4 " + json.dumps(report, ensure_ascii=True)[:1800])