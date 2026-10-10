import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "apply_offset_probe_audio.json")
BP = "/Game/bclass_source/modern_swift_actor"
report = {"teleport_offset_x": 5.0}


def v3(v):
    try:
        return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]
    except Exception:
        return str(v)[:50]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def setp(o, name, val, notify=False):
    if notify:
        e, r = call(o.set_editor_property, name, val, notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    else:
        e, r = call(o.set_editor_property, name, val)
    return [name, e, ("" if e == "ok" else str(r)[:70])]


try:
    # ---------------- 1) teleport arrival +5cm on X (BP template) ----------------
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "TimeEraPortalComponent":
            continue
        before = str(call(obj.get_editor_property, "TeleportOffset")[1])
        call(obj.modify)
        w = setp(obj, "TeleportOffset", unreal.Vector(5.0, 0.0, 0.0), notify=True)
        rows.append({"comp": obj.get_name(), "before": before, "write": w,
                     "after": str(call(obj.get_editor_property, "TeleportOffset")[1])})
    report["teleport_template"] = rows
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---------------- 2) character MovementAudio current settings ----------------
    cbp = unreal.EditorAssetLibrary.load_asset("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(cbp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "MovementAudioComponent":
            continue
        props = {}
        for p in ("bAutoDetectJumpAndLand", "RunSpeedThreshold", "bAutoFootstepByDistance",
                  "FootstepDistance", "SurfaceTraceDistance", "SurfaceTraceChannel",
                  "WalkSpeedThreshold", "SprintSpeedThreshold", "bPlayMusicPerState",
                  "MusicFadeTime", "bDispatchToInterfaceListeners", "VolumeMultiplier",
                  "PitchMin", "PitchMax"):
            e2, v = call(obj.get_editor_property, p)
            if e2 == "ok":
                props[p] = str(v)
        for p in ("DefaultSet", "StateMusic", "SurfaceSets"):
            e2, v = call(obj.get_editor_property, p)
            if e2 == "ok":
                props[p] = str(v)[:200]
        report["character_movement_audio"] = props
        break

    # ---------------- 3) existing sound assets in the project ----------------
    assets = []
    try:
        for p in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False):
            data = unreal.EditorAssetLibrary.find_asset_data(p)
            cls = ""
            try:
                cls = str(data.asset_class_path.asset_name)
            except Exception:
                cls = str(call(data.get_editor_property, "asset_class")[1])
            if cls in ("SoundWave", "SoundCue", "MetaSoundSource", "SoundClass", "SoundAttenuation",
                       "SoundConcurrency", "SoundMix", "ReverbEffect"):
                assets.append([p, cls])
    except Exception as exc:
        report["list_assets_err"] = repr(exc)[:120]
    report["sound_assets"] = assets
    report["sound_asset_count"] = len(assets)
    # the first-person template usually ships audio under /Game/FirstPerson
    try:
        fp = [p for p in unreal.EditorAssetLibrary.list_assets("/Game/FirstPerson", recursive=True, include_folder=False)]
        report["firstperson_asset_count"] = len(fp)
        report["firstperson_audio_like"] = [p for p in fp if "audio" in p.lower() or "sound" in p.lower() or "cue" in p.lower() or "step" in p.lower() or "jump" in p.lower() or "land" in p.lower()][:60]
    except Exception as exc:
        report["fp_err"] = repr(exc)[:120]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[OFFSET+AUDIO PROBE done]")
