import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jiguan.json")
report = {}

def safe(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:70] + ">"

def s(v, n=110):
    try:
        return str(v)[:n]
    except Exception:
        return "<str-err>"

def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return s(v, 40)

def gp(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None

def comps(actor):
    return safe(actor.get_components_by_class, unreal.ActorComponent) or []

def dump_interactable(c):
    d = {}
    for n in ("bEnabled", "InteractionPrompt", "InteractionPromptOpen", "bTrackOpenState",
              "bUseBuiltInToggle", "ToggleComponentNames", "bStartOpen", "bToggleLights",
              "LightComponentNames", "bDisableCollisionWhenOpen", "bUseInteractionProxy"):
        v = gp(c, n)
        if v is not None:
            d[n] = s(v)
    if hasattr(c, "is_open"):
        d["IsOpen()"] = s(safe(c.is_open), 40)
    return d

def dump_link(c):
    d = {"bSyncOnBeginPlay": s(gp(c, "bSyncOnBeginPlay"), 20), "Entries": []}
    for e in (gp(c, "Entries") or []):
        targets = [s(safe(t.get_actor_label), 40) for t in (gp(e, "Targets") or [])]
        d["Entries"].append({"Op": s(gp(e, "Operation"), 30), "Channel": s(gp(e, "Channel"), 30),
                             "Mirror": s(gp(e, "bMirrorSourceState"), 10),
                             "ClosedOp": s(gp(e, "ClosedOperation"), 30), "Targets": targets,
                             "Loc": v3(gp(e, "Location")) if gp(e, "Location") else None,
                             "Value": s(gp(e, "Value"), 20)})
    return d

def dump_generic(c):
    out = {}
    for n in sorted(dir(c)):
        if n.startswith("_"):
            continue
        try:
            v = c.get_editor_property(n)
        except Exception:
            continue
        if callable(v):
            continue
        out[n] = s(v, 90)
    return out

def dump_actor(a):
    info = {"label": s(safe(a.get_actor_label), 60), "class": s(safe(a.get_class().get_name), 60),
            "loc": v3(safe(a.get_actor_location)), "rot": s(safe(a.get_actor_rotation), 70),
            "hidden": s(gp(a, "bHidden"), 10), "comps": []}
    for c in comps(a):
        cn = s(safe(c.get_class().get_name), 60)
        entry = {"name": s(safe(c.get_name), 60), "class": cn}
        if "InteractableComponent" in cn:
            entry["interactable"] = dump_interactable(c)
        elif "InteractionLinkComponent" in cn:
            entry["link"] = dump_link(c)
        elif "OperationReceiver" in cn:
            entry["receiver"] = dump_generic(c)
        elif "StaticMeshComponent" in cn or "LightComponent" in cn or "BoxComponent" in cn:
            entry["mesh"] = s(gp(c, "StaticMesh"), 90)
            entry["visible"] = s(safe(c.is_visible), 10)
            entry["col_enabled"] = s(safe(c.get_collision_enabled), 30)
            entry["col_profile"] = s(safe(c.get_collision_profile_name), 40)
            entry["intensity"] = s(gp(c, "Intensity"), 20)
        info["comps"].append(entry)
    return info

try:
    assets = {}
    for p in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False):
        sp = str(p)
        low = sp.lower()
        if "jiguanqiang" in low or "jiguandeng" in low:
            d = unreal.EditorAssetLibrary.find_asset_data(p)
            assets[sp] = s(safe(lambda d=d: d.asset_class_path.asset_name), 40)
    report["assets"] = assets

    ls = {}
    exts = getattr(unreal, "MovieSceneSequenceExtensions", None)
    report["has_msext"] = exts is not None
    for nm in ("LS_jiguanqiang1", "LS_jiguanqiang2"):
        a = unreal.load_asset("/Game/" + nm)
        info = {"loaded": s(a, 90)}
        if a and exts is not None:
            for fn in ("get_playback_range", "get_display_rate"):
                info[fn] = s(safe(getattr(exts, fn), a), 160)
            info["msext_names"] = [n for n in dir(exts) if "bind" in n.lower()]
            try:
                info["bindings"] = [s(b, 120) for b in exts.get_bindings(a)]
            except Exception as exc:
                info["bindings"] = "<ERR " + repr(exc)[:70] + ">"
        ls[nm] = info
    report["level_sequences"] = ls

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    report["actor_count"] = len(actors)
    jiguan, lsa, alls, ext = [], [], [], []
    for a in actors:
        lbl = s(safe(a.get_actor_label), 70)
        cn = s(safe(a.get_class().get_name), 70)
        alls.append([lbl, cn])
        if "jiguan" in lbl.lower():
            try:
                jiguan.append(dump_actor(a))
            except Exception as exc:
                jiguan.append({"label": lbl, "error": repr(exc)[:140]})
        if "LevelSequence" in cn:
            lsa.append({"label": lbl, "class": cn, "loc": v3(safe(a.get_actor_location)),
                        "sequence": s(gp(a, "Sequence"), 90),
                        "auto_play": s(gp(a, "bAutoPlay"), 10),
                        "player": s(gp(a, "Player"), 60)})
        for c in comps(a):
            if "InteractionLinkComponent" in s(safe(c.get_class().get_name), 60):
                dl = dump_link(c)
                if "jiguan" in json.dumps(dl, ensure_ascii=False).lower():
                    ext.append({"owner": lbl, "owner_class": cn, "link": dl})
    report["jiguan_actors"] = jiguan
    report["level_sequence_actors"] = lsa
    report["links_touching_jiguan"] = ext
    report["all_actors"] = alls
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("[PROBE JIGUAN done]")