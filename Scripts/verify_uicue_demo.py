import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
DA = "/Game/MHY_ARCH_GAME/UI/DA_UiCueSet"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_uicue_demo.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    da = unreal.EditorAssetLibrary.load_asset(DA)
    e, cues = call(da.get_editor_property, "Cues")
    rows = []
    for c in (cues or []):
        rows.append({"CueId": str(call(c.get_editor_property, "CueId")[1]),
                     "Trigger": str(call(c.get_editor_property, "Trigger")[1]),
                     "TriggerId": str(call(c.get_editor_property, "TriggerId")[1]),
                     "Text": str(call(c.get_editor_property, "Text")[1])[:40],
                     "bOnce": str(call(c.get_editor_property, "bOnce")[1])})
    report["cues_from_disk"] = rows
    report["gap"] = str(call(da.get_editor_property, "QueueGapSeconds")[1])

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    start = None
    vol = None
    for a in actor_ss.get_all_level_actors():
        try:
            cn = a.get_class().get_name()
            lbl = a.get_actor_label()
        except Exception:
            continue
        if cn == "PlayerStart":
            start = a
        if cn == "UiCueTriggerVolume":
            vol = a
    report["level_volume"] = None
    if vol is not None:
        box = vol.get_component_by_class(unreal.BoxComponent)
        report["level_volume"] = {
            "label": vol.get_actor_label(),
            "trigger_id": str(call(vol.get_editor_property, "TriggerId")[1]),
            "loc": [round(float(vol.get_actor_location().x), 1), round(float(vol.get_actor_location().y), 1),
                    round(float(vol.get_actor_location().z), 1)],
            "box_extent": str(call(box.get_editor_property, "BoxExtent")[1])[:70] if box else "-",
            "contains_player_start": str(call(vol.contains_location, start.get_actor_location())[1]) if start else "-",
        }
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY UICUE DEMO done]")
