"""Verify the new interaction-operation classes are reflected."""
import json
import os
import traceback

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "oplink_probe.json")
report = {}


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:130], None)


def main():
    for name in ("InteractionLinkComponent", "InteractionOperationReceiverComponent",
                 "InteractionOperationReceiver", "ProximityBarrierComponent"):
        report["has_" + name] = hasattr(unreal, name)

    # enum values
    act = getattr(unreal, "InteractionOperationAction", None)
    report["action_values"] = [v for v in dir(act) if v.isupper()] if act else None

    # link component defaults
    cls = getattr(unreal, "InteractionLinkComponent", None)
    if cls:
        err, obj = call(unreal.new_object, cls)
        if err == "ok" and obj:
            e = {}
            for p in ("Entries", "bAutoBindInteractable"):
                err2, v = call(obj.get_editor_property, p)
                e[p] = str(v) if err2 == "ok" else err2
            report["link_defaults"] = e
            report["link_fns"] = [n for n in dir(obj) if "dispatch" in n.lower() or "resolve" in n.lower()]

    rcls = getattr(unreal, "InteractionOperationReceiverComponent", None)
    if rcls:
        err, obj = call(unreal.new_object, rcls)
        if err == "ok" and obj:
            e = {}
            for p in ("Channel", "Bindings", "bEnabled"):
                err2, v = call(obj.get_editor_property, p)
                e[p] = str(v) if err2 == "ok" else err2
            report["receiver_defaults"] = e

    # struct fields
    report["operation_fields"] = [p for p in dir(unreal.InteractionOperation) if not p.startswith("_")]
    report["binding_fields"] = [p for p in dir(unreal.InteractionOperationBinding) if not p.startswith("_")]

    # barrier debug helpers now that it compiled
    bcls = getattr(unreal, "ProximityBarrierComponent", None)
    if bcls:
        err, obj = call(unreal.new_object, bcls)
        if err == "ok" and obj:
            e = {}
            for p in ("bDrawOnScreenDebug", "bDrawDebug"):
                err2, v = call(obj.get_editor_property, p)
                e[p] = str(v) if err2 == "ok" else err2
            err3, s = call(obj.get_barrier_debug_string)
            e["get_barrier_debug_string"] = str(s) if err3 == "ok" else err3
            report["barrier"] = e


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
