import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_uicue_trigger.json")
report = {}
try:
    report["trigger_values"] = sorted([n for n in dir(unreal.UiCueTrigger) if n.isupper()])
    cue = unreal.UiCue()
    report["cue_fields"] = sorted([n for n in dir(cue) if n in ("trigger", "trigger_id", "cue_id", "text", "voice", "b_once")])
    report["subsystem_exists"] = hasattr(unreal, "UiCueSubsystem")
except Exception as exc:
    report["fatal"] = repr(exc)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY UICUE TRIGGER done]")
