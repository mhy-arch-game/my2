import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_offset2.json")
report = {}
P = ("TargetMode", "VerticalOffset", "TeleportOffset", "bPlaceOnGround", "GroundTraceDistance",
     "GroundClearance", "bMatchCounterpartYaw", "bSwitchEra", "PortalCooldown", "CounterpartId",
     "bAutoDetectEra", "OwnerEra")


def v3(v):
    try:
        return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]
    except Exception:
        return str(v)[:60]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:110], None)


def scene_world(c, actor_loc, actor_rot):
    for fn in ("get_world_location", "k2_get_component_location", "get_component_location"):
        f = getattr(c, fn, None)
        if f is None:
            continue
        e, v = call(f)
        if e == "ok":
            return ["api:" + fn, v3(v)]
    e, t = call(unreal.SceneComponent.k2_get_component_to_world if False else c.get_editor_property, "RelativeLocation")
    return ["rel_only", str(t)[:70] if e == "ok" else e]


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        prot = a.get_actor_rotation()
        aloc = a.get_actor_location()
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        era = a.get_component_by_class(unreal.TimeEraComponent)
        row = {"label": lbl, "loc": v3(aloc), "rot": [round(float(prot.roll), 2), round(float(prot.pitch), 2), round(float(prot.yaw), 2)]}
        e, ev = call(era.get_editor_property, "Era") if era else ("none", None)
        row["era"] = str(ev) if e == "ok" else str(e)
        if portal is not None:
            props = {}
            for p in P:
                e2, v = call(portal.get_editor_property, p)
                if e2 == "ok":
                    props[p] = str(v)
            row["portal"] = props
        comps = []
        for c in a.get_components_by_class(unreal.SceneComponent):
            d = {"name": c.get_name(), "class": c.get_class().get_name()}
            e3, rel = call(c.get_editor_property, "RelativeLocation")
            d["rel"] = str(rel)[:70] if e3 == "ok" else str(e3)
            try:
                d["world"] = scene_world(c, aloc, prot)
            except Exception as exc:
                d["world"] = "err " + repr(exc)[:60]
            comps.append(d)
        row["scene_components"] = comps
        rows.append(row)
    rows.sort(key=lambda r: int("".join(ch for ch in r["label"] if ch.isdigit()) or 0))
    report["actors"] = rows
    report["count"] = len(rows)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[OFFSET PROBE 2 saved]")
