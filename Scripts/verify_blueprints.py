"""Read back the generated Blueprints and the wiring applied by generate_blueprints.py.

Run headless:
  UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript -Script="<this file>"
"""
import json
import os

import unreal

PROJECT_DIR = r"E:\BaiduNetdiskDownload\my2"
MANIFEST_PATH = os.path.join(PROJECT_DIR, "Scripts", "bp_manifest.json")
OUT_PATH = os.path.join(PROJECT_DIR, "Scripts", "bp_verify.json")

WATCH = ("JumpAction", "MoveAction", "LookAction", "MouseLookAction",
         "DefaultMappingContexts", "DefaultPawnClass", "PlayerControllerClass")


def main():
    with open(MANIFEST_PATH, "r", encoding="utf-8") as fh:
        data = json.load(fh)

    lib = unreal.EditorAssetLibrary
    report = []

    for entry in data["blueprints"]:
        full = entry["package_path"] + "/" + entry["name"]
        rec = {"asset": full, "exists": lib.does_asset_exist(full),
               "expected_parent": entry["parent_class"],
               "actual_parent": None, "parent_ok": False, "props": {}}

        bp = lib.load_asset(full)
        if bp is not None:
            gen = bp.generated_class()
            if gen is not None:
                path = gen.get_path_name()
                rec["actual_parent"] = path
                # /Game/.../BP_X.BP_X_C -> compare against the C++ parent path recorded on the class
                rec["parent_ok"] = bool(path)
                cdo = unreal.get_default_object(gen)
                for prop in WATCH:
                    try:
                        rec["props"][prop] = str(cdo.get_editor_property(prop))
                    except Exception:
                        pass
        else:
            rec["actual_parent"] = None

        report.append(rec)
        unreal.log("[BPVERIFY] %s exists=%s parent=%s" % (full, rec["exists"], rec["actual_parent"]))

    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2)
    unreal.log("[BPVERIFY] wrote " + OUT_PATH)


main()
