import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jump.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return "<no:" + n + ">"

def cname(c):
    return str(call(c.get_class().get_name))

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    hits = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        cls = cname(a)
        comps = list(call(a.get_components_by_class, unreal.ActorComponent) or [])
        cnames = [cname(c) for c in comps]
        interesting = (("jump" in lbl.lower()) or ("tiao" in lbl.lower()) or ("gravity" in lbl.lower())
                       or ("zhongli" in lbl.lower()) or any("GravityZone" in n for n in cnames))
        if not interesting:
            continue
        d = {"class": cls, "loc": str(call(a.get_actor_location))[:50], "comps": []}
        for c in comps:
            cn = cname(c)
            e = {"c": cn}
            if "GravityZone" in cn or "BoxComponent" in cn:
                for n in ("GravityScaleInside", "bScaleJumpVelocity", "TargetJumpHeight",
                          "bAffectPawnsOnly", "BoxExtent", "bHiddenInGame"):
                    v = gp(c, n)
                    if not isinstance(v, str) or not v.startswith("<no:"):
                        e[n] = str(v)[:60]
                e["overlap"] = str(call(c.get_editor_property, "bGenerateOverlapEvents"))
                e["profile"] = str(call(c.get_collision_profile_name))
                e["col"] = str(call(c.get_collision_enabled))
            d["comps"].append(e)
        hits[lbl] = d
    report["zones"] = hits

    # 角色蓝图的跳跃参数
    bp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    cdo = None
    if not isinstance(bp, str) and bp:
        cls = call(bp.generated_class)
        if not isinstance(cls, str):
            cdo = call(cls.get_default_object)
    if cdo is not None and not isinstance(cdo, str):
        report["char"] = {"JumpMaxCount": str(gp(cdo, "JumpMaxCount")),
                          "JumpMaxHoldTime": str(gp(cdo, "JumpMaxHoldTime")),
                          "class": cname(cdo)}
        mov = call(cdo.get_editor_property, "CharacterMovement")
        if not isinstance(mov, str) and mov:
            report["char_move"] = {"JumpZVelocity": str(gp(mov, "JumpZVelocity")),
                                   "GravityScale": str(gp(mov, "GravityScale")),
                                   "AirControl": str(gp(mov, "AirControl")),
                                   "NavAgentProps": str(gp(mov, "NavAgentProps"))[:90],
                                   "bCanWalkOffLedges": str(gp(mov, "bCanWalkOffLedges"))}
    else:
        report["char"] = "cdo 取不到: " + str(cdo)[:80]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###JUMP " + json.dumps(report, ensure_ascii=True)[:3500])
print("[PROBE JUMP done]")