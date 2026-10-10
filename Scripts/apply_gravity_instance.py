import json, os, traceback, datetime
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "apply_gravity_instance.json")
UE_MAP = "E:/BaiduNetdiskDownload/my2/Content/FirstPerson/firstvision.umap"
H = 1125.0
K = 0.32
report = {"target_height": H, "new_scale": K, "method": "plain set_editor_property + save_current_level"}
lib = unreal.EditorAssetLibrary


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:170], None)


def mtime():
    try:
        return str(datetime.datetime.fromtimestamp(os.path.getmtime(UE_MAP)))
    except Exception:
        return "n/a"


try:
    report["mtime_before"] = mtime()
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    hit = None
    for a in actor_ss.get_all_level_actors():
        try:
            if a.get_actor_label() == "jumping_area":
                hit = a
                break
        except Exception:
            pass
    if hit is None:
        report["error"] = "jumping_area actor not found"
    else:
        comp = hit.get_component_by_class(unreal.GravityZoneComponent)
        report["actor_class"] = hit.get_class().get_name()
        report["comp"] = comp.get_name() if comp else None
        if comp is None:
            report["error"] = "no GravityZoneComponent"
        else:
            report["before"] = str(comp.get_editor_property("GravityScaleInside"))
            report["before_h"] = str(comp.get_editor_property("TargetJumpHeight"))
            # exactly the pattern proven by apply_door_hinge.py: no notify_mode
            e1 = call(comp.set_editor_property, "GravityScaleInside", K)[0]
            e2 = call(comp.set_editor_property, "TargetJumpHeight", H)[0]
            report["set_scale"] = e1
            report["set_height"] = e2
            report["after_in_process"] = str(comp.get_editor_property("GravityScaleInside"))
            report["after_h_in_process"] = str(comp.get_editor_property("TargetJumpHeight"))
            report["save_current_level"] = call(unreal.EditorLoadingAndSavingUtils.save_current_level)[0]
        # fallback: also try saving the actor's own (external actor) package
        try:
            pkg = hit.get_outer().get_name()
            report["actor_package"] = pkg
            report["save_actor_pkg"] = str(unreal.EditorAssetLibrary.save_asset(pkg))
        except Exception as exc:
            report["save_actor_pkg_err"] = repr(exc)[:140]
    report["mtime_after"] = mtime()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[APPLY-INSTANCE saved]")
