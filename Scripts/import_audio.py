import json, os, traceback
import unreal
SRC_DIR = "E:/BaiduNetdiskDownload/my2/Content/sound_resources"
DEST = "/Game/sound_resources"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "import_audio.json")
report = {"dest": DEST}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:140], None)


try:
    files = sorted(f for f in os.listdir(SRC_DIR) if f.lower().endswith((".mp3", ".wav", ".ogg", ".flac")))
    report["found"] = files
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        meta = {}
        for prop, val in (("filename", os.path.join(SRC_DIR, f)),
                          ("destination_path", DEST),
                          ("automated", True),
                          ("save", True),
                          ("replace_existing", True)):
            e, _ = call(t.set_editor_property, prop, val)
            meta[prop] = e
        tasks.append((f, t, meta))
    report["task_props"] = tasks[0][2] if tasks else {}
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, t, _ in tasks])
    for f, t, _ in tasks:
        paths = []
        e, v = call(t.get_editor_property, "imported_object_paths")
        if e == "ok" and v:
            paths = [str(p) for p in v]
        report.setdefault("tasks", []).append({"file": f, "imported": paths})

    rows = []
    for p in unreal.EditorAssetLibrary.list_assets(DEST, recursive=False, include_folder=False):
        a = unreal.EditorAssetLibrary.load_asset(p)
        cls = a.get_class().get_name() if a else "?"
        row = {"path": str(p), "class": cls}
        if cls == "SoundWave":
            for prop in ("Duration", "bLooping", "SoundClassObject"):
                e, v = call(a.get_editor_property, prop)
                row[prop] = str(v) if e == "ok" else str(e)
        rows.append(row)
    report["assets"] = rows
    report["asset_count"] = len(rows)
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[IMPORT AUDIO done]")
