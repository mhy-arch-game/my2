import json, os, traceback
import unreal

PREV = "E:/BaiduNetdiskDownload/my2/Scripts/probe_jiguan3.json"

def safe(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:80] + ">"

def s(v, n=140):
    try:
        return str(v)[:n]
    except Exception:
        return "<str-err>"

def gp(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return None

def arr(v):
    if v is None or isinstance(v, str):
        return []
    try:
        return list(v)
    except Exception:
        return []

def num(v):
    try:
        return round(float(v), 1)
    except Exception:
        return str(v)[:24]

print("###PREV_KEYS " + json.dumps(list(json.load(open(PREV, encoding="utf-8")).keys())))

try:
    exts = unreal.MovieSceneSequenceExtensions
    bext = unreal.MovieSceneBindingExtensions
    trext = unreal.MovieSceneTrackExtensions
    sext = unreal.MovieSceneSectionExtensions
    keys_api = []
    deep = {}
    for nm in ("LS_jiguanqiang1", "LS_jiguanqiang2"):
        a = unreal.load_asset("/Game/" + nm)
        info = {"end_s": num(safe(exts.get_playback_end_seconds, a)), "binds": []}
        for b in arr(safe(exts.get_bindings, a)):
            be = {"name": s(safe(bext.get_name, b), 50), "tracks": []}
            for t in arr(safe(bext.get_tracks, b)):
                te = {"track": s(safe(t.get_class().get_name), 60), "secs": []}
                for sec in arr(safe(trext.get_sections, t)):
                    se = {"class": s(safe(sec.get_class().get_name), 60),
                          "start_f": num(safe(sext.get_start_frame, sec)),
                          "end_f": num(safe(sext.get_end_frame, sec)),
                          "len_f": num(sec.get_length()) if hasattr(sec, "get_length") else "?",
                          "chans": []}
                    for ch in arr(safe(sext.get_all_channels, sec)):
                        if not keys_api:
                            keys_api.append("CH:" + str([x for x in dir(ch) if not x.startswith("_")])[:400])
                        ce = {"ch": s(safe(ch.get_class().get_name), 50)}
                        ks = safe(ch.get_keys)
                        klist = arr(ks)
                        if not keys_api and klist:
                            keys_api.append("KEY:" + str([x for x in dir(klist[0]) if not x.startswith("_")])[:400])
                        ce["n"] = len(klist) if isinstance(ks, (list,)) or not isinstance(ks, str) else s(ks, 40)
                        ce["n"] = len(klist)
                        smp = []
                        idxs = sorted(set(list(range(min(2, len(klist)))) + list(range(max(0, len(klist) - 2), len(klist)))))
                        for ix in idxs:
                            k = klist[ix]
                            val = safe(k.get_value) if hasattr(k, "get_value") else safe(k.get_double_value) if hasattr(k, "get_double_value") else "?"
                            tmv = safe(k.get_time) if hasattr(k, "get_time") else "?"
                            smp.append(num(val) + "@" + s(tmv, 30))
                        ce["samples"] = smp
                        se["chans"].append(ce)
                    te["secs"].append(se)
                be["tracks"].append(te)
            info["binds"].append(be)
        deep[nm] = info
    print("###KEYAPI " + json.dumps(keys_api, ensure_ascii=True)[:900])
    print("###LSDEEP " + json.dumps(deep, ensure_ascii=True)[:4500])
except Exception as exc:
    print("###LS_FATAL " + repr(exc)[:200])

prev = json.load(open(PREV, encoding="utf-8"))
print("###WALLS " + json.dumps(prev.get("walls"), ensure_ascii=True)[:3500])
print("###LINKS " + json.dumps(prev.get("links"), ensure_ascii=True)[:2500])
print("###LAMPS " + json.dumps(prev.get("lamps"), ensure_ascii=True)[:2500])
inter = prev.get("interactables") or []
hot = [x for x in inter if x.get("lights") == "True" or x.get("toggle") == "True" or x.get("track") == "True"]
print("###INTER_TOTAL " + str(len(inter)) + " hot=" + str(len(hot)))
print("###INTER_HOT " + json.dumps(hot, ensure_ascii=True)[:3000])
print("[PROBE4 done]")