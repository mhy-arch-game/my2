"""Create IMC_Interaction (IA_Interact -> E) and add it to the player controller.

Removes the manual step of wiring the interact key.
Run: UnrealEditor-Cmd.exe <project>.uproject -run=PythonScript -Script="<this file>" -nullrhi -DDC-ForceMemoryCache
"""
import json
import os

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "imc_setup_result.json")
IA_PATH = "/Game/Input/Actions/IA_Interact"
IMC_PATH = "/Game/Input/IMC_Interaction"
PC_PATH = "/Game/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMEPlayerController"

result = {"created": [], "skipped": [], "failed": [], "notes": []}
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary

ia = lib.load_asset(IA_PATH)
if ia is None:
    result["failed"].append("IA_Interact not found")
else:
    imc = lib.load_asset(IMC_PATH)
    if imc is None:
        try:
            imc = tools.create_asset("IMC_Interaction", "/Game/Input", unreal.InputMappingContext, None)
        except Exception as exc:
            result["failed"].append("create IMC: %r" % exc)
            imc = None
        if imc is not None:
            result["created"].append(IMC_PATH)
    else:
        result["skipped"].append(IMC_PATH)

    if imc is not None:
        try:
            existing = list(imc.get_editor_property("mappings"))
            already = False
            for m in existing:
                try:
                    if m.get_editor_property("action") == ia:
                        already = True
                        break
                except Exception:
                    pass

            if already:
                result["notes"].append("mapping already present")
            else:
                mapping = unreal.EnhancedActionKeyMapping()
                mapping.set_editor_property("action", ia)
                key = unreal.Key()
                key.set_editor_property("key_name", "E")
                mapping.set_editor_property("key", key)
                existing.append(mapping)
                imc.set_editor_property("mappings", existing)
                result["notes"].append("added IA_Interact -> E")
            lib.save_loaded_asset(imc)
        except Exception as exc:
            result["failed"].append("mapping: %r" % exc)

        # register the IMC on the player controller so the key is actually active
        pc = lib.load_asset(PC_PATH)
        if pc is None:
            result["failed"].append("player controller not found")
        else:
            try:
                gen = pc.generated_class()
                cdo = unreal.get_default_object(gen)
                ctxs = list(cdo.get_editor_property("DefaultMappingContexts"))
                if imc in ctxs:
                    result["notes"].append("IMC already on controller")
                else:
                    ctxs.append(imc)
                    cdo.set_editor_property("DefaultMappingContexts", ctxs)
                    lib.save_loaded_asset(pc)
                    result["notes"].append("IMC added to DefaultMappingContexts")
            except Exception as exc:
                result["failed"].append("controller wiring: %r" % exc)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)

unreal.log("[IMCSETUP] created=%s skipped=%s failed=%s notes=%s" % (
    result["created"], result["skipped"], result["failed"], result["notes"]))
