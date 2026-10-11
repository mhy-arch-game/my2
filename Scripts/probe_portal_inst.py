import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_portal_inst.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
def r3(v):
    try: return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]
    except Exception: return str(v)[:40]
KEYS = ["TargetMode", "VerticalOffset", "TeleportOffset", "bMatchCounterpartYaw", "bPlaceOnGround",
        "GroundTraceDistance", "GroundClearance", "OwnerEra", "bAutoDetectEra", "bSwitchEra",
        "bRequireCounterpartInOtherEra", "CounterpartId", "bRegisterAsAnchor", "bSuppressBuiltInToggle",
        "PortalCooldown", "TransitionDelay"]
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = {}
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("kongjianchuansuoqi"):
            continue
        d = {"class": str(call(a.get_class().get_name)),
             "loc": r3(call(a.get_actor_location)),
             "yaw": round(float(call(a.get_actor_rotation).yaw), 1),
             "comps": []}
        portal = None
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            cn = str(call(c.get_class().get_name))
            d["comps"].append(cn)
            if "TimeEraPortal" in cn:
                portal = c
            if "TimeEraComponent" in cn:
                d["era"] = str(gp(c, "Era"))
            if "StaticMeshComponent" in cn:
                lb = call(c.get_local_bounds)
                if not isinstance(lb, str) and len(lb) > 1:
                    d["mesh_min"] = r3(lb[0])
                    d["mesh_max"] = r3(lb[1])
        if portal is not None:
            for k in KEYS:
                v = gp(portal, k)
                if v is not None:
                    d["P_" + k] = str(v)[:60]
            ca = gp(portal, "CounterpartActor")
            d["P_CounterpartActor"] = str(ca)[:46] if ca else "None"
        out[lbl] = d
    report["portals"] = out
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###PORTAL " + json.dumps(report, ensure_ascii=True)[:7000])