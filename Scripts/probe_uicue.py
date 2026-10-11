import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_uicue.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:80] + ">"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return None

def cname(c):
    return str(call(c.get_class().get_name))

try:
    # A) 音频资产（含时长）
    sounds = {}
    for p in unreal.EditorAssetLibrary.list_assets("/Game/sound_resources", recursive=True, include_folder=False):
        sp = str(p).split(".")[0]
        d = unreal.EditorAssetLibrary.find_asset_data(p)
        cls = str(call(lambda d=d: d.asset_class_path.asset_name))
        a = unreal.load_asset(p)
        dur = gp(a, "Duration")
        if dur is None:
            dur = call(a.get_duration)
        sounds[sp] = {"class": cls, "duration": str(dur)[:20],
                      "looping": str(gp(a, "bLooping"))}
    report["sounds"] = sounds

    # B) 当前 UiCueSet 里的测试 cue
    ds = unreal.load_asset("/Game/MHY_ARCH_GAME/UI/DA_UiCueSet")
    report["dataset"] = str(ds)[:80]
    cues = []
    if not isinstance(ds, str):
        for c in (gp(ds, "Cues") or []):
            cues.append({"id": str(gp(c, "CueId")), "trigger": str(gp(c, "Trigger")),
                         "triggerId": str(gp(c, "TriggerId")), "text": str(gp(c, "Text"))[:60],
                         "voice": str(gp(c, "Voice"))[:70], "once": str(gp(c, "bOnce"))})
    report["cues"] = cues

    # C) 关卡相关 actor
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    acts = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        low = lbl.lower()
        if ("men" in low) or ("kongjian" in low) or ("jiguan" in low) or ("uicue" in low) or low.startswith("ls_"):
            comps = []
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                cn = cname(c)
                e = {"c": cn}
                if "InteractableComponent" in cn:
                    for n in ("bEnabled", "bTrackOpenState", "bUseBuiltInToggle", "bToggleLights", "bStartOpen"):
                        e[n] = str(gp(c, n))
                if "UiCue" in cn:
                    e["triggerId"] = str(gp(c, "TriggerId"))
                    e["extent"] = str(gp(c, "BoxExtent"))[:50]
                comps.append(e)
            acts[lbl] = {"class": cname(a), "loc": str(call(a.get_actor_location))[:58], "comps": comps}
    report["actors"] = acts
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###UICUE " + json.dumps(report, ensure_ascii=True)[:5000])
print("[PROBE UICUE done]")