import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_wall_group.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def effect(action, bActive):
    a = str(action)
    if "SET_ACTOR_HIDDEN" in a:
        return "隐藏" if bActive else "显形"
    if "SET_ACTOR_VISIBLE" in a:
        return "显形" if bActive else "隐藏"
    return "什么都不做"


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()
    lamps, masters, slaves = [], [], []
    for a in actors:
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        if ic is not None and lbl.startswith("jiguandeng"):
            lamps.append({"label": lbl,
                          "bToggleLights": str(call(ic.get_editor_property, "bToggleLights")[1]),
                          "LightComponentNames": str(call(ic.get_editor_property, "LightComponentNames")[1]),
                          "bStartOpen": str(call(ic.get_editor_property, "bStartOpen")[1]),
                          "bTrackOpenState": str(call(ic.get_editor_property, "bTrackOpenState")[1])})
        lk = a.get_component_by_class(unreal.InteractionLinkComponent)
        if lk is not None:
            e, arr = call(lk.get_editor_property, "Entries")
            for r in (arr or []):
                masters.append({"label": lbl,
                                "op": str(call(r.get_editor_property, "Operation")[1]),
                                "channel": str(call(r.get_editor_property, "Channel")[1]),
                                "mirror": str(call(r.get_editor_property, "bMirrorSourceState")[1]),
                                "closed": str(call(r.get_editor_property, "ClosedOperation")[1])})
        rc = a.get_component_by_class(unreal.InteractionOperationReceiverComponent)
        if rc is not None:
            e, arr = call(rc.get_editor_property, "Bindings")
            rows = []
            for r in (arr or []):
                rows.append({"op": str(call(r.get_editor_property, "Operation")[1]),
                             "action": str(call(r.get_editor_property, "Action")[1])})
            slaves.append({"label": lbl, "channel": str(call(rc.get_editor_property, "Channel")[1]), "rows": rows})
    report["lamps"] = lamps
    report["masters"] = masters
    report["slaves"] = slaves

    # ---- 逻辑干跑：主物开(交互1)/关(交互2) 时，从物应该变成什么 ----
    dry = []
    for m in masters:
        for s in slaves:
            if s["channel"] != m["channel"]:
                continue
            for step, bActive in ((1, True), (2, False)):
                op = m["closed"] if (m["mirror"] == "True" and not bActive) else m["op"]
                hit = None
                for r in s["rows"]:
                    if r["op"] == op:
                        hit = r
                        break
                dry.append({"master": m["label"], "slave": s["label"], "step": step,
                            "bActive": bActive, "发操作": op,
                            "命中": (hit["action"].split(".")[-1].split(":")[0] if hit else "无匹配行"),
                            "从物结果": effect(hit["action"], bActive) if hit else "什么都不做"})
    report["dry_run"] = dry
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY WALL GROUP done]")
