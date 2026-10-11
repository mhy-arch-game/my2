import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_ls_walls.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


def v3(v):
    try:
        return [round(float(v.x), 1), round(float(v.y), 1), round(float(v.z), 1)]
    except Exception:
        return str(v)[:40]


try:
    # 1) LS 资产
    ls = []
    for p in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False):
        s = str(p)
        if "jiguanqiang" in s.lower():
            data = unreal.EditorAssetLibrary.find_asset_data(p)
            cls = ""
            try:
                cls = str(data.asset_class_path.asset_name)
            except Exception:
                pass
            ls.append([s, cls])
    report["jiguanqiang_assets"] = ls

    # 2) 关卡：LS actor + 两面墙
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors, walls = [], []
    for a in actor_ss.get_all_level_actors():
        try:
            cn = a.get_class().get_name()
            lbl = a.get_actor_label()
        except Exception:
            continue
        if "LevelSequence" in cn:
            seq = call(a.get_editor_property, "Sequence")
            actors.append({"label": lbl, "class": cn, "loc": v3(a.get_actor_location()),
                           "sequence": str(seq[1])[:80] if seq[0] == "ok" else str(seq),
                           "has_player": str(call(a.get_sequence_player)[1])[:60]})
        if lbl in ("jiguanqiang1", "jiguanqiang2"):
            o = unreal.Vector(0, 0, 0)
            e = unreal.Vector(0, 0, 0)
            call(a.get_actor_bounds, False, o, e)
            walls.append({"label": lbl, "loc": v3(a.get_actor_location()),
                          "bounds_origin": v3(o), "bounds_extent": v3(e),
                          "components": [c.get_class().get_name() for c in a.get_components_by_class(unreal.ActorComponent)]})
    report["level_sequences"] = actors
    report["walls"] = walls
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE LS WALLS done]")
