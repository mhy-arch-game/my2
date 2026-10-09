"""Batch-generate Blueprint subclasses for the migrated MHY_ARCH_GAME module.

Run headless:
  UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript -Script="<this file>"

Driven by Scripts/bp_manifest.json; writes Scripts/bp_result.json for machine check.
Idempotent: assets that already exist are skipped.
"""
import json
import os
import traceback

import unreal

PROJECT_DIR = r"E:\BaiduNetdiskDownload\my2"
MANIFEST_PATH = os.path.join(PROJECT_DIR, "Scripts", "bp_manifest.json")
RESULT_PATH = os.path.join(PROJECT_DIR, "Scripts", "bp_result.json")


def log(msg):
    unreal.log("[BPGEN] " + str(msg))


def log_err(msg):
    unreal.log_error("[BPGEN] " + str(msg))


def resolve_class(path):
    """Resolve a /Script/Module.Class path to a UClass, tolerant of API differences."""
    for fn_name in ("load_class", "load_object"):
        fn = getattr(unreal, fn_name, None)
        if fn is None:
            continue
        try:
            cls = fn(None, path)
            if cls is not None:
                return cls
        except Exception:
            pass
    return None


def resolve_value(editor_asset_lib, value):
    """Resolve a wiring value. A "bp:" prefix means a Blueprint's generated class
    (needed for TSubclassOf properties such as DefaultPawnClass)."""
    if isinstance(value, str) and value.startswith("bp:"):
        bp = editor_asset_lib.load_asset(value[3:])
        if bp is None:
            return None
        return bp.generated_class()
    return editor_asset_lib.load_asset(value)


def main():
    result = {"created": [], "skipped": [], "failed": [], "wired": [], "wire_failed": []}

    with open(MANIFEST_PATH, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor_asset_lib = unreal.EditorAssetLibrary

    log("manifest: %d blueprints" % len(manifest.get("blueprints", [])))

    for entry in manifest.get("blueprints", []):
        name = entry["name"]
        pkg = entry["package_path"]
        parent_path = entry["parent_class"]
        kind = entry.get("kind", "blueprint")
        full = pkg + "/" + name

        try:
            if editor_asset_lib.does_asset_exist(full):
                result["skipped"].append(full)
                log("skip (exists) " + full)
                continue

            parent = resolve_class(parent_path)
            if parent is None:
                result["failed"].append({"asset": full, "error": "parent class unresolved: " + parent_path})
                log_err("parent unresolved for " + full)
                continue

            if kind == "widget":
                factory = unreal.WidgetBlueprintFactory()
                asset_class = unreal.WidgetBlueprint
            else:
                factory = unreal.BlueprintFactory()
                asset_class = unreal.Blueprint

            factory.set_editor_property("parent_class", parent)
            asset = tools.create_asset(asset_name=name, package_path=pkg,
                                       asset_class=asset_class, factory=factory)
            if asset is None:
                result["failed"].append({"asset": full, "error": "create_asset returned None"})
                log_err("create_asset returned None for " + full)
                continue

            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            except Exception:
                pass

            editor_asset_lib.save_loaded_asset(asset)
            result["created"].append(full)
            log("created " + full)
        except Exception as exc:
            result["failed"].append({"asset": full, "error": repr(exc), "trace": traceback.format_exc()})
            log_err("failed %s: %r" % (full, exc))

    for wiring in manifest.get("wiring", []):
        bp_path = wiring["blueprint"]
        try:
            bp = editor_asset_lib.load_asset(bp_path)
            if bp is None:
                result["wire_failed"].append({"blueprint": bp_path, "error": "blueprint not found"})
                continue

            gen_class = bp.generated_class()
            if gen_class is None:
                try:
                    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                    gen_class = bp.generated_class()
                except Exception:
                    pass
            if gen_class is None:
                result["wire_failed"].append({"blueprint": bp_path, "error": "no generated class"})
                continue

            cdo = unreal.get_default_object(gen_class)
            for prop, value in wiring["properties"].items():
                if isinstance(value, list):
                    resolved = [resolve_value(editor_asset_lib, p) for p in value]
                    missing = [p for p, o in zip(value, resolved) if o is None]
                    if missing:
                        result["wire_failed"].append({"blueprint": bp_path, "prop": prop, "error": "missing: " + ",".join(missing)})
                        continue
                    cdo.set_editor_property(prop, resolved)
                else:
                    obj = resolve_value(editor_asset_lib, value)
                    if obj is None:
                        result["wire_failed"].append({"blueprint": bp_path, "prop": prop, "error": "missing: " + value})
                        continue
                    cdo.set_editor_property(prop, obj)
                log("wired %s.%s" % (bp_path, prop))

            editor_asset_lib.save_loaded_asset(bp)
            result["wired"].append(bp_path)
        except Exception as exc:
            result["wire_failed"].append({"blueprint": bp_path, "error": repr(exc), "trace": traceback.format_exc()})
            log_err("wiring failed %s: %r" % (bp_path, exc))

    with open(RESULT_PATH, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)

    log("DONE created=%d skipped=%d failed=%d wired=%d wire_failed=%d" % (
        len(result["created"]), len(result["skipped"]), len(result["failed"]),
        len(result["wired"]), len(result["wire_failed"])))


main()
