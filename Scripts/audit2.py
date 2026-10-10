"""Reference audit, with errors preserved (positional args to avoid kwarg-name issues)."""
import json, os, traceback
import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "audit2.json")
report = {"listing": {}, "listing_err": {}, "referencers": {}}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:180], None)


def main():
    folders = ["/Game/MHY_ARCH_GAME/Blueprints", "/Game/bclass_source", "/Game/LevelPrototyping",
               "/Game/FirstPerson/Blueprints", "/Game/Input", "/Game/MHY_ARCH_GAME"]
    candidates = []
    for folder in folders:
        err, assets = call(lib.list_assets, folder, True, False)
        if err != "ok":
            report["listing_err"][folder] = err
            continue
        names = [str(x) for x in (assets or [])]
        report["listing"][folder] = [n.split(".")[-1] for n in names]
        candidates.extend(n for n in names if not n.endswith("_C"))

    report["candidate_count"] = len(candidates)
    for path in candidates:
        err, r = call(lib.find_package_referencers_for_asset, path, False)
        report["referencers"][path] = [str(x) for x in r] if err == "ok" else ("ERR " + err)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc); report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
