import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_interact_two_bugs.json")
LAMPS = ("jiguandeng1", "jiguandeng2", "jiguandeng3")
report = {"lamps": [], "rows": []}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


def find_actor(actors, label):
    for a in actors:
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None


try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_ss.get_all_level_actors()

    # ---------- 1) 灯：bToggleLights + LightComponentNames ----------
    for lbl in LAMPS:
        a = find_actor(actors, lbl)
        if a is None:
            report["lamps"].append({"label": lbl, "error": "actor not found"})
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        if ic is None:
            report["lamps"].append({"label": lbl, "error": "no InteractableComponent"})
            continue
        light_names = [c.get_name() for c in a.get_components_by_class(unreal.LightComponent)]
        call(ic.modify)
        before = {"bToggleLights": str(call(ic.get_editor_property, "bToggleLights")[1]),
                  "LightComponentNames": str(call(ic.get_editor_property, "LightComponentNames")[1]),
                  "bUseBuiltInToggle": str(call(ic.get_editor_property, "bUseBuiltInToggle")[1]),
                  "bStartOpen": str(call(ic.get_editor_property, "bStartOpen")[1])}
        w1 = call(ic.set_editor_property, "bToggleLights", True)[0]
        w2 = call(ic.set_editor_property, "LightComponentNames", [unreal.Name(n) for n in light_names])[0]
        # 一开始就亮/灭由 bStartOpen 决定；同时把初始状态应用一次（BeginPlay 会做，这里只保证数据正确）
        report["lamps"].append({"label": lbl, "light_comps": light_names, "before": before,
                                "set_bToggleLights": w1, "set_light_names": w2,
                                "after_bToggleLights": str(call(ic.get_editor_property, "bToggleLights")[1]),
                                "after_light_names": str(call(ic.get_editor_property, "LightComponentNames")[1])})

    # ---------- 2) jiguanqiang3：把 Close 行的 Action 设成 SET_ACTOR_VISIBLE ----------
    target = find_actor(actors, "jiguanqiang3")
    if target is None:
        report["rows"].append({"error": "jiguanqiang3 not found"})
    else:
        rc = target.get_component_by_class(unreal.InteractionOperationReceiverComponent)
        if rc is None:
            report["rows"].append({"error": "no receiver component"})
        else:
            call(rc.modify)
            e, arr = call(rc.get_editor_property, "Bindings")
            rows = list(arr) if (e == "ok" and arr) else []
            report["rows"].append({"raw_before": [str(call(r.export_text)[1])[:120] for r in rows]})
            changed = []
            for r in rows:
                e2, op = call(r.get_editor_property, "Operation")
                if e2 == "ok" and str(op) == "Close":
                    e3, act = call(r.get_editor_property, "Action")
                    changed.append({"before_action": str(act)})
                    call(r.set_editor_property, "Action", unreal.InteractionOperationAction.SET_ACTOR_VISIBLE)
                    changed[-1]["after_action"] = str(call(r.get_editor_property, "Action")[1])
                    changed[-1]["raw_after"] = str(call(r.export_text)[1])[:120]
            w = call(rc.set_editor_property, "Bindings", rows,
                     notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
            report["rows"].append({"changes": changed, "write_bindings": w})
            e4, arr2 = call(rc.get_editor_property, "Bindings")
            if e4 == "ok" and arr2:
                report["rows"].append({"raw_after_all": [str(call(x.export_text)[1])[:120] for x in arr2]})

    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX INTERACT TWO BUGS done]")
