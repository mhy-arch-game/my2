"""Fresh-process verification of the swift_actor replacement + pairing."""
import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
BP_SWIFT = "/Game/bclass_source/modern_swift_actor"
OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "verify_swift.json")
PREFIX = "kongjianchuansuoqi"
report = {}
lib = unreal.EditorAssetLibrary


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    bp = lib.load_asset(BP_SWIFT)
    swift_class = bp.generated_class() if bp else None
    near_miss = []   # any actor still named kongjian* that is NOT the blueprint

    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {}
    for a in actor_ss.get_all_level_actors():
        try:
            by_label[a.get_actor_label()] = a
        except Exception:
            pass

    rows = []
    for n in range(1, 11):
        pref = PREFIX + str(n)
        a = by_label.get(pref)
        if a is None:
            rows.append({"num": n, "error": "missing"})
            continue
        r = {"num": n, "class": a.get_class().get_name(),
             "is_swift_class": (swift_class is not None and a.get_class() == swift_class)}
        era = a.get_component_by_class(unreal.TimeEraComponent)
        portal = a.get_component_by_class(unreal.TimeEraPortalComponent)
        inter = a.get_component_by_class(unreal.InteractableComponent)
        r["has_TimeEra"] = era is not None
        r["has_TimeEraPortal"] = portal is not None
        r["has_Interactable"] = inter is not None
        if era is not None:
            e, v = call(era.get_editor_property, "Era")
            r["Era"] = str(v) if e == "ok" else e
        if portal is not None:
            for p in ("TargetMode", "InteractionPrompt", "VerticalOffset"):
                e, v = call(portal.get_editor_property, p)
                r[p] = str(v) if e == "ok" else e
            e, cp = call(portal.get_editor_property, "CounterpartActor")
            if e == "ok" and cp is not None:
                cpnum = None
                try:
                    cpl = cp.get_actor_label()
                    if cpl.startswith(PREFIX) and cpl[len(PREFIX):].isdigit():
                        cpnum = int(cpl[len(PREFIX):])
                    r["counterpart_label"] = cpl
                except Exception as exc:
                    r["counterpart_error"] = repr(exc)[:90]
                r["counterpart_num"] = cpnum
                r["pair_ok"] = (cpnum == (n + 1 if n % 2 == 1 else n - 1))
            else:
                r["counterpart_num"] = None
                r["pair_ok"] = False
        rows.append(r)

    # leftovers still using the old mesh-actor form
    for lbl, a in by_label.items():
        if lbl.startswith(PREFIX) and swift_class is not None and a.get_class() != swift_class:
            near_miss.append([lbl, a.get_class().get_name()])

    report["rows"] = rows
    report["leftover_non_swift"] = near_miss
    report["swift_instances"] = sum(1 for a in actor_ss.get_all_level_actors()
                                    if swift_class is not None and a.get_class() == swift_class)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
