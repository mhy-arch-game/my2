import json, traceback
import unreal

def safe(fn, *a, **kw):
    try:
        return fn(*a, **kw)
    except Exception as exc:
        return "<ERR " + repr(exc)[:70] + ">"

def s(v, n=160):
    try:
        return str(v)[:n]
    except Exception:
        return "<err>"

def arr(v):
    if v is None or isinstance(v, str):
        return []
    try:
        return list(v)
    except Exception:
        return []

ex = unreal.MovieSceneSequenceExtensions
bx = unreal.MovieSceneBindingExtensions
tx = unreal.MovieSceneTrackExtensions
sx = unreal.MovieSceneSectionExtensions
out = {}
try:
    for nm in ("LS_jiguanqiang1", "LS_jiguanqiang2"):
        a = unreal.load_asset("/Game/" + nm)
        info = {"sec": []}
        for b in arr(safe(ex.get_bindings, a)):
            for t in arr(safe(bx.get_tracks, b)):
                for sec in arr(safe(tx.get_sections, t)):
                    d = {"class": s(safe(sec.get_class().get_name), 60),
                         "start": s(safe(sx.get_start_frame, sec), 30),
                         "end": s(safe(sx.get_end_frame, sec), 30), "ch": []}
                    for ch in arr(safe(sx.get_all_channels, sec)):
                        ce = {"cls": s(safe(ch.get_class().get_name), 50),
                              "name": s(safe(getattr(ch, "channel_name", "?")), 40)}
                        ks = arr(safe(ch.get_keys))
                        ce["nkeys"] = len(ks)
                        smp = []
                        for k in (ks[:2] + ks[-2:] if len(ks) > 4 else ks):
                            v = safe(k.get_value) if hasattr(k, "get_value") else "?"
                            smp.append(s(v, 50) + " @ " + s(safe(k.get_time), 30))
                        ce["samples"] = smp
                        if not d["ch"]:
                            ce["ch_api"] = str([x for x in dir(ch) if not x.startswith("_")])[:300]
                        if ks and not d["ch"]:
                            ce["key_api"] = str([x for x in dir(ks[0]) if not x.startswith("_")])[:300]
                        d["ch"].append(ce)
                    info["sec"].append(d)
        out[nm] = info
except Exception as exc:
    out["fatal"] = repr(exc)[:300]
    out["tb"] = traceback.format_exc()[-500:]
print("###LSDEEP2 " + json.dumps(out, ensure_ascii=True)[:5000])
print("[PROBE5 done]")