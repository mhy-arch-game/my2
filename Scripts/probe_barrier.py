"""Verify the new component is reflected and read its defaults."""
import json
import os
import traceback

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "barrier_probe.json")
report = {}


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    cls = getattr(unreal, "ProximityBarrierComponent", None)
    report["class_present"] = cls is not None
    if cls is None:
        return
    err, obj = call(unreal.new_object, cls)
    report["new_object"] = err
    if err != "ok" or obj is None:
        return
    props = {}
    for p in ("LocalAxis", "TriggerDistance", "bTriggerOnNegativeSide", "bRequireInsideFirst",
              "PlayerIndex", "UpdateInterval", "bDrawDebug", "bIncludeAttachedActors",
              "bForceCollisionWhenShown", "bStopPollingAfterTrigger"):
        err2, v = call(obj.get_editor_property, p)
        props[p] = str(v) if err2 == "ok" else err2
    report["defaults"] = props
    report["has_OnSealed"] = hasattr(obj, "OnSealed")
    for m in ("IsSealed", "GetSignedDistance", "SealNow"):
        report["fn_" + m] = hasattr(obj, m)


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
