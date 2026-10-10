"""Evidence for the old-content review: referencers of each candidate asset."""
import json, os, traceback
import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "structure_audit.json")
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:130], None)


def main():
    folders = ["/Game/MHY_ARCH_GAME/Blueprints", "/Game/bclass_source", "/Game/LevelPrototyping",
               "/Game/FirstPerson/Blueprints", "/Game/Input"]
    listing = {}
    candidates = []
    for folder in folders:
        err, assets = call(lib.list_assets, folder, recursive=True, include_folder=False)
        names = [str(a) for a in assets] if err == "ok" else []
        listing[folder] = [n.split(".")[-1] for n in names]
        candidates.extend(n for n in names if not n.endswith("_C"))

    refs = {}
    for path in candidates:
        err, r = call(lib.find_package_referencers_for_asset, path, False)
        refs[path] = [str(x) for x in r] if err == "ok" else err
    report["listing"] = listing
    report["referencers"] = refs


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc); report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
