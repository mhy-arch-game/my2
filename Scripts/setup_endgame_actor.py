"""Create the endofthegame Blueprint (child of AEndOfTheGameActor) and place one instance
in firstvision, in front of the PlayerStart. Idempotent. Writes Scripts/setup_endgame_actor.json."""
import json, math, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_DIR = "/Game/MHY_ARCH_GAME/GameFlow"
BP_NAME = "endofthegame"
BP_PATH = BP_DIR + "/" + BP_NAME
PARENT_CLASS = "/Script/MHY_ARCH_GAME.EndOfTheGameActor"
LABEL = "endofthegame"
FWD_CM = 180.0
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "setup_endgame_actor.json")
report = {}

def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:110] + ">"

def cname(c):
    return str(call(c.get_class().get_name))

def v3(v):
    try: return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception: return str(v)[:40]

def resolve_class(path):
    for fn in ("load_class", "load_object"):
        f = getattr(unreal, fn, None)
        if f is None: continue
        try:
            c = f(None, path)
            if c is not None: return c
        except Exception: pass
    return None

def bounds_of(a):
    b = call(a.get_actor_bounds, False)
    try:
        origin, extent = b[0], b[1]
        return (float(origin.x), float(origin.y), float(origin.z),
                float(extent.x), float(extent.y), float(extent.z))
    except Exception:
        return None

try:
    # ---- 1) 蓝图类 ----
    if unreal.EditorAssetLibrary.does_asset_exist(BP_PATH):
        report["blueprint"] = "exists"
    else:
        parent = resolve_class(PARENT_CLASS)
        if parent is None:
            report["blueprint"] = "parent class unresolved: " + PARENT_CLASS
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                asset_name=BP_NAME, package_path=BP_DIR, asset_class=unreal.Blueprint, factory=factory)
            if asset is None:
                report["blueprint"] = "create_asset returned None"
            else:
                try: unreal.BlueprintEditorLibrary.compile_blueprint(asset)
                except Exception as exc: report["compile_err"] = repr(exc)[:90]
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
                report["blueprint"] = "created"

    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    report["bp_parent"] = str(call(bp.get_editor_property, "parent_class"))[:80] if bp else None
    gen = call(bp.generated_class) if bp else None
    report["bp_gen_class"] = str(gen)[:80]

    # ---- 2) 关卡 ----
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(ss.get_all_level_actors())
    report["actor_count"] = len(actors)

    ps = None
    existing = None
    for a in actors:
        cls = cname(a)
        if "PlayerStart" in cls and ps is None: ps = a
        if str(call(a.get_actor_label)) == LABEL: existing = a
    if ps is None:
        report["fatal"] = "找不到 PlayerStart"
    else:
        loc = call(ps.get_actor_location)
        rot = call(ps.get_actor_rotation)
        yaw = float(rot.yaw)
        report["player_start"] = {"loc": v3(loc), "yaw": round(yaw, 1), "label": str(call(ps.get_actor_label))}

        rad = math.radians(yaw)
        spot = unreal.Vector(float(loc.x) + math.cos(rad) * FWD_CM,
                             float(loc.y) + math.sin(rad) * FWD_CM,
                             float(loc.z))
        report["spot"] = v3(spot)

        # 仅作参考：这个点落在哪些 actor 的包围盒里（判断有没有插进墙/地板）
        inside = []
        for a in actors:
            if a is ps: continue
            b = bounds_of(a)
            if b is None: continue
            if (abs(spot.x - b[0]) <= b[3] and abs(spot.y - b[1]) <= b[4] and abs(spot.z - b[2]) <= b[5]):
                inside.append(str(call(a.get_actor_label)) + " [" + cname(a) + "]")
        report["spot_inside_bounds_of"] = inside[:12]
        report["spot_inside_count"] = len(inside)

        if existing is not None:
            r = call(existing.set_actor_location, spot, False, False)
            target = existing
            report["mode"] = "复用已有实例"
        else:
            target = call(ss.spawn_actor_from_class, gen, spot, rot)
            report["mode"] = "新建实例"
        if isinstance(target, str):
            report["spawn_err"] = target
        else:
            call(target.set_actor_label, LABEL)
            report["placed"] = {"label": str(call(target.get_actor_label)),
                                "class": cname(target),
                                "loc": v3(call(target.get_actor_location)),
                                "rot": v3(call(target.get_actor_rotation))}
            comps = []
            for c in list(call(target.get_components_by_class, unreal.ActorComponent) or []):
                info = {"class": cname(c)}
                if "InteractableComponent" in cname(c):
                    info["prompt"] = str(call(c.get_editor_property, "InteractionPrompt"))
                    info["prompt_open"] = str(call(c.get_editor_property, "InteractionPromptOpen"))
                    info["enabled"] = str(call(c.get_editor_property, "bEnabled"))
                    info["builtin_toggle"] = str(call(c.get_editor_property, "bUseBuiltInToggle"))
                    info["track_state"] = str(call(c.get_editor_property, "bTrackOpenState"))
                if "StaticMeshComponent" in cname(c):
                    info["mesh"] = str(call(c.get_editor_property, "StaticMesh"))[:70]
                    info["collision"] = str(call(c.get_collision_enabled))
                    info["profile"] = str(call(c.get_collision_profile_name))
                    info["scale"] = v3(call(c.get_editor_property, "RelativeScale3D")) if hasattr(c, "get_editor_property") else None
                comps.append(info)
            report["components"] = comps
        report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:40]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-700:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=True)
print("SETUP ENDGAME done")