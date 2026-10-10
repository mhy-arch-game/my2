"""Replace kongjianchuansuoqi* with modern_swift_actor and wire odd-even era pairing.

Pairing rule (as requested): 1-2, 3-4, 5-6, 7-8, 9-10  (each pair differs only in Z).
Odd  member -> Era = Ancient, prompt "传送至现代"
Even member -> Era = Modern , prompt "传送至过去"
Both point CounterpartActor at their partner.
"""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_SWIFT = "/Game/bclass_source/modern_swift_actor"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "swift_replace_result.json")
PREFIX = "kongjianchuansuoqi"
VERTICAL_OFFSET = -5000.0
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:170], None)


def setp(obj, prop, value):
    return call(obj.set_editor_property, prop, value,
                unreal.PropertyAccessChangeNotifyMode.ALWAYS)[0]


def main():
    bp = lib.load_asset(BP_SWIFT)
    if bp is None:
        report["error"] = "modern_swift_actor not found"
        return
    swift_class = bp.generated_class()
    if swift_class is None:
        report["error"] = "modern_swift_actor has no generated class"
        return

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    sources = {}
    for a in actor_ss.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
        except Exception:
            continue
        if not lbl.startswith(PREFIX):
            continue
        tail = lbl[len(PREFIX):]
        if tail.isdigit():
            sources[int(tail)] = a
    report["found"] = sorted(sources.keys())
    if not sources:
        report["error"] = "no " + PREFIX + "* actors found"
        return

    # ---- pass 1: spawn the blueprint instances -------------------------
    spawned = {}
    made = []
    for num in sorted(sources.keys()):
        src = sources[num]
        e = {"label": src.get_actor_label(), "class_before": src.get_class().get_name()}
        try:
            if src.get_class() == swift_class:
                spawned[num] = src
                e["note"] = "已经是 modern_swift_actor，跳过"
                made.append(e)
                continue
            xform = src.get_actor_transform()
            new_actor = actor_ss.spawn_actor_from_class(swift_class, src.get_actor_location(),
                                                        src.get_actor_rotation())
            new_actor.set_actor_transform(xform, False, False)
            new_actor.set_actor_label(e["label"])
            actor_ss.destroy_actor(src)
            spawned[num] = new_actor
            e["class_after"] = new_actor.get_class().get_name()
        except Exception as exc:
            e["error"] = repr(exc)[:150]
        made.append(e)

    # ---- pass 2: fill in the components -------------------------------
    filled = []
    for num in sorted(spawned.keys()):
        actor = spawned[num]
        f = {"num": num, "label": actor.get_actor_label()}
        partner_num = num + 1 if num % 2 == 1 else num - 1
        partner = spawned.get(partner_num)
        is_odd = (num % 2 == 1)
        f["pair_with"] = partner_num if partner else None
        f["era"] = "Ancient" if is_odd else "Modern"
        f["prompt"] = "传送至现代" if is_odd else "传送至过去"

        era_comp = actor.get_component_by_class(unreal.TimeEraComponent)
        portal = actor.get_component_by_class(unreal.TimeEraPortalComponent)
        inter = actor.get_component_by_class(unreal.InteractableComponent)

        if era_comp is not None:
            f["set_era"] = setp(era_comp, "Era", unreal.TimeEra.ANCIENT if is_odd else unreal.TimeEra.MODERN)
        else:
            f["set_era"] = "no TimeEraComponent"

        if portal is not None:
            r = {}
            r["TargetMode"] = setp(portal, "TargetMode", unreal.TimeEraPortalTargetMode.COUNTERPART_OBJECT)
            r["VerticalOffset"] = setp(portal, "VerticalOffset", VERTICAL_OFFSET)
            r["InteractionPrompt"] = setp(portal, "InteractionPrompt", f["prompt"])
            r["bAutoUseInteractableOnOwner"] = setp(portal, "bAutoUseInteractableOnOwner", True)
            if partner is not None:
                r["CounterpartActor"] = setp(portal, "CounterpartActor", partner)
            else:
                r["CounterpartActor"] = "no partner found"
            f["portal"] = r
        else:
            f["portal"] = "no TimeEraPortalComponent"

        if inter is not None:
            f["set_interactable_prompt"] = setp(inter, "InteractionPrompt", f["prompt"])

        # read back
        rb = {}
        if era_comp is not None:
            rb["Era"] = str(era_comp.get_editor_property("Era"))
        if portal is not None:
            for p in ("TargetMode", "CounterpartActor", "InteractionPrompt", "VerticalOffset"):
                e, v = call(portal.get_editor_property, p)
                rb[p] = str(v) if e == "ok" else e
        f["readback"] = rb
        filled.append(f)
    report["spawned"] = made
    report["filled"] = filled

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    err, _ = call(unreal.EditorLoadingAndSavingUtils.save_map, world, MAP)
    report["save"] = err


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
