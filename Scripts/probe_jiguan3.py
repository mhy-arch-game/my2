import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jiguan3.json")
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

def comps(a):
    return safe(a.get_components_by_class, unreal.ActorComponent) or []

def cname(c):
    return s(safe(c.get_class().get_name), 70)

def block(name, fn):
    try:
        report[name] = fn()
    except Exception as exc:
        report[name] = {"fatal": repr(exc)[:200], "tb": traceback.format_exc()[-400:]}

def f(v):
    try:
        return round(float(v), 1)
    except Exception:
        return str(v)[:30]

def ls_block():
    exts = unreal.MovieSceneSequenceExtensions
    bext = unreal.MovieSceneBindingExtensions
    trext = getattr(unreal, "MovieSceneTrackExtensions", None)
    sext = getattr(unreal, "MovieSceneSectionExtensions", None)
    out = {"api": {"trext": trext is not None, "sext": sext is not None,
                   "trext_names": [n for n in dir(trext) if not n.startswith("_")][:45] if trext else [],
                   "sext_names": [n for n in dir(sext) if not n.startswith("_")][:45] if sext else []}}
    for nm in ("LS_jiguanqiang1", "LS_jiguanqiang2"):
        a = unreal.load_asset("/Game/" + nm)
        info = {"playback": s(safe(exts.get_playback_range, a), 90),
                "start_s": f(safe(exts.get_playback_start_seconds, a)),
                "end_s": f(safe(exts.get_playback_end_seconds, a)),
                "tick": s(safe(exts.get_tick_resolution, a), 60)}
        binds = []
        for b in safe(exts.get_bindings, a) or []:
            e = {"name": s(safe(bext.get_name, b), 60),
                 "display": s(safe(bext.get_display_name, b), 60),
                 "possessed": s(safe(bext.get_possessed_object_class, b), 60),
                 "valid": s(safe(bext.is_valid, b), 10)}
            trs = []
            for t in safe(bext.get_tracks, b) or []:
                te = {"track": cname(t)}
                secs = safe(trext.get_sections, t) if trext else None
                if not isinstance(secs, list):
                    secs = gp(t, "Sections")
                if not isinstance(secs, list):
                    secs = []
                    te["sections_err"] = s(safe(trext.get_sections, t) if trext else "no-ext", 80)
                te["section_count"] = len(secs)
                sl = []
                for sec in secs:
                    se = {"class": cname(sec), "range": s(gp(sec, "Range"), 70),
                          "start_s": f(safe(sext.get_start_frame, sec)) if sext else None,
                          "end_s": f(safe(sext.get_end_frame, sec)) if sext else None}
                    chs = safe(sext.get_channels, sec) if sext else []
                    if not isinstance(chs, list):
                        chs = []
                    chl = []
                    for ch in chs:
                        ce = {"ch": cname(ch)}
                        ks = safe(ch.get_keys)
                        if isinstance(ks, list):
                            ce["keys"] = len(ks)
                            samples = []
                            pick = list(range(min(2, len(ks)))) + list(range(max(0, len(ks) - 2), len(ks)))
                            for idx in pick:
                                k = ks[idx]
                                samples.append(s(safe(k.get_value), 40) + " @ " + s(safe(k.get_time), 40))
                            ce["samples"] = samples
                        else:
                            ce["keys"] = s(ks, 60)
                        chl.append(ce)
                    se["channels"] = chl
                    sl.append(se)
                te["sections"] = sl
                trs.append(te)
            e["tracks"] = trs
            binds.append(e)
        info["bindings"] = binds
        out[nm] = info
    return out

