import json, os, re, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "audit_assets.json")
report = {}
PAT = re.compile(r"/Game/[A-Za-z0-9_/.\-()\u4e00-\u9fff]+")
found = set()


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


def harvest(v):
    try:
        for m in PAT.findall(str(v)):
            found.add(m)
    except Exception:
        pass


try:
    report["ia_timeshift"] = unreal.EditorAssetLibrary.does_asset_exist("/Game/Input/Actions/IA_TimeShift")

    # --- level ---
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    classes = {}
    weird = []
    null_prim = 0
    total_prim = 0
    for a in actor_ss.get_all_level_actors():
        cn = a.get_class().get_name()
        classes[cn] = classes.get(cn, 0) + 1
        if cn.startswith(("REINST_", "TRASH_", "SKEL_", "TRASHCLASS_")):
            weird.append([a.get_actor_label(), cn])
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            total_prim += 1
            for prop in ("StaticMesh", "SkeletalMesh"):
                e, v = call(c.get_editor_property, prop)
                if e == "ok":
                    if v is None:
                        null_prim += 1
                    harvest(v)
            e2, mats = call(c.get_editor_property, "OverrideMaterials")
            if e2 == "ok" and mats:
                harvest(mats)
    report["level_classes"] = dict(sorted(classes.items(), key=lambda kv: -kv[1]))
    report["level_class_count"] = len(classes)
    report["level_weird_classes"] = weird
    report["level_primitive_count"] = total_prim
    report["level_null_mesh_slots"] = null_prim

    # --- key blueprints ---
    bps = ["/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter",
           "/Game/bclass_source/modern_swift_actor",
           "/Game/bclass_source/active_door",
           "/Game/bclass_source/active_light",
           "/Game/bclass_source/jumping_area",
           "/Game/bclass_source/reveal_wall",
           "/Game/MHY_ARCH_GAME/UI/WBP_InteractionPrompt"]
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    bp_info = {}
    for p in bps:
        bp = unreal.EditorAssetLibrary.load_asset(p)
        if bp is None:
            bp_info[p] = "MISSING"
            continue
        entry = {"class": str(call(bp.generated_class)[1])[-40:], "components": {}}
        harvest(bp.get_path_name())
        for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
            e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
            if e1 != "ok":
                continue
            e3, obj = call(flib.get_object, d)
            e2, var = call(flib.get_variable_name, d)
            if obj is None:
                continue
            name = str(var) if e2 == "ok" else "?"
            props = {}
            for prop in ("StaticMesh", "SkeletalMesh", "DefaultSet", "StateMusic", "SurfaceSets", "WidgetClass"):
                e, v = call(obj.get_editor_property, prop)
                if e == "ok" and v is not None:
                    props[prop] = str(v)[:90]
                    harvest(v)
            if props:
                entry["components"][name] = props
        bp_info[p] = entry
    report["blueprints"] = bp_info

    report["referenced_game_paths"] = sorted(found)
    report["referenced_count"] = len(found)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[AUDIT ASSETS done]")
