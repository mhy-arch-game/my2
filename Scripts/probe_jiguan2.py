import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jiguan2.json")
report = {}

def safe(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:70] + ">"

def s(v, n=130):
    try:
        return str(v)[:n]
    except Exception:
        return "<str-err>"

def gp(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None

def comps(actor):
    return safe(actor.get_components_by_class, unreal.ActorComponent) or []

def cname(c):
    return s(safe(c.get_class().get_name), 70)

try:
    # ---------- A) 两个 LS 到底绑了什么 ----------
    exts = getattr(unreal, "MovieSceneSequenceExtensions", None)
    bext = getattr(unreal, "MovieSceneBindingExtensions", None)
    text = getattr(unreal, "MovieSceneTrackExtensions", None)
    report["api"] = {"mseq_ext": exts is not None, "binding_ext": bext is not None,
                     "track_ext": text is not None,
                     "mseq_names": [n for n in dir(exts) if not n.startswith("_")][:60] if exts else [],
                     "binding_names": [n for n in dir(bext) if not n.startswith("_")][:60] if bext else []}
    lsrep = {}
    for nm in ("LS_jiguanqiang1", "LS_jiguanqiang2"):
        a = unreal.load_asset("/Game/" + nm)
        info = {"playback": s(safe(lambda: exts.get_playback_range(a)), 90) if exts else "?"}
        binds = []
        if a and exts:
            for b in safe(exts.get_bindings, a) or []:
                e = {"raw": s(b, 200)}
                if bext:
                    e["name"] = s(safe(lambda b=b: bext.get_name(b)), 60)
                    e["id"] = s(safe(lambda b=b: bext.get_id(b)), 80)
                    e["possessed_class"] = s(safe(lambda b=b: bext.get_possessed_object_class(b)), 60)
                    tracks = []
                    for t in safe(lambda b=b: bext.get_tracks(b), ) or []:
                        entry = {"track": cname(t)}
                        try:
                            secs = gp(t, "Sections") or []
                            entry["sections"] = len(secs)
                            rngs = []
                            for sec in secs:
                                rngs.append(s(gp(sec, "Range"), 70))
                            entry["ranges"] = rngs
                        except Exception as exc:
                            entry["sections"] = "<ERR " + repr(exc)[:50] + ">"
                        tracks.append(entry)
                    e["tracks"] = tracks
                binds.append(e)
        info["bindings"] = binds
        lsrep[nm] = info
    report["ls"] = lsrep

    # ---------- B~E) 关卡 ----------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    links, recvs, inter, lamps = [], [], [], []
    for a in actors:
        lbl = s(safe(a.get_actor_label), 70)
        for c in comps(a):
            cn = cname(c)
            if "InteractionLinkComponent" in cn:
                en = []
                for e in (gp(c, "Entries") or []):
                    en.append({"op": s(gp(e, "Operation"), 30), "ch": s(gp(e, "Channel"), 40),
                               "mirror": s(gp(e, "bMirrorSourceState"), 10),
                               "closed": s(gp(e, "ClosedOperation"), 30),
                               "targets": [s(safe(t.get_actor_label), 40) for t in (gp(e, "Targets") or [])]})
                links.append({"owner": lbl, "sync": s(gp(c, "bSyncOnBeginPlay"), 10), "entries": en})
            elif "OperationReceiver" in cn:
                recvs.append({"owner": lbl, "channel": s(gp(c, "channel"), 40),
                              "bindings": s(gp(c, "bindings"), 160)})
            elif "InteractableComponent" in cn:
                inter.append({"owner": lbl, "enabled": s(gp(c, "bEnabled"), 10),
                              "useToggle": s(gp(c, "bUseBuiltInToggle"), 10),
                              "trackOpen": s(gp(c, "bTrackOpenState"), 10),
                              "startOpen": s(gp(c, "bStartOpen"), 10),
                              "lights": s(gp(c, "bToggleLights"), 10),
                              "lightNames": s(gp(c, "LightComponentNames"), 60),
                              "prompt": s(gp(c, "InteractionPrompt"), 40),
                              "promptOpen": s(gp(c, "InteractionPromptOpen"), 40)})
        if lbl.lower().startswith("jiguandeng"):
            d = {"label": lbl, "class": s(safe(a.get_class().get_name), 40),
                 "loc": s(safe(a.get_actor_location), 60), "hidden": s(gp(a, "bHidden"), 10),
                 "bEnabled": s(safe(a.get_actor_enable_collision), 10), "comps": []}
            for c in comps(a):
                e = {"name": s(safe(c.get_name), 40), "class": cname(c)}
                e["intensity"] = s(gp(c, "Intensity"), 20)
                e["visible"] = s(safe(c.is_visible), 10)
                if "InteractableComponent" in cname(c):
                    e["bToggleLights"] = s(gp(c, "bToggleLights"), 10)
                    e["lightNames"] = s(gp(c, "LightComponentNames"), 60)
                    e["bEnabled"] = s(gp(c, "bEnabled"), 10)
                    e["IsOpen"] = s(safe(c.is_open), 10)
                d["comps"].append(e)
            lamps.append(d)
    report["links"] = links
    report["receivers"] = recvs
    report["interactables"] = inter
    report["lamps"] = lamps
    report["actor_count"] = len(actors)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("[PROBE2 done]")