import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "audit_referencers.json")
report = {}
PKGS = ["/Game/models/tripo_convert_a27cf087-650e-439c-80cf-d55ede9a4be3",
        "/Game/models/毛绒熊3d模型",
        "/Game/models/毛绒熊3d模型_basecolor",
        "/Game/models/毛绒熊3d模型_normal",
        "/Game/models/fireplace",
        "/Game/models/fireplace_basecolor",
        "/Game/models/b9acd5b7_b130_41b9_b7e3_14ed88825340",
        "/Game/bclass_source/ancient_swift_actor"]


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for p in PKGS:
        e, refs = call(ar.get_referencers, p, unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                                                    include_hard_package_references=True,
                                                                                    include_searchable_names=True))
        report[p] = {"exists": unreal.EditorAssetLibrary.does_asset_exist(p),
                     "referencers": [str(x) for x in refs] if (e == "ok" and refs is not None) else str(e)}
    seqs = [str(a) for a in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
            if "LevelSequence" in str(a) or "Sequence" in str(a)]
    report["level_sequences"] = seqs[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[AUDIT REFERENCERS done]")
