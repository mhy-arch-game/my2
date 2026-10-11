"""Persist the 触碰 prompt onto the level instance Interactable component."""
import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "set_prompt.json")
rep = {}
def call(fn, *a, **kw):
    try: return fn(*a, **kw)
    except Exception as exc: return "ERR " + repr(exc)[:110]
def make_text(s):
    tries = []
    T = getattr(unreal, "Text", None)
    if T is not None:
        tries.append(("unreal.Text(s)", lambda: T(s)))
        tries.append(("unreal.Text.from_string", lambda: T.from_string(s)))
    tries.append(("str", lambda: s))
    for name, fn in tries:
        try:
            v = fn()
            if v is not None:
                rep.setdefault("text_via", []).append(name)
                return v
        except Exception as exc:
            rep.setdefault("text_err", []).append(name + ": " + repr(exc)[:60])
    return s
try:
    txt = make_text("触碰")
    rep["text_type"] = str(type(txt).__name__)
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/firstvision")
    ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in ss.get_all_level_actors():
        cn = str(call(a.get_class().get_name))
        if "end_of_the_game" in cn.lower() or "endofthegame" in cn.lower():
            for c in (call(a.get_components_by_class, unreal.ActorComponent) or []):
                if "InteractableComponent" in str(call(c.get_class().get_name)):
                    rep["before"] = str(call(c.get_editor_property, "InteractionPrompt"))
                    call(c.set_editor_property, "InteractionPrompt", txt,
                         notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
                    rep["after"] = str(call(c.get_editor_property, "InteractionPrompt"))
    rep["save"] = str(call(unreal.EditorLoadingAndSavingUtils.save_current_level))[:20]
    rep["save_dirty"] = str(call(unreal.EditorLoadingAndSavingUtils.save_dirty_packages, True, True))[:20]
except Exception as exc:
    rep["fatal"] = repr(exc)
    rep["tb"] = traceback.format_exc()[-400:]
with open(OUT, "w", encoding="utf-8") as fh: json.dump(rep, fh, indent=1, ensure_ascii=True)
print("SET PROMPT done")