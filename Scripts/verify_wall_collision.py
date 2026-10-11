import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_wall_collision.json")
report = {}
WALLS = ["jiguanqiang0", "jiguanqiang1", "jiguanqiang3", "jiguanqiang4"]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    rows = []
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl not in WALLS:
            continue
        e = {"label": lbl, "class": a.get_class().get_name()}
        e["actor_enable_collision"] = str(call(a.get_actor_enable_collision)[1])
        comps = []
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            d = {"comp": c.get_name(), "class": c.get_class().get_name(),
                 "profile": str(call(c.get_collision_profile_name)[1]),
                 "enabled": str(call(c.get_collision_enabled)[1]),
                 "pawn": str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)[1]),
                 "vis": str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_VISIBILITY)[1])}
            comps.append(d)
        e["primitives"] = comps
        e["all_components"] = [c.get_class().get_name() for c in a.get_components_by_class(unreal.ActorComponent)]
        rows.append(e)
    rows.sort(key=lambda r: r["label"])
    report["walls"] = rows
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY WALL COLLISION done]")
