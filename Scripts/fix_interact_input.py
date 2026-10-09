"""Fix: the running controller (BP_FirstPersonPlayerController, native parent
Engine.PlayerController) only registers IMC_Default + IMC_MouseLook. IMC_Interaction
was registered on a controller that is never used, so E was never mapped.

Add IA_Interact -> E into IMC_Default, which IS registered. Idempotent.
"""
import json
import os

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "fix_input_result.json")
result = {"notes": [], "failed": []}

IA_PATH = "/Game/Input/Actions/IA_Interact"
IMC_PATH = "/Game/Input/IMC_Default"

lib = unreal.EditorAssetLibrary

ia = lib.load_asset(IA_PATH)
imc = lib.load_asset(IMC_PATH)

if ia is None:
    result["failed"].append("IA_Interact missing")
elif imc is None:
    result["failed"].append("IMC_Default missing")
else:
    try:
        mappings = list(imc.get_editor_property("mappings"))

        already = False
        for m in mappings:
            if m.get_editor_property("action") == ia:
                already = True
                break

        if already:
            result["notes"].append("IA_Interact already mapped in IMC_Default")
        else:
            mapping = unreal.EnhancedActionKeyMapping()
            mapping.set_editor_property("action", ia)
            key = unreal.Key()
            key.set_editor_property("key_name", "E")
            mapping.set_editor_property("key", key)
            mappings.append(mapping)
            imc.set_editor_property("mappings", mappings)
            lib.save_loaded_asset(imc)
            result["notes"].append("added IA_Interact -> E to IMC_Default")

        # read back
        rows = []
        for m in list(imc.get_editor_property("mappings")):
            try:
                key = m.get_editor_property("key")
                rows.append({
                    "action": str(m.get_editor_property("action")),
                    "key_name": str(key.get_editor_property("key_name")),
                })
            except Exception as exc:
                rows.append({"error": repr(exc)})
        result["IMC_Default_mappings"] = rows
    except Exception as exc:
        result["failed"].append("mapping: %r" % exc)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2, ensure_ascii=False)
unreal.log("[FIXINPUT] done: " + str(result.get("notes")) + " failed=" + str(result.get("failed")))
