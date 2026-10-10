import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "remove_wall_receiver.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:100] + ">"

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for a in ss.get_all_level_actors():
        if str(call(a.get_actor_label)) != "jiguanqiang1":
            continue
        handles = list(call(sds.k2_gather_subobject_data_for_instance, a) or [])
        table = []
        context = None
        victim = None
        for h in handles:
            d = call(sds.k2_find_subobject_data_from_handle, h)
            if isinstance(d, str):
                table.append({"err": d[:70]})
                continue
            obj = call(flib.get_object, d)
            vn = call(flib.get_variable_name, d)
            cn = str(call(obj.get_class().get_name)) if not isinstance(obj, str) else "?"
            on = str(call(obj.get_name)) if not isinstance(obj, str) else "?"
            table.append({"var": str(vn)[:40], "obj": on[:40], "class": cn[:60]})
            if not isinstance(obj, str):
                try:
                    if obj == a:
                        context = h
                except Exception:
                    pass
            if "InteractionOperationReceiver" in cn:
                victim = h
        report["handles_before"] = table
        report["context_found"] = context is not None
        if context is None and handles:
            context = handles[0]
        if victim is not None and context is not None:
            report["delete"] = str(call(sds.k2_delete_subobject_from_instance, context, victim))[:200]
        else:
            report["delete"] = "victim=%s context=%s" % (victim is not None, context is not None)
        after = [str(call(c.get_class().get_name)) for c in list(call(a.get_components_by_class, unreal.ActorComponent) or [])]
        report["components_after"] = after
        report["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:80]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###REMOVE " + json.dumps(report, ensure_ascii=True)[:2000])
print("[REMOVE done]")