import json, os, traceback
import unreal

MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_jump2.json")
report = {}

def call(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:90] + ">"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return None

def cname(c):
    return str(call(c.get_class().get_name))

try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    boxes = []
    for a in ss.get_all_level_actors():
        lbl = str(call(a.get_actor_label))
        if not lbl.startswith("jumping_area"):
            continue
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "GravityZone" not in cname(c):
                continue
            ext = gp(c, "BoxExtent")
            loc = call(a.get_actor_location)
            box = {"label": lbl,
                   "loc": [round(float(loc.x), 1), round(float(loc.y), 1), round(float(loc.z), 1)],
                   "ext": [round(float(ext.x), 1), round(float(ext.y), 1), round(float(ext.z), 1)]}
            box["min"] = [box["loc"][i] - box["ext"][i] for i in range(3)]
            box["max"] = [box["loc"][i] + box["ext"][i] for i in range(3)]
            boxes.append(box)
    report["boxes"] = boxes
    overlaps = []
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            a, b = boxes[i], boxes[j]
            ov = [min(a["max"][k], b["max"][k]) - max(a["min"][k], b["min"][k]) for k in range(3)]
            overlaps.append({"pair": a["label"] + " & " + b["label"],
                             "overlap_cm": [round(v, 1) for v in ov],
                             "overlaps": all(v > 0 for v in ov)})
    report["overlaps"] = overlaps

    cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
    cdo = unreal.get_default_object(cls) if cls else None
    if cdo:
        report["char"] = {"JumpMaxCount": str(gp(cdo, "JumpMaxCount")),
                          "JumpMaxHoldTime": str(gp(cdo, "JumpMaxHoldTime"))}
        mov = gp(cdo, "CharacterMovement")
        if mov:
            report["char_move"] = {"JumpZVelocity": str(gp(mov, "JumpZVelocity")),
                                   "GravityScale": str(gp(mov, "GravityScale")),
                                   "AirControl": str(gp(mov, "AirControl"))}
            nap = gp(mov, "NavAgentProps")
            if nap is not None:
                report["nav"] = str(nap)[:160]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1, ensure_ascii=False)
print("###JUMP2 " + json.dumps(report, ensure_ascii=True)[:3000])
print("[PROBE JUMP2 done]")