import json, os, traceback
import unreal
MAP = "/Game/FirstPerson/firstvision"
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_outline.json")
report = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "<ERR " + repr(exc)[:80] + ">"
def gp(o, n):
    try: return o.get_editor_property(n)
    except Exception: return None
try:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    pp = []
    det = None
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        if "PostProcess" in cn:
            st = gp(a, "Settings")
            wb = None
            if st is not None:
                wb = gp(st, "WeightedBlendables")
            pp.append({"label": str(call(a.get_actor_label)), "class": cn,
                       "unbound": str(gp(a, "bUnbound")), "priority": str(gp(a, "Priority")),
                       "blendables": str(wb)[:150] if wb is not None else "?"})
        if "FirstPersonCharacter" in cn:
            for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
                ccn = str(call(c.get_class().get_name))
                if "PostProcess" in ccn:
                    pp.append({"label": str(call(a.get_actor_label)) + "/" + ccn,
                               "class": ccn, "blendables": str(gp(c, "Settings"))[:100]})
                if "InteractionDetector" in ccn:
                    det = {"owner": str(call(a.get_actor_label)),
                           "bApplyFocusOutline": str(gp(c, "bApplyFocusOutline")),
                           "bUseStencilOutline": str(gp(c, "bUseStencilOutline")),
                           "FocusedOutlineStencil": str(gp(c, "FocusedOutlineStencil")),
                           "OutlineParameterCollection": str(gp(c, "OutlineParameterCollection"))[:60],
                           "OutlineThicknessParameter": str(gp(c, "OutlineThicknessParameter")),
                           "FocusedOutlineThickness": str(gp(c, "FocusedOutlineThickness")),
                           "bOutlineVisiblePrimitivesOnly": str(gp(c, "bOutlineVisiblePrimitivesOnly"))}
    report["post_process"] = pp
    report["detector"] = det
    # 有没有别的交互物自带描边（StructureBlockComponent）
    blocks = []
    for a in ss.get_all_level_actors():
        for c in list(call(a.get_components_by_class, unreal.ActorComponent) or []):
            if "StructureBlock" in str(call(c.get_class().get_name)):
                blocks.append({"owner": str(call(a.get_actor_label)),
                               "bUseOutline": str(gp(c, "bUseOutline")),
                               "HighlightStencilValue": str(gp(c, "HighlightStencilValue"))})
    report["structure_blocks"] = blocks[:6]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["tb"] = traceback.format_exc()[-300:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(report, fh, indent=1, ensure_ascii=False)
print("###OUTLINE " + json.dumps(report, ensure_ascii=True)[:1800])