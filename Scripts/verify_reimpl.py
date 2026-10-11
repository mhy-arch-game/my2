"""Compile the end_of_the_game Blueprint and list the level instance components."""
import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_reimpl.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:110]
def v3(v):
    try: return [round(float(v.x),1), round(float(v.y),1), round(float(v.z),1)]
    except Exception: return str(v)[:40]
try:
    BP_PATH = "/Game/bclass_source/end_of_the_game"
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    rep["bp"] = str(call(bp.get_name)) if bp else None
    r = call(unreal.BlueprintEditorLibrary.compile_blueprint, bp)
    rep["compile"] = str(r)[:60]
    gen = call(bp.generated_class) if bp else None
    rep["generated"] = str(gen)[:110]
    ad = call(unreal.EditorAssetLibrary.find_asset_data, BP_PATH)
    if not isinstance(ad, str):
        rep["parent_tag"] = str(call(ad.get_tag_value, "ParentClass"))[:110]

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        if "end_of_the_game" in cn.lower() or "endofthegame" in cn.lower():
            info = {"class": cn, "label": str(call(a.get_actor_label)), "loc": v3(call(a.get_actor_location)), "comps": []}
            for c in (call(a.get_components_by_class, unreal.ActorComponent) or []):
                ci = {"name": str(call(c.get_name)), "class": str(call(c.get_class().get_name))}
                if "InteractableComponent" in ci["class"]:
                    ci["prompt"] = str(call(c.get_editor_property, "InteractionPrompt"))
                    ci["enabled"] = str(call(c.get_editor_property, "bEnabled"))
                info["comps"].append(ci)
            rep.setdefault("instances", []).append(info)
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-500:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("VERIFY REIMPL done")