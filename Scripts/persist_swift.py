"""Persist the swift_actor wiring.

Phase A: actor.modify() + explicit CounterpartActor pairing -> save -> reload -> read.
Phase B (only if A fails): put the mode/offset/prompt into the BLUEPRINT TEMPLATE
(VerticalOffset mode), so instances only need the Era override - which is proven to persist.
"""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_SWIFT = "/Game/bclass_source/modern_swift_actor"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "swift_persist.json")
PREFIX = "kongjianchuansuoqi"
VOFF = -5000.0
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:170], None)


def setp(obj, prop, value):
    return call(obj.set_editor_property, prop, value,
                unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]


def snapshot(tag):
    """Reload the map from disk and read each instance back."""
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {}
    for a in actor_ss.get_all_level_actors():
        try:
            by_label[a.get_actor_label()] = a
        except Exception:
            pass
    rows = []
    for n in range(1, 11):
        a = by_label.get(PREFIX + str(n))
        if a is None:
            rows.append({"num": n, "error": "missing"})
            continue
        r = {"num": n}
        era = a.get_component_by_class(unreal.TimeEraComponent)
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        if era is not None:
            e, v = call(era.get_editor_property, "Era")
            r["Era"] = str(v) if e == "ok" else e
        if portal is not None:
            for p in ("TargetMode", "VerticalOffset", "InteractionPrompt"):
                e, v = call(portal.get_editor_property, p)
                r[p] = str(v) if e == "ok" else e
            e, cp = call(portal.get_editor_property, "CounterpartActor")
            cpnum = None
            if e == "ok" and cp is not None:
                try:
                    cl = cp.get_actor_label()
                    if cl.startswith(PREFIX) and cl[len(PREFIX):].isdigit():
                        cpnum = int(cl[len(PREFIX):])
                except Exception:
                    pass
            r["counterpart_num"] = cpnum
            r["pair_ok"] = (cpnum == (n + 1 if n % 2 == 1 else n - 1))
        rows.append(r)
    return rows


def get_actors():
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = {}
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if lbl.startswith(PREFIX) and lbl[len(PREFIX):].isdigit():
            out[int(lbl[len(PREFIX):])] = a
    return out


def save_map():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return call(unreal.EditorLoadingAndSavingUtils.save_map, w, MAP)[0]


def phase_a():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = get_actors()
    log = []
    for num in sorted(actors.keys()):
        a = actors[num]
        is_odd = (num % 2 == 1)
        partner = actors.get(num + 1 if is_odd else num - 1)
        era = a.get_component_by_class(unreal.TimeEraComponent)
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        e = {"num": num}
        e["modify_actor"] = call(a.modify)[0]
        if era is not None:
            e["modify_era"] = call(era.modify)[0]
            e["set_era"] = setp(era, "Era", unreal.TimeEra.ANCIENT if is_odd else unreal.TimeEra.MODERN)
        if portal is not None:
            e["modify_portal"] = call(portal.modify)[0]
            e["set_mode"] = setp(portal, "TargetMode", unreal.TimeEraPortalTargetMode.COUNTERPART_OBJECT)
            e["set_off"] = setp(portal, "VerticalOffset", VOFF)
            e["set_prompt"] = setp(portal, "InteractionPrompt",
                                   "传送至现代" if is_odd else "传送至过去")
            if partner is not None:
                e["set_cp"] = setp(portal, "CounterpartActor", partner)
        log.append(e)
    report["phase_a_writes"] = log
    report["phase_a_save"] = save_map()
    report["phase_a_after_reload"] = snapshot("A")


def find_portal_template():
    bp = lib.load_asset(BP_SWIFT)
    if bp is None:
        return None, None
    subsys = unreal.get_engine_subsystem(unreal.UnrealSubobjectDataSubsystem) if hasattr(unreal, "UnrealSubobjectDataSubsystem") else unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    flib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in list(subsys.k2_gather_subobject_data_for_blueprint(bp)):
        err, data = call(subsys.k2_find_subobject_data_from_handle, h)
        if err != "ok":
            continue
        err2, var = call(flib.get_variable_name, data)
        if str(var) != "TimeEraPortal":
            continue
        err3, obj = call(flib.get_object, data)
        if err3 == "ok":
            return bp, obj
    return bp, None


def phase_b():
    bp, tpl = find_portal_template()
    if tpl is None:
        report["phase_b"] = "portal template not found"
        return
    w = {}
    w["TargetMode"] = setp(tpl, "TargetMode", unreal.TimeEraPortalTargetMode.VERTICAL_OFFSET)
    w["VerticalOffset"] = setp(tpl, "VerticalOffset", VOFF)
    w["InteractionPrompt"] = setp(tpl, "InteractionPrompt", "传送")
    report["phase_b_template_writes"] = w
    report["phase_b_compile"] = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)[0]
    report["phase_b_save_bp"] = call(lib.save_loaded_asset, bp)[0]

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = get_actors()
    log = []
    for num in sorted(actors.keys()):
        a = actors[num]
        is_odd = (num % 2 == 1)
        era = a.get_component_by_class(unreal.TimeEraComponent)
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        e = {"num": num, "modify": call(a.modify)[0]}
        if era is not None:
            call(era.modify)
            e["set_era"] = setp(era, "Era", unreal.TimeEra.ANCIENT if is_odd else unreal.TimeEra.MODERN)
        if portal is not None:
            call(portal.modify)
            # clear any stale per-instance reference; the mode comes from the template
            e["clear_cp"] = setp(portal, "CounterpartActor", None)
        log.append(e)
    report["phase_b_writes"] = log
    report["phase_b_save"] = save_map()
    report["phase_b_after_reload"] = snapshot("B")


try:
    phase_a()
    a_rows = report.get("phase_a_after_reload") or []
    a_ok = bool(a_rows) and all(r.get("pair_ok") and r.get("VerticalOffset") == "-5000.0" for r in a_rows)
    report["phase_a_result"] = "PERSISTED" if a_ok else "LOST"
    if not a_ok:
        phase_b()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
