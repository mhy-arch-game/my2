"""Verify movement audio states + interface reflection."""
import json, os, traceback
import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "movaudio_probe.json")
report = {}


def call(fn, *a):
    try:
        return ("ok", fn(*a))
    except Exception as exc:
        return (repr(exc)[:140], None)


def main():
    for n in ("MovementAudioInterface", "MovementAudioComponent", "MovementAudioState",
              "MovementStateMusic", "MovementAudioEvent"):
        report["has_" + n] = hasattr(unreal, n)

    st = getattr(unreal, "MovementAudioState", None)
    report["states"] = [v for v in dir(st) if v.isupper()] if st else None
    report["music_fields"] = [p for p in dir(unreal.MovementStateMusic) if not p.startswith("_")]

    cls = getattr(unreal, "MovementAudioComponent", None)
    err, o = call(unreal.new_object, cls)
    if err == "ok" and o is not None:
        props = {}
        for p in ("WalkSpeedThreshold", "SprintSpeedThreshold", "bPlayMusicPerState",
                  "StateMusic", "MusicFadeTime", "bDispatchToInterfaceListeners",
                  "bLogStateChanges", "RunSpeedThreshold"):
            err2, v = call(o.get_editor_property, p)
            props[p] = str(v) if err2 == "ok" else err2
        report["defaults"] = props
        err3, s = call(o.get_movement_state)
        report["get_movement_state"] = str(s) if err3 == "ok" else err3
        err4, d = call(o.get_movement_audio_debug_string)
        report["debug_string"] = str(d) if err4 == "ok" else err4


try:
    main()
except Exception as exc:
    report["fatal"] = repr(exc); report["traceback"] = traceback.format_exc()

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
