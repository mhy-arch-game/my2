import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_character.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:70]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    # 角色蓝图的 CDO：Mesh / 胶囊
    cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
    cdo = unreal.get_default_object(cls) if cls else None
    if cdo:
        mesh = gp(cdo, "Mesh")
        report["cdo_mesh"] = str(mesh)[:70] if mesh is not None else "None"
        if mesh is not None:
            report["cdo_mesh_animclass"] = str(gp(mesh, "AnimClass"))[:60]
            report["cdo_mesh_collision"] = str(call(mesh.get_collision_profile_name))
            report["cdo_mesh_rel_loc"] = str(gp(mesh, "RelativeLocation"))[:60]
        cap = gp(cdo, "CapsuleComponent")
        if cap is not None:
            report["capsule"] = {"half_height": str(gp(cap, "CapsuleHalfHeight")),
                                 "radius": str(gp(cap, "CapsuleRadius")),
                                 "profile": str(call(cap.get_collision_profile_name))}
    # 关卡里摆的角色实例：组件全览
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        if "BP_FirstPersonCharacter" not in str(call(a.get_class().get_name)):
            continue
        comps = []
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            cn = str(call(c.get_class().get_name))
            e = {"class": cn}
            if "SkeletalMesh" in cn:
                e["mesh"] = str(gp(c, "SkeletalMesh"))[:60]
                e["anim"] = str(gp(c, "AnimClass"))[:50]
                e["visible"] = str(call(c.is_visible))
            if "Camera" in cn:
                e["rel_loc"] = str(gp(c, "RelativeLocation"))[:60]
            comps.append(e)
        report["level_instance_components"] = comps
        break
    # 哪些动画带 MovementAudio 通知
    withnotify = []
    checked = 0
    for root_dir in ("/Game/FirstPerson", "/Game/Characters"):
        for p in unreal.EditorAssetLibrary.list_assets(root_dir, recursive=True, include_folder=False):
            sp = str(p)
            if "AnimSequence" not in sp and "Anim" not in sp.split("/")[-1]:
                continue
            a = unreal.EditorAssetLibrary.find_asset_data(p)
            cn = str(call(lambda a=a: a.asset_class_path.asset_name))
            if cn != "AnimSequence":
                continue
            anim = unreal.EditorAssetLibrary.load_asset(p)
            checked += 1
            ns = gp(anim, "Notifies")
            txt = str(ns)
            if "MovementAudio" in txt:
                withnotify.append(sp)
    report["anim_sequences_checked"] = checked
    report["anims_with_movement_audio_notify"] = withnotify[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("CHAR PROBE done")