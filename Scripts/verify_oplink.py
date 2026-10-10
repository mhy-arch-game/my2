import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "verify_oplink.json")
report = {}
try:
    act = unreal.InteractionOperationAction
    report["action_values"] = sorted([n for n in dir(act) if n.isupper()])
    link = unreal.InteractionLinkComponent
    report["link_dir"] = sorted([n for n in dir(link) if "sync" in n.lower() or "dispatch" in n.lower() or "entry" in n.lower()])
    rc = unreal.InteractionOperationReceiverComponent
    report["receiver_dir"] = sorted([n for n in dir(rc) if "apply" in n.lower() or "channel" in n.lower()])
    report["receiver_funcs"] = {n: (rc.find_function(n) is not None) for n in ("apply_operation", "can_receive", "get_channel")}
    report["entry_fields"] = sorted([p for p in ("Operation", "Channel", "Targets", "bMirrorSourceState", "ClosedOperation", "Location", "Value", "Target")])
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[VERIFY OPLINK done]")
