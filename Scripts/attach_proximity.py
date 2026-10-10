import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "attach_proximity.json")
TARGETS = {"jiguanqiang1": "/Game/LS_jiguanqiang1", "jiguanqiang2": "/Game/LS_jiguanqiang2"}
report = {}

def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:200], None)

def cname(c):
    try:
        return str(c.get_class().get_name())
    except Exception:
        return "?"

try:
    report["has_class"] = hasattr(unreal, "ProximitySequenceComponent")
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    CLS = unreal.ProximitySequenceComponent
    out = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label)[1])
        if lbl not in TARGETS:
            continue
        info = {"class": str(call(a.get_class().get_name)[1])}
        found = None
        for c in list(call(a.get_components_by_class, unreal.ActorComponent)[1] or []):
            if "ProximitySequenceComponent" in cname(c):
                found = c
        if found is None:
            handles = list(call(sds.k2_gather_subobject_data_for_instance, a)[1] or [])
            info["handles"] = len(handles)
            parent = None
            for h in handles:
                e, d = call(sds.k2_find_subobject_data_from_handle, h)
                vn = None
                if e == "ok":
                    vn = call(flib.get_variable_name, d)[1]
                if str(vn) in ("StaticMeshComponent0", "DefaultSceneRoot"):
                    parent = h
                    info["parent"] = str(vn)
                    break
            if parent is None and handles:
                parent = handles[0]
                info["parent"] = "index0"
            params = unreal.AddNewSubobjectParams()
            call(params.set_editor_property, "parent_handle", parent)
            call(params.set_editor_property, "new_class", CLS)
            call(params.set_editor_property, "conform_transform_to_parent", False)
            r = call(sds.add_new_subobject, params)
            info["add"] = str(r[0])[:160]
            if r[0] == "ok":
                info["add_ret"] = str(r[1])[:220]
            for c in list(call(a.get_components_by_class, unreal.ActorComponent)[1] or []):
                if "ProximitySequenceComponent" in cname(c):
                    found = c
        if found is not None:
            seq = unreal.load_asset(TARGETS[lbl])
            info["seq_asset"] = str(call(seq.get_path_name)[1])
            call(found.set_editor_property, "Sequence", seq)
            info["seq_after"] = str(call(found.get_editor_property, "Sequence")[1])[:130]
            info["trigger"] = str(call(found.get_editor_property, "TriggerDistance")[1])
            info["once"] = str(call(found.get_editor_property, "bOnce")[1])
            info["blockOnFinish"] = str(call(found.get_editor_property, "bBlockOnFinish")[1])
            info["initial"] = str(call(found.get_editor_property, "bApplyInitialStateOnBeginPlay")[1])
        else:
            info["component"] = "NOT FOUND"
        out[lbl] = info
    report["actors"] = out
    report["save"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()[-600:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###ATTACH " + json.dumps(report, ensure_ascii=True)[:2200])
print("[ATTACH done]")