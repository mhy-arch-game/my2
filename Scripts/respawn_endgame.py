"""Re-create the endofthegame instance at the right spot (moving an existing instance
does not survive saving on this level - spawn the actor at the final transform instead)."""
import json, math, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_PATH = "/Game/MHY_ARCH_GAME/GameFlow/endofthegame"
LABEL = "endofthegame"
FWD_CM = 180.0
Z_OFFSET_CM = 128.0
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "respawn_endgame.json")
rep = {}

def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:100] + ">"

def v3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old, char = [], None
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        if "endofthegame" in cn.lower():
            old.append(a)
        if "FirstPersonCharacter" in cn and char is None:
            char = a
    rep["old_count"] = len(old)
    rep["old_locs"] = [v3(call(a.get_actor_location)) for a in old]

    cloc = call(char.get_actor_location)
    yaw = float(call(char.get_actor_rotation).yaw)
    rad = math.radians(yaw)
    spot = unreal.Vector(float(cloc.x) + math.cos(rad) * FWD_CM,
                         float(cloc.y) + math.sin(rad) * FWD_CM,
                         float(cloc.z) + Z_OFFSET_CM)
    rot = unreal.Rotator(0.0, yaw, 0.0)
    rep["char_loc"] = v3(cloc)
    rep["spot"] = v3(spot)

    for a in old:
        call(ss.destroy_actor, a)
    rep["destroyed"] = len(old)

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    gen = call(bp.generated_class)
    newa = call(ss.spawn_actor_from_class, gen, spot, rot)
    if isinstance(newa, str):
        rep["spawn_err"] = newa
    else:
        call(newa.set_actor_label, LABEL)
        root = call(newa.get_editor_property, "root_component")
        rep["root_mobility"] = str(call(root.get_editor_property, "mobility")) if root else None
        rep["new_loc"] = v3(call(newa.get_actor_location))
        rep["new_label"] = str(call(newa.get_actor_label))
    rep["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:30]
    rep["save_dirty"] = str(call(unreal.EditorLoadingAndSavingUtils.save_dirty_packages, True, True))[:30]
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("RESPAWN ENDGAME done")