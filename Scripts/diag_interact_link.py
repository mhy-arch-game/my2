import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "diag_interact_link.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return str(v)[:40]


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    interactables, links, receivers, lights = [], [], [], []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
            cn = a.get_class().get_name()
        except Exception:
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        if ic is not None:
            e = {"label": lbl, "class": cn, "loc": v3(a.get_actor_location())}
            for p in ("bEnabled", "bUseBuiltInToggle", "bToggleLights", "bTrackOpenState", "bStartOpen"):
                e[p] = str(call(ic.get_editor_property, p)[1])
            e["IsOpen"] = str(call(ic.is_open)[1])
            e["prompt"] = str(call(ic.get_editor_property, "InteractionPrompt")[1])[:40]
            e["ToggleComponentNames"] = str(call(ic.get_editor_property, "ToggleComponentNames")[1])
            e["LightComponentNames"] = str(call(ic.get_editor_property, "LightComponentNames")[1])
            e["LightComponents_count"] = str(len(call(ic.get_editor_property, "LightComponents")[1] or []))
            e["light_comps_on_actor"] = [c.get_name() for c in a.get_components_by_class(unreal.LightComponent)]
            e["scene_comps"] = [c.get_name() for c in a.get_components_by_class(unreal.SceneComponent)]
            interactables.append(e)
        lk = a.get_component_by_class(unreal.InteractionLinkComponent)
        if lk is not None:
            entries = []
            e2, arr = call(lk.get_editor_property, "Entries")
            if e2 == "ok" and arr:
                for it in arr:
                    entries.append({p: str(call(it.get_editor_property, p)[1]) for p in
                                    ("Operation", "Channel", "bMirrorSourceState", "ClosedOperation", "Value", "Location")})
                    e3, tg = call(it.get_editor_property, "Targets")
                    entries[-1]["Targets"] = [x.get_actor_label() for x in tg] if (e3 == "ok" and tg) else []
            links.append({"label": lbl, "class": cn, "entries": entries,
                          "bAutoBindInteractable": str(call(lk.get_editor_property, "bAutoBindInteractable")[1]),
                          "bSyncOnBeginPlay": str(call(lk.get_editor_property, "bSyncOnBeginPlay")[1])})
        rc = a.get_component_by_class(unreal.InteractionOperationReceiverComponent)
        if rc is not None:
            bnds = []
            e4, arr4 = call(rc.get_editor_property, "Bindings")
            if e4 == "ok" and arr4:
                for b in arr4:
                    bnds.append({p: str(call(b.get_editor_property, p)[1]) for p in ("Operation", "Action", "Duration")})
            receivers.append({"label": lbl, "class": cn,
                              "Channel": str(call(rc.get_editor_property, "Channel")[1]),
                              "bEnabled": str(call(rc.get_editor_property, "bEnabled")[1]),
                              "bindings": bnds})
        lc = a.get_components_by_class(unreal.LightComponent)
        if lc:
            for c in lc:
                lights.append({"actor": lbl, "actor_class": cn, "comp": c.get_name(),
                               "visible": str(call(c.is_visible)[1]),
                               "intensity": str(call(c.get_editor_property, "Intensity")[1])})
    report["interactables"] = interactables
    report["links"] = links
    report["receivers"] = receivers
    report["lights"] = lights
    report["counts"] = {"interactables": len(interactables), "links": len(links),
                        "receivers": len(receivers), "light_comps": len(lights)}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[DIAG INTERACT LINK done]")
