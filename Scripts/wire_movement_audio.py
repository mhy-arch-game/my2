import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "wire_movement_audio.json")
RUN = "/Game/sound_resources/freesound_community-running-6358.freesound_community-running-6358"
JUMP = "/Game/sound_resources/freesound_community-jumping-on-wooden-floor-41234.freesound_community-jumping-on-wooden-floor-41234"
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
report = {"run": RUN, "jump": JUMP, "candidate_errors": []}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:150], None)


def soft_candidates(path):
    out = []
    try:
        out.append(("SoftObjectPath(path)", unreal.SoftObjectPath(path)))
    except Exception as exc:
        report["candidate_errors"].append(["SoftObjectPath(path)", repr(exc)[:110]])
    for ctor in ("asset_path_name", "AssetPathName", "Path"):
        try:
            sop = unreal.SoftObjectPath()
            sop.set_editor_property(ctor, path)
            out.append(("SoftObjectPath." + ctor, sop))
        except Exception as exc:
            report["candidate_errors"].append(["SoftObjectPath." + ctor, repr(exc)[:110]])
    try:
        obj = unreal.EditorAssetLibrary.load_asset(path)
        if obj:
            out.append(("UObject", obj))
    except Exception as exc:
        report["candidate_errors"].append(["UObject", repr(exc)[:110]])
    out.append(("str", path))
    return out


def try_set(struct_obj, prop, path):
    last = "no candidate"
    for name, val in soft_candidates(path):
        try:
            struct_obj.set_editor_property(prop, val)
            try:
                got = struct_obj.get_editor_property(prop)
                if got is None:
                    last = name + " -> assigned but read back None"
                    continue
            except Exception:
                pass
            return name
        except Exception as exc:
            last = name + ": " + repr(exc)[:100]
    return last


try:
    bp = unreal.EditorAssetLibrary.load_asset(CHAR)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "MovementAudioComponent":
            continue
        call(obj.modify)

        s = unreal.MovementAudioSet()
        report["jump_how"] = try_set(s, "Jump", JUMP)
        report["land_how"] = try_set(s, "Land", JUMP)
        # last resort: whole-struct text import
        if "None" in str(call(s.get_editor_property, "Jump")[1]) or report["jump_how"] != "ok":
            try:
                s.import_text('(Jump="%s",Land="%s")' % (JUMP, JUMP))
                report["import_text_used"] = True
            except Exception as exc:
                report["import_text_err"] = repr(exc)[:120]
        call(obj.set_editor_property, "DefaultSet", s,
             notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)

        entries = []
        for state in (unreal.MovementAudioState.RUN, unreal.MovementAudioState.SPRINT):
            en = unreal.MovementStateMusic()
            call(en.set_editor_property, "State", state)
            call(en.set_editor_property, "VolumeMultiplier", 1.0)
            how = try_set(en, "Music", RUN)
            if "None" in str(call(en.get_editor_property, "Music")[1]):
                try:
                    en.import_text('(Music="%s")' % RUN)
                    how = "import_text"
                except Exception as exc:
                    how = repr(exc)[:90]
            report.setdefault("music_how", []).append({"state": str(state), "how": how,
                                                      "readback": str(call(en.get_editor_property, "Music")[1])[:90]})
            entries.append(en)
        call(obj.set_editor_property, "StateMusic", entries,
             notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        report["after_default"] = str(call(obj.get_editor_property, "DefaultSet")[1])[:300]
        report["after_statemusic"] = str(call(obj.get_editor_property, "StateMusic")[1])[:400]
        break
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_char"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[WIRE MOVEMENT AUDIO v2 done]")
