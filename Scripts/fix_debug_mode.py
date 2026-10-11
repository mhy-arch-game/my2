import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "fix_debug_mode.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:90] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    # 1) 蓝图模板里的默认值
    cls = unreal.load_class(None, BP + ".BP_FirstPersonCharacter_C")
    cdo = unreal.get_default_object(cls) if cls else None
    cdo_val = None
    if cdo:
        for c in list(call(cdo.get_components_by_class, unreal.ActorComponent) or []):
            if "InteractionDetector" in str(call(c.get_class().get_name)):
                cdo_val = gp(c, "bDrawDebug")
    report["cdo_bDrawDebug"] = str(cdo_val)

    # 2) 关卡实例：关掉
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    fixed = []
    for a in ss.get_all_level_actors():
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if gp(c, "bDrawDebug") is True:
                call(c.set_editor_property, "bDrawDebug", False,
                     notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                fixed.append(str(call(a.get_actor_label)) + " / " + str(call(c.get_class().get_name)))
    report["turned_off"] = fixed
    report["save_level"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]

    # 3) 蓝图模板若也是 True，一并关掉
    if cdo_val is True:
        bp = unreal.EditorAssetLibrary.load_asset(BP)
        scs = gp(bp, "SimpleConstructionScript")
        nodes = list(call(scs.get_all_nodes) or []) if scs is not None else []
        for n in nodes:
            t = gp(n, "ComponentTemplate")
            if t is not None and gp(t, "bDrawDebug") is True:
                call(t.set_editor_property, "bDrawDebug", False)
                report.setdefault("bp_template_fixed", []).append(str(call(t.get_class().get_name)))
        report["save_bp"] = str(call(unreal.BlueprintEditorLibrary.compile_blueprint, bp))[:20]
        report["save_bp_asset"] = str(call(unreal.EditorAssetLibrary.save_loaded_asset, bp))[:20]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###FIXDBG " + json.dumps(report, ensure_ascii=True)[:900])