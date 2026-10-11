"""Sample the end-game object rotation over time in a real -game session,
to prove the spin keeps running (永续旋转)."""
import unreal

STATE = "E:/BaiduNetdiskDownload/my2/Scripts/endgame_spin_state.txt"
MAP_OBJ = "/Game/FirstPerson/firstvision.firstvision"
NL = chr(10)
N = {"t": 0.0, "target": None, "next": 1.0, "samples": []}

def say(m):
    try:
        with open(STATE, "a", encoding="utf-8") as fh:
            fh.write(str(m) + NL)
    except Exception:
        pass

def tick(dt):
    N["t"] += dt
    t = N["t"]
    try:
        if N["target"] is None:
            if t < 0.5:
                return
            w = unreal.load_object(None, MAP_OBJ)
            if not isinstance(w, unreal.World):
                return
            for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
                try:
                    n = a.get_class().get_name().lower()
                    if "end_of_the_game" in n or "endofthegame" in n:
                        N["target"] = a
                        break
                except Exception:
                    pass
            if N["target"] is None:
                if t > 10.0 and not N["samples"]:
                    N["samples"].append("NOTFOUND")
                    say("t=%.1f 没找到物件" % t)
                return
            a = N["target"]
            comp = None
            rate = None
            tick_on = None
            try:
                comp = a.get_spin_component()
            except Exception as exc:
                say("取 Spin 组件异常 " + repr(exc))
            if comp is not None:
                try: rate = comp.get_editor_property("rotation_rate")
                except Exception: pass
                try: tick_on = comp.is_component_tick_enabled()
                except Exception: pass
            spinning = None
            try: spinning = a.is_spinning()
            except Exception as exc: say("IsSpinning 异常 " + repr(exc))
            say("找到 %s" % a.get_class().get_name())
            say("  Spin 组件=%s  tick_enabled=%s  rotation_rate=%s  is_spinning=%s" % (str(comp) != "None", tick_on, rate, spinning))
            return
        if t >= N["next"]:
            N["next"] = t + 2.0
            r = N["target"].get_actor_rotation()
            yaw, pitch, roll = float(r.yaw), float(r.pitch), float(r.roll)
            N["samples"].append((round(t, 1), yaw, pitch, roll))
            say("t=%.1f rotation yaw=%.1f pitch=%.1f roll=%.1f" % (t, yaw, pitch, roll))
            if len(N["samples"]) >= 4:
                ys = [s[1] for s in N["samples"]]
                deltas = [round((ys[i + 1] - ys[i]) % 360.0, 1) for i in range(len(ys) - 1)]
                ok = all(d > 1.0 for d in deltas)
                say("每 2 秒的 yaw 增量: %s" % str(deltas))
                say("结论: " + ("持续旋转 OK" if ok else "没有持续旋转 FAIL"))
                N["next"] = 1e9
    except Exception as exc:
        say("tick 异常 %r" % exc)

say("=== runtime spin test 已加载 ===")
unreal.register_slate_post_tick_callback(tick)