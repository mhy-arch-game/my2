"""Create the interaction Input Action (and an optional mapping context).

The migrated StructureInteraction docs reference an "IA_Interact" that was never
migrated. This creates it so the detector's InteractAction can point at it.

Run headless:
  UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript -Script="<this file>" -nullrhi -DDC-ForceMemoryCache
"""
import json
import os

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "input_setup_result.json")
PACKAGE = "/Game/Input/Actions"

result = {"created": [], "skipped": [], "failed": [], "notes": []}

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def try_factory(names):
    for name in names:
        cls = getattr(unreal, name, None)
        if cls is None:
            continue
        try:
            return cls(), name
        except Exception as exc:
            result["notes"].append("factory %s ctor failed: %r" % (name, exc))
    return None, None


def create_asset(name, package, asset_class, factory_names):
    full = package + "/" + name
    if lib.does_asset_exist(full):
        result["skipped"].append(full)
        return None

    factory, used = try_factory(factory_names)
    try:
        asset = tools.create_asset(name, package, asset_class, factory)
    except Exception as exc:
        result["failed"].append({"asset": full, "error": repr(exc), "factory": used})
        return None

    if asset is None:
        result["failed"].append({"asset": full, "error": "create_asset returned None", "factory": used})
        return None

    lib.save_loaded_asset(asset)
    result["created"].append(full)
    result["notes"].append("%s created with factory=%s" % (full, used))
    return asset


ia = create_asset("IA_Interact", PACKAGE, unreal.InputAction,
                  ["InputActionFactory", "AssetFactory"])

# Make it a simple digital (button) action.
if ia is not None:
    try:
        ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        lib.save_loaded_asset(ia)
        result["notes"].append("IA_Interact value_type = BOOLEAN")
    except Exception as exc:
        result["notes"].append("could not set value_type: %r" % exc)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)

unreal.log("[INPUTSETUP] created=%s skipped=%s failed=%s" % (
    result["created"], result["skipped"], result["failed"]))


def _names():
    return [n for n in dir(unreal) if "Factory" in n and ("Input" in n or "Action" in n)]
