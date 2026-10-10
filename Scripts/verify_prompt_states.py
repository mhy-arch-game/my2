import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
CHAR = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_prompt_states.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:120], None)


try:
    cls = unreal.InteractableComponent
    report["has_new_field"] = "interaction_prompt_open" in dir(cls)
    rows = []
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
        if cn == "active_door_C" or lbl.startswith("jiguandeng"):
            rows.append({"label": lbl,
                         "closed_prompt": str(call(ic.get_editor_property, "InteractionPrompt")[1])[:24],
                         "open_prompt": str(call(ic.get_editor_property, "InteractionPromptOpen")[1])[:24],
                         "IsOpen": str(call(ic.is_open)[1])})
    rows.sort(key=lambda r: r["label"])
    report["from_disk"] = rows[:8] + rows[-6:]
    report["count"] = len(rows)
    bp = unreal.EditorAssetLibrary.load_asset(CHAR)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e != "ok":
            continue
        e2, obj = call(flib.get_object, d)
        e3, var = call(flib.get_variable_name, d)
        if obj is not None and str(var) == "InteractionPrompt":
            report["char_prompt_component"] = {
                p: str(call(obj.get_editor_property, p)[1])[:70]
                for p in ("WorldPromptHeightOffset", "WorldPromptDrawSize", "WorldPromptFacingYaw", "bWorldSpacePrompt")}
            break
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY PROMPT STATES done]")
