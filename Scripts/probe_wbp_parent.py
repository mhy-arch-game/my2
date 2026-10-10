import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_wbp_parent.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    data = unreal.EditorAssetLibrary.find_asset_data("/Game/MHY_ARCH_GAME/UI/WBP_InteractionPrompt")
    report["asset_class"] = str(call(data.get_editor_property, "asset_class_path")[1])[:120]
    tags = {}
    try:
        names = list(data.get_tag_value("ParentClass")) if False else None
    except Exception:
        pass
    for tag in ("ParentClass", "NativeParentClass", "GeneratedClass", "BlueprintType"):
        e, v = call(data.get_tag_value, tag)
        if e == "ok":
            tags[tag] = str(v)
    report["tags_subset"] = tags
    e, allt = call(data.get_editor_property, "tags_and_values")
    if e == "ok" and allt:
        report["all_tags"] = {str(k): str(v) for k, v in allt.items()}
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE WBP PARENT done]")
