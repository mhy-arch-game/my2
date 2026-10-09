"""Diagnose why pressing E does not open the door. Read-only."""
import json
import os
import traceback

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "diag_result.json")
report = {"sections": {}}


def note(k, v):
    report["sections"][k] = v


def safe(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "ERR:" + repr(exc)


def dump_props(obj, props):
    out = {}
    for p in props:
        out[p] = safe(lambda: str(obj.get_editor_property(p)))
    return out


def bp_cdo(path):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    if bp is None:
        return None, "asset not found"
    gen = bp.generated_class()
    if gen is None:
        return None, "no generated class (blueprint not compiled?)"
    return unreal.get_default_object(gen), None


def bp_components(path, comp_class, props):
    cdo, err = bp_cdo(path)
    if err:
        return {"error": err}
    comps = safe(cdo.get_components_by_class, comp_class)
    if isinstance(comps, str):
        return {"error": comps}
    return {
        "count": len(comps),
        "components": [{"name": c.get_name(), "props": dump_props(c, props)} for c in comps],
    }


# 1) which GameMode / Pawn / Controller is configured
for gm_path in ("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode",
                "/Game/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMEGameMode"):
    cdo, err = bp_cdo(gm_path)
    note("gamemode:" + gm_path, {"error": err} if err else dump_props(
        cdo, ["DefaultPawnClass", "PlayerControllerClass", "HUDClass", "GameStateClass"]))

# 2) is IA_Interact actually mapped to E in IMC_Interaction?
imc = unreal.EditorAssetLibrary.load_asset("/Game/Input/IMC_Interaction")
if imc is None:
    note("IMC_Interaction", {"error": "asset missing"})
else:
    mappings = safe(lambda: list(imc.get_editor_property("mappings")))
    if isinstance(mappings, str):
        note("IMC_Interaction", {"error": mappings})
    else:
        rows = []
        for m in mappings:
            rows.append({
                "action": safe(lambda: str(m.get_editor_property("action"))),
                "key": safe(lambda: str(m.get_editor_property("key"))),
                "key_name": safe(lambda: str(m.get_editor_property("key").get_editor_property("key_name"))),
                "triggers": safe(lambda: str(m.get_editor_property("triggers"))),
                "modifiers": safe(lambda: str(m.get_editor_property("modifiers"))),
            })
        note("IMC_Interaction", {"mappings": rows})

# 3) player controller(s): which mapping contexts are registered
for pc_path in ("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController",
                "/Game/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMEPlayerController"):
    cdo, err = bp_cdo(pc_path)
    note("controller:" + pc_path, {"error": err} if err else dump_props(
        cdo, ["DefaultMappingContexts", "MobileExcludedMappingContexts", "bForceTouchControls"]))

# 4) the player character's detector
note("detector", bp_components(
    "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter",
    unreal.InteractionDetectorComponent,
    ["InteractAction", "PickMode", "InteractionRadius", "TraceDistance", "bRequireFacing",
     "MinFacingCosine", "UpdateInterval", "ProbeObjectTypes", "bDrawDebug",
     "bApplyFocusOutline", "bUseStencilOutline"]))

# 5) the door's interactable
note("door", bp_components(
    "/Game/bclass_source/active_door",
    unreal.InteractableComponent,
    ["bEnabled", "InteractionPrompt", "bUseBuiltInToggle", "ToggleComponents",
     "ToggleComponentNames", "OpenRelativeTransform", "ClosedRelativeTransform",
     "ToggleDuration", "bStartOpen", "bDisableCollisionWhenOpen",
     "bToggleLights", "LightComponents", "LightComponentNames"]))

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[DIAG] written " + OUT)
