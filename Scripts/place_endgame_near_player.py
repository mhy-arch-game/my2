"""Move the endofthegame instance next to the character the player actually possesses,
and persist it properly (the level uses one-file-per-actor, so save_current_level alone is not enough)."""
import json, math, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/MHY_ARCH_GAME/GameFlow/endofthegame"
LABEL = "endofthegame"
FWD_CM = 180.0
Z_OFFSET_CM = 128.0   # 角色被游戏抬高约 64cm（贴地），再抬到视线高度
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "place_endgame.json")
rep = {}

def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:100] + ">"

def v3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]

def yaw_of(rot):
    try: return round(float(rot.yaw), 1)
    except Exception: return 0.0

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    chars, target_actor = [], None
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        if "FirstPersonCharacter" in cn:
            chars.append({"class": cn, "label": str(call(a.get_actor_label)), "name": str(call(a.get_name)),
                          "loc": v3(call(a.get_actor_location)), "yaw": yaw_of(call(a.get_actor_rotation)),
                          "auto_possess": str(call(a.get_editor_property, "auto_possess_player"))})
        if str(call(a.get_actor_label)) == LABEL:
            target_actor = a
    rep["characters"] = chars

    # 选一个"会被玩家占据"的角色：AutoPossessPlayer != Disabled 优先
    pick = None
    for c in chars:
        if "DISABLED" not in c["auto_possess"].upper():
            pick = c
            break
    if pick is None and chars:
        pick = chars[0]
    rep["picked"] = pick

    if pick is None:
        rep["fatal"] = "关卡里没有 FirstPersonCharacter"
    else:
        yaw = float(pick["yaw"])
        rad = math.radians(yaw)
        spot = unreal.Vector(pick["loc"][0] + math.cos(rad) * FWD_CM,
                             pick["loc"][1] + math.sin(rad) * FWD_CM,
                             pick["loc"][2] + Z_OFFSET_CM)
        rep["spot"] = v3(spot)

        if target_actor is None:
            bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
            gen = call(bp.generated_class) if bp else None
            target_actor = call(ss.spawn_actor_from_class, gen, spot, call(a.get_actor_rotation))
            rep["mode"] = "新建"
            if not isinstance(target_actor, str):
                call(target_actor.set_actor_label, LABEL)
        else:
            rep["mode"] = "移动已有"
            call(target_actor.set_actor_location, spot, False, True)
        rep["after_move"] = v3(call(target_actor.get_actor_location)) if not isinstance(target_actor, str) else "ERR"

        # 关卡是 one-file-per-actor：必须把脏包（外部 actor 文件）也存了
        rep["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:30]
        rep["save_dirty"] = str(call(unreal.EditorLoadingAndSavingUtils.save_dirty_packages, True, True))[:30]
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("PLACE ENDGAME done")