import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_wall_group.json")
report = {"masters": [], "slaves": []}
MASTERS = ["jiguandeng1", "jiguandeng2", "jiguandeng3", "men20"]
SLAVES = ["jiguanqiang0", "jiguanqiang1", "jiguanqiang3", "jiguanqiang4"]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


def by_label(actors, lbl):
    for a in actors:
        try:
            if a.get_actor_label() == lbl:
                return a
        except Exception:
            pass
    return None


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()

    # ---- 主物：mirror=false（操作名固定，状态由 bActive 携带）----
    for lbl in MASTERS:
        a = by_label(actors, lbl)
        lk = a.get_component_by_class(unreal.InteractionLinkComponent) if a else None
        if lk is None:
            report["masters"].append({"label": lbl, "error": "no link component"})
            continue
        call(lk.modify)
        e, arr = call(lk.get_editor_property, "Entries")
        rows = list(arr) if (e == "ok" and arr) else []
        out = []
        for r in rows:
            e2, before = call(r.get_editor_property, "bMirrorSourceState")
            call(r.set_editor_property, "bMirrorSourceState", False)
            out.append({"before_mirror": str(before),
                        "after_mirror": str(call(r.get_editor_property, "bMirrorSourceState")[1]),
                        "op": str(call(r.get_editor_property, "Operation")[1]),
                        "channel": str(call(r.get_editor_property, "Channel")[1])})
        w = call(lk.set_editor_property, "Entries", rows,
                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        report["masters"].append({"label": lbl, "rows": out, "write": w})

    # ---- 从物：只保留一条 Open -> SET_ACTOR_HIDDEN（状态驱动）----
    for lbl in SLAVES:
        a = by_label(actors, lbl)
        rc = a.get_component_by_class(unreal.InteractionOperationReceiverComponent) if a else None
        if rc is None:
            report["slaves"].append({"label": lbl, "error": "no receiver"})
            continue
        call(rc.modify)
        e, arr = call(rc.get_editor_property, "Bindings")
        before = [str(call(x.export_text)[1])[:90] for x in (arr or [])]
        new_row = unreal.InteractionOperationBinding()
        call(new_row.set_editor_property, "Operation", "Open")
        call(new_row.set_editor_property, "Action", unreal.InteractionOperationAction.SET_ACTOR_HIDDEN)
        call(new_row.set_editor_property, "Duration", 0.0)
        w = call(rc.set_editor_property, "Bindings", [new_row],
                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        e2, arr2 = call(rc.get_editor_property, "Bindings")
        report["slaves"].append({"label": lbl, "before": before, "write": w,
                                 "after": [str(call(x.export_text)[1])[:90] for x in (arr2 or [])],
                                 "channel": str(call(rc.get_editor_property, "Channel")[1])})

    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX WALL GROUP done]")
