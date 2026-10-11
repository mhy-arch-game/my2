import json, os, traceback
import unreal
SRC = "E:/BaiduNetdiskDownload/my2/Content/background.MP3"
DEST = "/Game/sound_resources"
INI = "E:/BaiduNetdiskDownload/my2/Config/DefaultGame.ini"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "setup_bgmusic.json")
report = {"src": SRC, "dest": DEST}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    task = unreal.AssetImportTask()
    for prop, val in (("filename", SRC), ("destination_path", DEST), ("automated", True),
                      ("save", True), ("replace_existing", True)):
        call(task.set_editor_property, prop, val)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    e, paths = call(task.get_editor_property, "imported_object_paths")
    imported = [str(p) for p in paths] if (e == "ok" and paths) else []
    report["imported"] = imported

    asset_path = imported[0].split(".")[0] if imported else None
    report["asset_path"] = asset_path
    if asset_path:
        sw = unreal.EditorAssetLibrary.load_asset(asset_path)
        report["class"] = sw.get_class().get_name() if sw else "MISSING"
        if sw is not None:
            report["duration_before"] = str(call(sw.get_editor_property, "Duration")[1])
            report["bLooping_before"] = str(call(sw.get_editor_property, "bLooping")[1])
            call(sw.modify)
            report["set_looping"] = call(sw.set_editor_property, "bLooping", True,
                                         notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]
            report["bLooping_after"] = str(call(sw.get_editor_property, "bLooping")[1])
            report["save_asset"] = call(unreal.EditorAssetLibrary.save_loaded_asset, sw)[0]

    # ---- 把默认值写进 Config/DefaultGame.ini（随工程提交）----
    section = "[/Script/MHY_ARCH_GAME.BackgroundMusicSettings]"
    text = ""
    if os.path.exists(INI):
        with open(INI, "r", encoding="utf-8") as fh:
            text = fh.read()
    if section in text:
        report["ini"] = "已存在该段，未改动"
    else:
        lines = ["", section]
        if asset_path:
            lines.append("Music=" + imported[0] if imported else "")
        lines += ["Volume=1.000000", "Speed=1.000000", "bAutoPlay=True", "bLoop=True"]
        with open(INI, "a", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
        report["ini"] = "已追加设置段"
    with open(INI, "r", encoding="utf-8") as fh:
        report["ini_tail"] = fh.read()[-400:]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[SETUP BGMUSIC done]")
