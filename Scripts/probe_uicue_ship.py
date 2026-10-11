import json, os, re, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_uicue_ship.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:80]
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
def arr(v):
    try: return [x for x in v]
    except Exception: return []
def p2s(v):
    s = str(v)
    m = re.search(r"'([^']+)'", s)
    return m.group(1) if m else s
def tags_of(a):
    return [str(t) for t in arr(call(gp, a, "tags"))]
try:
    da = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/DA_UiCueSet")
    cues = []
    for c in arr(gp(da, "Cues")):
        cd = {"CueId": str(gp(c, "CueId")), "Trigger": str(gp(c, "Trigger")),
              "TriggerId": str(gp(c, "TriggerId")), "TextLen": len(str(gp(c, "Text"))),
              "Voices": [p2s(v) for v in arr(gp(c, "Voices"))]}
        cues.append(cd)
    report["cues"] = cues
    report["triggerIds"] = sorted(set([c["TriggerId"] for c in cues if c["TriggerId"] not in ("None","")]))

    # sound durations (cook-safe field check)
    sounds = {}
    for p in sorted(set([v for c in cues for v in c["Voices"]])):
        a = unreal.EditorAssetLibrary.load_asset(p)
        sounds[p] = {"class": str(call(a.get_class)) if a else "NOLOAD",
                     "duration": str(call(gp, a, "duration")),
                     "bLooping": str(call(gp, a, "looping"))}
    report["sounds"] = sounds

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = []
    for a in ss.get_all_level_actors():
        cn = call(a.get_class)
        cname = call(cn.get_name) if cn else "?"
        comps = [str(call(c.get_class().get_name)) for c in arr(call(a.get_components_by_class, unreal.ActorComponent))]
        actors.append({"name": str(call(a.get_name)), "label": str(call(a.get_actor_label)),
                       "class": str(cname), "tags": tags_of(a), "comps": comps})
    report["actorCount"] = len(actors)
    report["interactableCount"] = sum(1 for a in actors if "InteractableComponent" in a["comps"])
    rep = {}
    for t in report["triggerIds"]:
        hits = [a for a in actors if a["name"] == t or a["label"] == t or a["name"].startswith(t) or a["label"].startswith(t)]
        rep[t] = hits
    report["matchReport"] = rep
    # who would match using ONLY cook-safe keys (GetName / tags)?
    safe = {}
    for t in report["triggerIds"]:
        nm = [a["name"] for a in actors if a["name"] == t or a["name"].startswith(t)]
        tg = [[a["label"] for a in actors if any(x == t or x.endswith(":" + t) for x in a["tags"])]]
        safe[t] = {"byName": nm, "byTag": tg[0]}
    report["cookSafeMatch"] = safe
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-900:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=True)
print("PROBE UICUE SHIP v2 done")