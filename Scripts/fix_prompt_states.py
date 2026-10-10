import json, os, traceback
import unreal
BP_DOOR = "/Game/bclass_source/active_door"
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_prompt_states.json")
report = {"doors": [], "lamps": [], "char": {}}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def set_texts(ic, closed, opened):
    call(ic.modify)
    r = {}
    r["closed"] = call(ic.set_editor_property, "InteractionPrompt", closed)[0]
    r["open"] = call(ic.set_editor_property, "InteractionPromptOpen", opened)[0]
    r["now_closed_state"] = str(call(ic.get_editor_property, "InteractionPrompt")[1])
    r["now_open_state"] = str(call(ic.get_editor_property, "InteractionPromptOpen")[1])
    return r


try:
    # ---------- 1) 门的蓝图模板 ----------
    bp = unreal.EditorAssetLibrary.load_asset(BP_DOOR)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e != "ok":
            continue
        e2, obj = call(flib.get_object, d)
        if obj is None or obj.get_class().get_name() != "InteractableComponent":
            continue
        report["door_template"] = set_texts(obj, "开门", "关门")
        break
    report["door_template_compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["door_template_save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---------- 2) 关卡：24 扇门 + 3 盏机关灯 ----------
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
            cn = a.get_class().get_name()
        except Exception:
            continue
        ic = a.get_component_by_class(unreal.InteractableComponent)
        if ic is None:
            continue
        if cn == "active_door_C":
            report["doors"].append({"label": lbl, **set_texts(ic, "开门", "关门")})
        elif lbl.startswith("jiguandeng"):
            report["lamps"].append({"label": lbl, **set_texts(ic, "开灯", "关灯")})

    # ---------- 3) 角色提示组件：降低高度 + 压扁面板 ----------
    cbp = unreal.EditorAssetLibrary.load_asset(CHAR)
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(cbp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e != "ok":
            continue
        e2, obj = call(flib.get_object, d)
        e3, var = call(flib.get_variable_name, d)
        if obj is None or str(var) != "InteractionPrompt":
            continue
        call(obj.modify)
        report["char"]["before_height"] = str(call(obj.get_editor_property, "WorldPromptHeightOffset")[1])
        report["char"]["before_size"] = str(call(obj.get_editor_property, "WorldPromptDrawSize")[1])[:70]
        report["char"]["height"] = call(obj.set_editor_property, "WorldPromptHeightOffset", 0.0,
                                        notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        report["char"]["size"] = call(obj.set_editor_property, "WorldPromptDrawSize", unreal.Vector2D(320.0, 64.0),
                                      notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
        report["char"]["after_height"] = str(call(obj.get_editor_property, "WorldPromptHeightOffset")[1])
        report["char"]["after_size"] = str(call(obj.get_editor_property, "WorldPromptDrawSize")[1])[:70]
        break
    report["char_compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, cbp)[0]
    report["char_save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, cbp)[0]

    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX PROMPT STATES done]")
