import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_outline.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    # 1) material-ish assets
    mats = {"Material": [], "MaterialInstanceConstant": [], "MaterialParameterCollection": [],
            "PostProcessMaterial": [], "Other": []}
    for p in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False):
        data = unreal.EditorAssetLibrary.find_asset_data(p)
        cls = ""
        try:
            cls = str(data.asset_class_path.asset_name)
        except Exception:
            pass
        if cls in mats:
            mats[cls].append(str(p))
    report["materials"] = {k: v for k, v in mats.items() if v}

    # 2) post process volumes / blendables in the level
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ppv = []
    for a in actor_ss.get_all_level_actors():
        cn = a.get_class().get_name()
        if "PostProcess" in cn or "PostProcess" in a.get_actor_label():
            entry = {"label": a.get_actor_label(), "class": cn}
            e, settings = call(a.get_editor_property, "settings")
            if e == "ok" and settings is not None:
                e2, wb = call(settings.get_editor_property, "weighted_blendables")
                if e2 == "ok" and wb is not None:
                    blends = []
                    try:
                        for b in wb:
                            blends.append(str(call(b.get_editor_property, "object")[1])[:90])
                    except Exception as exc:
                        blends.append("iter err " + repr(exc)[:60])
                    entry["blendables"] = blends
                e3, eb = call(settings.get_editor_property, "b_override_blendables")
                entry["b_override_blendables"] = str(eb)
            ppv.append(entry)
    report["post_process_volumes"] = ppv
    report["ppv_count"] = len(ppv)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE OUTLINE done]")
