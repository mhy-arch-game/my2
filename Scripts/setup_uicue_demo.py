import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "setup_uicue_demo.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:150], None)


def make_cue(cue_id, trigger, trigger_id, text):
    c = unreal.UiCue()
    call(c.set_editor_property, "CueId", cue_id)
    call(c.set_editor_property, "Trigger", trigger)
    call(c.set_editor_property, "TriggerId", trigger_id)
    call(c.set_editor_property, "Text", text)
    call(c.set_editor_property, "bOnce", True)
    call(c.set_editor_property, "SegmentSecondsOverride", 0.0)
    return c


try:
    # ---------- 1) DataAsset 里放两条演示 cue ----------
    da = unreal.EditorAssetLibrary.load_asset(DA)
    report["da"] = da.get_name() if da else "MISSING"
    if da is None:
        raise RuntimeError("DA_UiCueSet not found")

    cue_a = make_cue(
        "demo_spawn", unreal.UiCueTrigger.BOX_VOLUME, "demo_spawn",
        "UI Cue 演示：你刚出生就进入了示例触发体积。这条字幕来自 DA_UiCueSet 里的 demo_spawn。"
        "多句会被自动断句，并按顺序播放。想改成你自己的内容，直接改这条 cue 就行。")
    cue_b = make_cue(
        "demo_men1", unreal.UiCueTrigger.INTERACT_FIRST_TOGGLE, "men1",
        "UI Cue 演示：这是 men1 第一次被切换状态（开或关）。"
        "再按一次 E 不会重复触发，因为本触发只认第一次。")

    call(da.modify)
    report["set_cues"] = call(da.set_editor_property, "Cues", [cue_a, cue_b],
                              notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
    report["save_da"] = call(unreal.EditorAssetLibrary.save_loaded_asset, da)[0]

    # ---------- 2) 关卡：出生点放一个触发体积 ----------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    start = None
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_class().get_name() == "PlayerStart":
                start = a
                break
        except Exception:
            pass
    loc = start.get_actor_location() if start else unreal.Vector(0.0, 0.0, 3182.0)
    report["player_start"] = [round(float(loc.x), 1), round(float(loc.y), 1), round(float(loc.z), 1)]

    existing = None
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_actor_label() == "UiCueDemo_Volume":
                existing = a
                break
        except Exception:
            pass

    if existing is None:
        vol = actor_ss.spawn_actor_from_class(unreal.UiCueTriggerVolume, loc, unreal.Rotator(0, 0, 0))
        report["spawned"] = bool(vol)
        if vol:
            call(vol.set_actor_label, "UiCueDemo_Volume")
            call(vol.set_editor_property, "TriggerId", "demo_spawn")
            box = vol.get_component_by_class(unreal.BoxComponent)
            if box:
                call(box.set_box_extent, unreal.Vector(600.0, 600.0, 600.0), True)
            report["volume_loc"] = [round(float(vol.get_actor_location().x), 1),
                                    round(float(vol.get_actor_location().y), 1),
                                    round(float(vol.get_actor_location().z), 1)]
    else:
        report["spawned"] = "already exists"

    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[SETUP UICUE DEMO done]")
