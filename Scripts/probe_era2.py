import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_era2.json")
report = {}
ERA_P = ("Era", "bExistsInBothEras", "bGateVisibility", "bGateCollision", "bGateTick")
PORTAL_P = ("bSwitchEra", "bAutoDetectEra", "OwnerEra", "TargetMode", "VerticalOffset")


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:110], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    counters = {"Ancient": 0, "Modern": 0, "both": 0}
    rows = []
    starts = []
    hidden_devices = []
    for a in actor_ss.get_all_level_actors():
        era = a.get_component_by_class(unreal.TimeEraComponent)
        if era is not None:
            both = call(era.get_editor_property, "bExistsInBothEras")[1]
            if both:
                counters["both"] += 1
            else:
                ev = str(call(era.get_editor_property, "Era")[1])
                if "ANCIENT" in ev:
                    counters["Ancient"] += 1
                elif "MODERN" in ev:
                    counters["Modern"] += 1
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        cn = a.get_class().get_name()
        if "PlayerStart" in cn or "PlayerStart" in lbl:
            starts.append({"label": lbl, "class": cn,
                           "loc": [round(float(a.get_actor_location().x), 1),
                                   round(float(a.get_actor_location().y), 1),
                                   round(float(a.get_actor_location().z), 1)]})
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        row = {"label": lbl, "class": cn}
        e, hv = call(a.get_editor_property, "bHidden")
        row["actor_bHidden"] = str(hv) if e == "ok" else str(e)
        if era is not None:
            row["era"] = {p: str(call(era.get_editor_property, p)[1]) for p in ERA_P}
        else:
            row["era"] = "NONE"
        p = a.get_component_by_class(unreal.TimeEraPortalComponent)
        if p is not None:
            row["portal"] = {k: str(call(p.get_editor_property, k)[1]) for k in PORTAL_P}
        for m in a.get_components_by_class(unreal.StaticMeshComponent):
            row["mesh"] = {"comp": m.get_name(),
                           "bHiddenInGame": str(call(m.get_editor_property, "bHiddenInGame")[1]),
                           "editor_visible": str(call(m.is_visible)[1])}
        rows.append(row)
    rows.sort(key=lambda x: int("".join(c for c in x["label"] if c.isdigit()) or 0))
    report["devices"] = rows
    report["level_era_counts"] = counters
    report["player_starts"] = starts
    # what the runtime would hide when the session starts in ANCIENT (the config default)
    for r in rows:
        era = r.get("era")
        if isinstance(era, dict):
            ev = era.get("Era", "")
            both = era.get("bExistsInBothEras", "False")
            gate = era.get("bGateVisibility", "True")
            visible = (both == "True") or ("ANCIENT" in ev)
            if not visible and gate == "True":
                hidden_devices.append(r["label"])
    report["hidden_at_runtime_if_initial_era_ancient"] = hidden_devices
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE ERA2 done]")
