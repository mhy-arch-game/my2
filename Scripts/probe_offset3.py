import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_offset3.json")
report = {}


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
        base = {"label": lbl, "loc": v3(a.get_actor_location())}
        for c in a.get_components_by_class(unreal.SceneComponent):
            row = dict(base)
            row["comp"] = c.get_name()
            row["class"] = c.get_class().get_name()
            if c.get_class().get_name() == "StaticMeshComponent":
                row["profile"] = str(call(c.get_collision_profile_name)[1])
                row["collision_enabled"] = str(call(c.get_collision_enabled)[1])
                row["resp_to_pawn"] = str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)[1])
                row["resp_to_vis"] = str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_VISIBILITY)[1])
                row["bVisible"] = str(call(c.get_editor_property, "bVisible")[1])
                row["bHiddenInGame"] = str(call(c.get_editor_property, "bHiddenInGame")[1])
                e2, bounds = call(c.get_local_bounds)
                if e2 == "ok" and bounds is not None and len(bounds) == 2:
                    row["local_min"] = v3(bounds[0])
                    row["local_max"] = v3(bounds[1])
            rows.append(row)
    report["primitives"] = rows
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[OFFSET PROBE 3d saved]")
