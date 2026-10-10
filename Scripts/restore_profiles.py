import json, os, traceback
import unreal
SRC = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "restore_source.json")
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "restore_profiles.json")
DEVICE_PREFIX = "kongjianchuansuoqi"
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    with open(SRC, "r", encoding="utf-8") as fh:
        src = json.load(fh)
    by_path = {}
    by_label = {}
    dist = {}
    for m in src.get("meshes", []):
        by_path.setdefault(m["actor"], {})[m["comp"]] = m["profile"]
        by_label.setdefault((m["label"], m["comp"]), []).append(m["profile"])
        dist[m["profile"]] = dist.get(m["profile"], 0) + 1
    report["source_actors"] = len(by_path)
    report["source_meshes"] = sum(len(v) for v in by_path.values())
    report["source_distribution"] = dist
    report["source_facts"] = src.get("facts")

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    changed = matched = already = 0
    unmatched = []
    for a in actor_ss.get_all_level_actors():
        try:
            path = a.get_path_name()
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            target = None
            hit = by_path.get(path)
            if hit is not None:
                target = hit.get(c.get_name())
            if target is None:
                cand = by_label.get((lbl, c.get_name()))
                if cand and len(cand) == 1:
                    target = cand[0]
            cur = str(call(c.get_collision_profile_name)[1])
            if target is None:
                unmatched.append([lbl, c.get_name(), cur])
                continue
            matched += 1
            if cur == target:
                already += 1
                continue
            if call(c.set_collision_profile_name, target)[0] == "ok":
                changed += 1
    report["matched"] = matched
    report["already_correct"] = already
    report["changed"] = changed
    report["unmatched_count"] = len(unmatched)
    report["unmatched_sample"] = unmatched[:20]

    # ONLY the 10 portal devices get IgnoreOnlyPawn (label filter this time!)
    devices = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith(DEVICE_PREFIX):
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            e = call(c.set_collision_profile_name, "IgnoreOnlyPawn")[0]
            devices.append([lbl, c.get_name(), e,
                           str(call(c.get_collision_profile_name)[1]),
                           str(call(c.get_collision_response_to_channel, unreal.CollisionChannel.ECC_PAWN)[1])])
    report["devices_touched"] = len(devices)
    report["devices"] = devices
    report["assert_devices_10"] = (len(devices) == 10)
    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[RESTORE PROFILES done]")
