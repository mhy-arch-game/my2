import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_offset_audio.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def v3(v):
    try:
        return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]
    except Exception:
        return str(v)[:50]


try:
    # 1) portal offset on the 10 level instances (inherited from the BP template)
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        p = a.get_component_by_class(unreal.TimeEraPortalComponent)
        if p is None:
            continue
        rows.append({"label": lbl, "yaw": round(float(a.get_actor_rotation().yaw), 1),
                     "TeleportOffset": v3(call(p.get_editor_property, "TeleportOffset")[1])})
    rows.sort(key=lambda r: int("".join(ch for ch in r["label"] if ch.isdigit()) or 0))
    report["portal_offsets"] = rows

    # 2) character MovementAudio setting, read fresh from the asset
    bp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "MovementAudioComponent":
            continue
        report["char_audio"] = {
            "bAutoFootstepByDistance": str(call(obj.get_editor_property, "bAutoFootstepByDistance")[1]),
            "FootstepDistance": str(call(obj.get_editor_property, "FootstepDistance")[1]),
            "bAutoDetectJumpAndLand": str(call(obj.get_editor_property, "bAutoDetectJumpAndLand")[1]),
            "DefaultSet": str(call(obj.get_editor_property, "DefaultSet")[1])[:160],
        }
        break
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY OFFSET+AUDIO done]")