def walls_block():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = {}
    for a in ss.get_all_level_actors():
        lbl = s(safe(a.get_actor_label), 60)
        if not lbl.startswith("jiguan"):
            continue
        r = safe(a.get_actor_rotation)
        o = unreal.Vector()
        e = unreal.Vector()
        safe(a.get_actor_bounds, False, o, e)
        d = {"class": cname(a), "loc": [f(safe(a.get_actor_location).x), f(safe(a.get_actor_location).y), f(safe(a.get_actor_location).z)],
             "yaw": f(getattr(r, "yaw", None)) if r is not None else "?",
             "pitch": f(getattr(r, "pitch", None)) if r is not None else "?",
             "roll": f(getattr(r, "roll", None)) if r is not None else "?",
             "fwd": s(safe(a.get_actor_forward_vector), 60),
             "hidden": s(gp(a, "bHidden"), 8),
             "col": s(safe(a.get_actor_enable_collision), 8),
             "bounds_origin": [f(o.x), f(o.y), f(o.z)], "bounds_extent": [f(e.x), f(e.y), f(e.z)],
             "comps": []}
        for c in comps(a):
            cn = cname(c)
            ce = {"name": s(safe(c.get_name), 40), "class": cn}
            if "StaticMeshComponent" in cn:
                ce["mesh"] = s(gp(c, "StaticMesh"), 100)
                ce["vis"] = s(gp(c, "bVisible"), 8)
                ce["col_enabled"] = s(safe(c.get_collision_enabled), 30)
                ce["profile"] = s(safe(c.get_collision_profile_name), 40)
            if "InteractableComponent" in cn:
                for n in ("bEnabled", "bTrackOpenState", "bUseBuiltInToggle", "bToggleLights",
                          "LightComponentNames", "bStartOpen", "InteractionPrompt", "InteractionPromptOpen"):
                    ce[n] = s(gp(c, n), 50)
                ce["IsOpen"] = s(safe(c.is_open), 8)
            if "OperationReceiver" in cn:
                ce["channel"] = s(gp(c, "channel"), 40)
                ce["bindings"] = s(gp(c, "bindings"), 200)
            if "InteractionLinkComponent" in cn:
                ce["sync"] = s(gp(c, "bSyncOnBeginPlay"), 8)
            d["comps"].append(ce)
        out[lbl] = d
    return out

def links_block():
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    links = []
    for a in ss.get_all_level_actors():
        lbl = s(safe(a.get_actor_label), 60)
        for c in comps(a):
            if "InteractionLinkComponent" not in cname(c):
                continue
            en = []
            for e in (gp(c, "Entries") or []):
                en.append({"op": s(gp(e, "Operation"), 30), "ch": s(gp(e, "Channel"), 40),
                           "mirror": s(gp(e, "bMirrorSourceState"), 8),
                           "closed": s(gp(e, "ClosedOperation"), 30),
                           "targets": [s(safe(t.get_actor_label), 40) for t in (gp(e, "Targets") or [])]})
            links.append({"owner": lbl, "sync": s(gp(c, "bSyncOnBeginPlay"), 8), "entries": en})
    return links

def inter_block():
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = []
    for a in ss.get_all_level_actors():
        lbl = s(safe(a.get_actor_label), 60)
        for c in comps(a):
            if "InteractableComponent" not in cname(c):
                continue
            out.append({"owner": lbl, "enabled": s(gp(c, "bEnabled"), 8),
                        "toggle": s(gp(c, "bUseBuiltInToggle"), 8), "track": s(gp(c, "bTrackOpenState"), 8),
                        "startOpen": s(gp(c, "bStartOpen"), 8), "lights": s(gp(c, "bToggleLights"), 8),
                        "lightNames": s(gp(c, "LightComponentNames"), 50),
                        "prompt": s(gp(c, "InteractionPrompt"), 30),
                        "promptOpen": s(gp(c, "InteractionPromptOpen"), 30),
                        "isOpen": s(safe(c.is_open), 8)})
    return out

def lamps_block():
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = []
    for a in ss.get_all_level_actors():
        lbl = s(safe(a.get_actor_label), 60)
        if not lbl.startswith("jiguandeng"):
            continue
        d = {"label": lbl, "class": cname(a), "loc": s(safe(a.get_actor_location), 60),
             "hidden": s(gp(a, "bHidden"), 8), "comps": []}
        for c in comps(a):
            cn = cname(c)
            ce = {"name": s(safe(c.get_name), 40), "class": cn, "intensity": s(gp(c, "Intensity"), 20)}
            try:
                ce["visible"] = s(c.is_visible, 8)
            except Exception:
                pass
            if "InteractableComponent" in cn:
                for n in ("bEnabled", "bToggleLights", "LightComponentNames", "bStartOpen", "bTrackOpenState"):
                    ce[n] = s(gp(c, n), 50)
                ce["IsOpen"] = s(safe(c.is_open), 8)
            if "OperationReceiver" in cn:
                ce["channel"] = s(gp(c, "channel"), 40)
            if "InteractionLinkComponent" in cn:
                ce["entries"] = s(gp(c, "Entries"), 140)
            d["comps"].append(ce)
        out.append(d)
    return out

block("ls", ls_block)
block("walls", walls_block)
block("links", links_block)
block("interactables", inter_block)
block("lamps", lamps_block)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("[PROBE3 done]")