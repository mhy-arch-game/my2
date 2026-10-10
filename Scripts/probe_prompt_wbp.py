import json, os, traceback
import unreal
OUT = os.path.join("E:/BaiduNetdiskDownload/my2", "Scripts", "probe_prompt_wbp.json")
report = {}


def call(fn, *a, **kw):
    try:
        return ("ok", fn(*a, **kw))
    except Exception as exc:
        return (repr(exc)[:130], None)


try:
    for p in unreal.EditorAssetLibrary.list_assets("/Game/MHY_ARCH_GAME/UI", recursive=True, include_folder=False):
        data = unreal.EditorAssetLibrary.find_asset_data(p)
        cls = ""
        try:
            cls = str(data.asset_class_path.asset_name)
        except Exception:
            pass
        report.setdefault("ui_assets", []).append([str(p), cls])

    bp = unreal.EditorAssetLibrary.load_asset("/Game/MHY_ARCH_GAME/UI/WBP_InteractionPrompt")
    report["bp_class"] = bp.get_class().get_name() if bp else "MISSING"
    if bp:
        try:
            parent = bp.get_editor_property("parent_class")
            report["parent_class"] = str(parent)
        except Exception as exc:
            report["parent_err"] = repr(exc)[:120]
        # generated class of the widget blueprint
        try:
            gcls = bp.generated_class()
            report["generated_class"] = str(gcls)
            report["is_prompt_widget"] = bool(gcls and gcls.is_child_of(unreal.InteractionPromptWidget))
        except Exception as exc:
            report["gen_err"] = repr(exc)[:120]
        # widget tree
        try:
            tree = bp.get_editor_property("widget_tree")
            kids = []
            if tree:
                root = tree.get_editor_property("root_widget")
                def walk(w, depth=0, out=None):
                    if out is None:
                        out = []
                    if w is None:
                        return out
                    out.append({"depth": depth, "name": w.get_name(), "class": w.get_class().get_name(),
                                "text": str(call(w.get_editor_property, "text")[1])[:60] if w.get_class().get_name() == "TextBlock" else ""})
                    try:
                        for c in w.get_children():
                            walk(c, depth + 1, out)
                    except Exception:
                        pass
                    return out
                kids = walk(root)
            report["widget_tree"] = kids
        except Exception as exc:
            report["tree_err"] = repr(exc)[:150]
        # function graphs (does it override SetPrompt?)
        try:
            graphs = bp.get_editor_property("function_graphs")
            report["function_graphs"] = [g.get_name() for g in graphs] if graphs else []
        except Exception as exc:
            report["graphs_err"] = repr(exc)[:120]
        try:
            ubergraph = bp.get_editor_property("ubergraph_pages")
            report["ubergraph_pages"] = [g.get_name() for g in ubergraph] if ubergraph else []
        except Exception as exc:
            report["uber_err"] = repr(exc)[:120]
except Exception as exc:
    report["fatal"] = repr(exc)
    report["traceback"] = traceback.format_exc()
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=2, ensure_ascii=False)
print("[PROBE PROMPT WBP done]")
