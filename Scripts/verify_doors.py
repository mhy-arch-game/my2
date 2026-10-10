"""Read-only verification of the door replacement."""
import json
import os

import unreal

OUT = os.path.join(r"E:\BaiduNetdiskDownload\my2", "Scripts", "verify_doors_result.json")
unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")

ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = ss.get_all_level_actors()

bp = unreal.EditorAssetLibrary.load_asset("/Game/bclass_source/active_door")
door_class = bp.generated_class()

doors = []
men_left = []
menkuang = []

for a in actors:
    try:
        label = a.get_actor_label()
    except Exception:
        label = ""
    cls = a.get_class().get_name()
    if a.get_class() == door_class:
        doors.append({"label": label, "name": a.get_name()})
    if label.startswith("menkuang"):
        menkuang.append(label)
    elif label.startswith("men"):
        if a.get_class() != door_class:
            men_left.append({"label": label, "name": a.get_name(), "class": cls})

report = {
    "total_actors": len(actors),
    "door_instances": len(doors),
    "doors": sorted(doors, key=lambda d: d["label"]),
    "men_still_not_door": men_left,
    "menkuang_count": len(menkuang),
}
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
unreal.log("[VERIFYDOORS] doors=%d men_left=%d menkuang=%d" % (
    len(doors), len(men_left), len(menkuang)))
