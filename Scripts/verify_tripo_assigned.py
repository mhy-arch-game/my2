import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_tripo_assigned.json")
MAP = "/Game/FirstPerson/firstvision"
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def v3(v):
    try:
        return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]
    except Exception:
        return str(v)[:40]


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dev = []
    gl_users = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            e, m = call(c.get_editor_property, "StaticMesh")
            mp = "" if m is None else str(m)
            if "SM_GrenadeLauncher" in mp:
                gl_users.append([lbl, a.get_class().get_name()])
            if lbl.startswith("kongjianchuansuoqi"):
                e2, lb = call(c.get_local_bounds)
                dev.append({"label": lbl, "mesh": mp[-46:],
                            "size": [round(float(lb[1].x - lb[0].x), 1), round(float(lb[1].y - lb[0].y), 1),
                                     round(float(lb[1].z - lb[0].z), 1)] if (e2 == "ok" and lb and len(lb) > 1) else str(e2)})
    dev.sort(key=lambda r: int("".join(ch for ch in r["label"] if ch.isdigit()) or 0))
    report["devices"] = dev
    report["SM_GrenadeLauncher_users"] = gl_users[:20]
    report["SM_GrenadeLauncher_user_count"] = len(gl_users)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY TRIPO ASSIGNED done]")
