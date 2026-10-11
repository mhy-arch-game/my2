import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "create_uicue_asset.json")
PATH = "/Game/MHY_ARCH_GAME/UI"
NAME = "DA_UiCueSet"
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    report["classes"] = {
        "subsystem": hasattr(unreal, "UiCueSubsystem"),
        "trigger_volume": hasattr(unreal, "UiCueTriggerVolume"),
        "subtitle_widget": hasattr(unreal, "UiCueSubtitleWidget"),
        "presenter_interface": hasattr(unreal, "UiCuePresenter"),
        "cue_set": hasattr(unreal, "UiCueSet"),
        "segment_struct": hasattr(unreal, "UiCueSegment"),
    }

    existing = unreal.EditorAssetLibrary.does_asset_exist(PATH + "/" + NAME)
    report["existed"] = existing
    if not existing:
        factory = unreal.DataAssetFactory()
        call(factory.set_editor_property, "data_asset_class", unreal.UiCueSet)
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        da = call(tools.create_asset, NAME, PATH, unreal.UiCueSet, factory)
        report["created"] = str(da[1])[:80] if da[0] == "ok" else da

    da = unreal.EditorAssetLibrary.load_asset(PATH + "/" + NAME)
    report["loaded_class"] = da.get_class().get_name() if da else "MISSING"
    if da:
        report["defaults"] = {
            "QueueGapSeconds": str(call(da.get_editor_property, "QueueGapSeconds")[1]),
            "DefaultSegmentSeconds": str(call(da.get_editor_property, "DefaultSegmentSeconds")[1]),
            "SegmentDelimiters": str(call(da.get_editor_property, "SegmentDelimiters")[1])[:40],
            "bUseBuiltInSubtitle": str(call(da.get_editor_property, "bUseBuiltInSubtitle")[1]),
            "SubtitleBottomOffset": str(call(da.get_editor_property, "SubtitleBottomOffset")[1]),
            "Cues": str(call(da.get_editor_property, "Cues")[1])[:40],
        }
    # 体积 Actor 是否可放置
    v = unreal.load_class(None, "/Script/MHY_ARCH_GAME.UiCueTriggerVolume")
    report["volume_class_loaded"] = str(v)
    report["save"] = call(unreal.EditorAssetLibrary.save_loaded_asset, da)[0] if da else "n/a"
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[CREATE UICUE ASSET done]")
