import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
SOUND = "/Game/sound_resources/"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "setup_uicue_real.json")
report = {"missing": []}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:110] + ">"

def snd(name):
    a = unreal.EditorAssetLibrary.load_asset(SOUND + name)
    if a is None:
        report["missing"].append(name)
    return a

def make_cue(cue_id, trigger, trigger_id, text, voices, prox=600.0):
    c = unreal.UiCue()
    call(c.set_editor_property, "CueId", cue_id)
    call(c.set_editor_property, "Trigger", trigger)
    call(c.set_editor_property, "TriggerId", trigger_id)
    call(c.set_editor_property, "Text", text)
    loaded = []
    for v in voices:
        a = snd(v)
        if a is not None:
            loaded.append(a)
    call(c.set_editor_property, "Voices", loaded)
    call(c.set_editor_property, "bOnce", True)
    call(c.set_editor_property, "SegmentSecondsOverride", 0.0)
    call(c.set_editor_property, "ProximityDistance", prox)
    return c

G = unreal.UiCueTrigger.GAME_START
I = unreal.UiCueTrigger.INTERACT_FIRST_TOGGLE
P = unreal.UiCueTrigger.PROXIMITY_TO_ACTOR

CUES = [
    make_cue("ui_opening", G, "",
             "Hey, can you hear me? The rules of space here aren't ordinary. Try interacting with this mechanism.",
             ["opening", "opening2", "opening3"]),
    make_cue("ui_kongjian1", I, "kongjianchuansuoqi1",
             "This is another space of his mind. Try to do something.",
             ["kongjianzhuanhuanqi1", "kongjianzhuanhuanqi1_2"]),
    make_cue("ui_kongjian2", I, "kongjianchuansuoqi2",
             "It seems like a change in one space can affect another space.",
             ["kongjianzhuanhuanqi2"]),
    make_cue("ui_men1", I, "men1",
             "Behind these doors are stored his memories.",
             ["men1"]),
    make_cue("ui_men2", I, "men2",
             "The space of this memory is pretty chaotic, get ready.",
             ["men2"]),
    make_cue("ui_men3", I, "men3",
             "A hidden passage, this should be one of his scars.",
             ["men3"]),
    make_cue("ui_men21", I, "men21",
             "The location is right here, try looking higher up. The space here feels very light, give jumping a try.",
             ["men21", "men21_2"]),
    make_cue("ui_jiguanqiang1", P, "jiguanqiang1",
             "Sometimes you'll run into obstacles, it's his inner caution toward you.",
             ["LS12"], 600.0),
    make_cue("ui_jiguanqiang2", P, "jiguanqiang2",
             "Sometimes you'll run into obstacles, it's his inner caution toward you.",
             ["LS12"], 600.0),
]

try:
    da = unreal.EditorAssetLibrary.load_asset(DA)
    report["da"] = str(da)[:70]
    report["set_cues"] = str(call(da.set_editor_property, "Cues", CUES,
                                  notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS))[:60]
    report["save_da"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, da))[:20]

    # 清掉旧的测试触发体积
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = []
    for a in ss.get_all_level_actors():
        if "UiCueTriggerVolume" not in str(call(a.get_class().get_name)):
            continue
        lbl = str(call(a.get_actor_label))
        if lbl.startswith("UiCueDemo") or lbl.startswith("demo"):
            call(ss.destroy_actor, a)
            removed.append(lbl)
    report["removed_volumes"] = removed
    report["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###SETUP " + json.dumps(report, ensure_ascii=True)[:800])
print("[SETUP UICUE done]")