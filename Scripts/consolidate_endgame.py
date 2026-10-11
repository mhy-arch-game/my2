"""把项目里已有的 bclass_source/end_of_the_game 复用为结束游戏物件：
   1) 报告它的引用者；2) 无引用者则重新指定父类为 AEndOfTheGameActor；
   3) 关卡里的实例换成这个类；4) 删掉先前临时建的 GameFlow/endofthegame 蓝图。"""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
TARGET_BP = "/Game/bclass_source/end_of_the_game"
NEW_PARENT = "/Script/MHY_ARCH_GAME.EndOfTheGameActor"
OLD_BP = "/Game/MHY_ARCH_GAME/GameFlow/endofthegame"
LABEL = "endofthegame"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "consolidate_endgame.json")
rep = {}

def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:110] + ">"

def v3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]

try:
    # ---- 1) 引用者 ----
    refs = call(unreal.EditorAssetLibrary.find_package_referencers_for_asset, TARGET_BP, False)
    rep["referencers"] = [str(r) for r in refs] if not isinstance(refs, str) else refs

    # ---- 2) 重指定父类 ----
    bp = unreal.EditorAssetLibrary.load_asset(TARGET_BP)
    parent = None
    for fn in ("load_class", "load_object"):
        f = getattr(unreal, fn, None)
        if f is None: continue
        try:
            parent = f(None, NEW_PARENT)
            if parent is not None: break
        except Exception: pass
    rep["parent_class"] = str(parent)[:90]
    if parent is None:
        rep["reparent"] = "父类解析失败"
    elif hasattr(unreal.BlueprintEditorLibrary, "reparent_blueprint"):
        rep["reparent"] = str(call(unreal.BlueprintEditorLibrary.reparent_blueprint, bp, parent))[:80]
        call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)
        call(unreal.EditorAssetLibrary.save_loaded_asset, bp)
    else:
        rep["reparent"] = "没有 reparent_blueprint API"

    gen = call(bp.generated_class)
    rep["generated"] = str(gen)[:100]
    _ad = call(unreal.EditorAssetLibrary.find_asset_data, TARGET_BP)
    if not isinstance(_ad, str):
        rep["gen_parent_tag"] = str(call(_ad.get_tag_value, "ParentClass"))[:110]

    # ---- 3) 关卡实例换成新类 ----
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dropped = []
    spot = None
    rot = None
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name)).lower()
        if "endofthegame" in cn or "end_of_the_game" in cn:
            dropped.append({"class": str(call(a.get_class().get_name)), "loc": v3(call(a.get_actor_location))})
            if spot is None:
                loc = call(a.get_actor_location)
                spot = unreal.Vector(loc.x, loc.y, loc.z)
                rot = call(a.get_actor_rotation)
            call(ss.destroy_actor, a)
    rep["dropped"] = dropped
    if spot is None:
        spot = unreal.Vector(596.7, -263.0, 238.6)
        rot = unreal.Rotator(0.0, 0.0, 0.0)
    newa = call(ss.spawn_actor_from_class, gen, spot, rot)
    if isinstance(newa, str):
        rep["spawn_err"] = newa
    else:
        call(newa.set_actor_label, LABEL)
        rep["placed"] = {"class": str(call(newa.get_class().get_name)), "label": str(call(newa.get_actor_label)),
                         "loc": v3(call(newa.get_actor_location))}
        comps = []
        for c in (call(newa.get_components_by_class, unreal.ActorComponent) or []):
            info = {"class": str(call(c.get_class().get_name))}
            if "InteractableComponent" in info["class"]:
                info["prompt"] = str(call(c.get_editor_property, "InteractionPrompt"))
                info["enabled"] = str(call(c.get_editor_property, "bEnabled"))
            comps.append(info)
        rep["components"] = comps
    rep["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:30]
    rep["save_dirty"] = str(call(unreal.EditorLoadingAndSavingUtils.save_dirty_packages, True, True))[:30]

    # ---- 4) 删掉临时蓝图 ----
    if unreal.EditorAssetLibrary.does_asset_exist(OLD_BP):
        rep["delete_old_bp"] = str(call(unreal.EditorAssetLibrary.delete_asset, OLD_BP))[:40]
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-600:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("CONSOLIDATE done")