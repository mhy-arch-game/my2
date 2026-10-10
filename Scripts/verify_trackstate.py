import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_trackstate.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    cls = unreal.InteractableComponent
    report["has_property"] = "b_track_open_state" in dir(cls) or "bTrackOpenState" in dir(cls)
    report["dir_hits"] = sorted([n for n in dir(cls) if "track" in n.lower() or "start_open" in n.lower()])
    cdo = unreal.get_default_object(cls)
    report["cdo_default"] = str(call(cdo.get_editor_property, "bTrackOpenState")[1])
    report["cdo_start_open"] = str(call(cdo.get_editor_property, "bStartOpen")[1])

    variants = [("outer_kw", lambda: unreal.new_object(cls, outer=cdo)),
                ("positional", lambda: unreal.new_object(cls, cdo)),
                ("no_outer", lambda: unreal.new_object(cls))]
    inst = None
    for name, mk in variants:
        try:
            inst = mk()
            report["created_via"] = name
            break
        except Exception as exc:
            report["created_err_" + name] = repr(exc)[:110]
    if inst is None:
        # fall back: reuse the CDO itself (its value is only changed in memory)
        inst = cdo
        report["created_via"] = "CDO (fallback)"

    # --- 旧行为：没有状态位时 SetOpen 直接返回 ---
    call(inst.set_editor_property, "bTrackOpenState", False)
    call(inst.set_open, True, True)
    report["state_with_tracking_OFF"] = bool(inst.is_open())

    # --- 新行为：开状态位后 SetOpen 生效 ---
    call(inst.set_editor_property, "bTrackOpenState", True)
    call(inst.set_open, True, True)
    report["state_with_tracking_ON_open"] = bool(inst.is_open())
    call(inst.set_open, False, True)
    report["state_with_tracking_ON_close"] = bool(inst.is_open())
    report["tick_enabled_after"] = bool(call(inst.is_component_tick_enabled)[1])
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY TRACKSTATE done]")
