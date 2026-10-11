import json, os
import unreal

OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "api_probe.json")
rep = {}
def names(obj, keys):
    out = []
    try:
        for n in dir(obj):
            ln = n.lower()
            if any(k in ln for k in keys):
                out.append(n)
    except Exception:
        pass
    return sorted(out)

keys = ["world", "game_instance", "gameinstance", "player_controller"]
rep["unreal_module"] = sorted([n for n in dir(unreal) if "world" in n.lower() or "library" in n.lower() or "bplib" in n.lower() or "subsystem" in n.lower()])
for cls in ["SystemLibrary", "GameplayStatics", "KismetSystemLibrary", "KismetMathLibrary", "PythonScriptLibrary", "EditorLevelLibrary", "UnrealEditorSubsystem"]:
    o = getattr(unreal, cls, None)
    rep[cls] = ("MISSING" if o is None else names(o, keys))
rep["has_unreal_editor_subsystem"] = hasattr(unreal, "UnrealEditorSubsystem")
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("PROBE API done")