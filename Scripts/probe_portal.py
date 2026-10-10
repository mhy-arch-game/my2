"""Verify portal target mode + OwnerEra editability + debug string."""
import json
import os
import traceback

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "portal_probe.json")
report = {}


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:150], None)


def main():
    cls = getattr(unreal, "TimeEraPortalComponent", None)
    report["class"] = cls is not None
    if cls is None:
        return

    enum = getattr(unreal, "TimeEraPortalTargetMode", None)
    report["enum"] = [v for v in dir(enum) if v.isupper()] if enum else None

    err, o = call(unreal.new_object, cls)
    if err != "ok" or o is None:
        report["new_object"] = err
        return

    props = {}
    for p in ("TargetMode", "VerticalOffset", "OwnerEra", "bAutoDetectEra",
              "CounterpartActor", "CounterpartId", "TeleportOffset", "bPlaceOnGround"):
        err2, v = call(o.get_editor_property, p)
        props[p] = str(v) if err2 == "ok" else err2
    report["defaults"] = props

    # OwnerEra must be writable now
    report["owner_era_write"] = call(o.set_editor_property, "OwnerEra",
                                     unreal.TimeEra.MODERN)[0]
    err3, back = call(o.get_editor_property, "OwnerEra")
    report["owner_era_readback"] = str(back) if err3 == "ok" else err3

    err4, s = call(o.get_portal_debug_string)
    report["debug_string"] = str(s) if err4 == "ok" else err4


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[PORTALPROBE] done")
