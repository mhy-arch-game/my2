import json, os, traceback
import unreal
BP = "/Game/bclass_source/modern_swift_actor"
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_era_grouping.json")
report = {}
ancient = unreal.TimeEra.ANCIENT
modern = unreal.TimeEra.MODERN


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


def sync(owner_era_comp, era_comp, notify):
    """把 portal 的 OwnerEra 同步成 TimeEraComponent.Era（分组自洽）。"""
    e, ev = call(era_comp.get_editor_property, "Era")
    if e != "ok":
        return "no era"
    call(owner_era_comp.modify)
    if notify:
        call(owner_era_comp.set_editor_property, "OwnerEra", ev,
             notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    else:
        call(owner_era_comp.set_editor_property, "OwnerEra", ev)
    return "%s -> %s" % (str(call(owner_era_comp.get_editor_property, "OwnerEra")[1]), str(ev)[:22])


try:
    # ---- blueprint template ----
    bp = unreal.EditorAssetLibrary.load_asset(BP)
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    era_t = portal_t = None
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        e1, d = call(subsys.k2_find_subobject_data_from_handle, h)
        if e1 != "ok":
            continue
        e3, obj = call(flib.get_object, d)
        if obj is None:
            continue
        c = obj.get_class().get_name()
        if c == "TimeEraComponent":
            era_t = obj
        elif c == "TimeEraPortalComponent":
            portal_t = obj
    report["template_owner_era"] = sync(portal_t, era_t, True) if (era_t and portal_t) else "missing"
    report["compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["save_bp"] = call(unreal.EditorAssetLibrary.save_loaded_asset, bp)[0]

    # ---- level instances ----
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rows = []
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        era = a.get_component_by_class(unreal.TimeEraComponent)
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        loc = a.get_actor_location()
        yaw = float(a.get_actor_rotation().yaw)
        if era is None or portal is None:
            rows.append({"label": lbl, "error": "missing era/portal"})
            continue
        res = sync(portal, era, False)
        ev = str(call(era.get_editor_property, "Era")[1])
        off = float(call(portal.get_editor_property, "VerticalOffset")[1])
        # 复刻 C++ 的取号规则：Ancient -> +VerticalOffset，Modern -> -VerticalOffset
        signed = off if "ANCIENT" in ev else -off
        rows.append({
            "label": lbl, "Era": ev.replace("<TimeEra.", "").replace(": 0>", "").replace(": 1>", ""),
            "sync": res, "yaw": round(yaw, 1), "z": round(float(loc.z), 1),
            "VerticalOffset": off, "signedZ": signed,
            "direction": "UP" if signed > 0 else ("DOWN" if signed < 0 else "?"),
            "destZ": round(float(loc.z) + signed, 1),
        })
    rows.sort(key=lambda r: int("".join(c for c in r["label"] if c.isdigit()) or 0))
    report["devices"] = rows
    report["save_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[FIX ERA GROUPING done]")
