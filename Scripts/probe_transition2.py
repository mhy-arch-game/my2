# -*- coding: utf-8 -*-
import unreal, json
out = {}
gz = unreal.GravityZoneComponent
out["gravity_dir_hits"] = sorted([n for n in dir(gz) if any(k in n.lower() for k in ("jump", "gravity_scale", "air_time", "preserving", "scale_jump"))])
pc = unreal.TimeEraPortalComponent
out["portal_dir_hits"] = sorted([n for n in dir(pc) if any(k in n.lower() for k in ("transition", "portal"))])
try:
    t = unreal.TeleportTransitionContext()
    out["struct_dir"] = sorted([n for n in dir(t) if not n.startswith("_")])
except Exception as e:
    out["struct_dir"] = "ERR %s" % e
unreal.log("[PROBE2] " + json.dumps(out, ensure_ascii=False, indent=1, sort_keys=True))
