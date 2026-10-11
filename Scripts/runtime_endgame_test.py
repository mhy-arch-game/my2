"""Runtime check (real -game session): spin sampling + focus/prompt + interact + clean exit."""
import math
import unreal

STATE = "E:/BaiduNetdiskDownload/my2/Scripts/endgame_runtime_state.txt"
MAP_OBJ = "/Game/FirstPerson/firstvision.firstvision"
NL = chr(10)
OFFSETS = [(0.0, -200.0, 0.0), (0.0, 200.0, 0.0), (200.0, 0.0, 0.0), (-200.0, 0.0, 0.0),
           (0.0, -300.0, 0.0), (0.0, 300.0, 0.0)]
N = {"t": 0.0, "stage": 0, "target": None, "det": None, "quit_at": 0.0, "errs": 0,
     "world": None, "pc": None, "tp": -1, "last_tp": 0.0, "yaws": [], "next_yaw": 0.5}

def say(m):
    try:
        with open(STATE, "a", encoding="utf-8") as fh:
            fh.write(str(m) + NL)
    except Exception:
        pass

def aim(pc, cam, target):
    loc = target.get_actor_location()
    dx = loc.x - cam.x
    dy = loc.y - cam.y
    dz = loc.z - cam.z
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
    pc.set_control_rotation(unreal.Rotator(pitch, yaw, 0.0))

def tick(dt):
    N["t"] += dt
    t = N["t"]
    try:
        if N["world"] is None and t > 0.5:
            cand = unreal.load_object(None, MAP_OBJ)
            if isinstance(cand, unreal.World):
                N["world"] = cand
                say("t=%.1f 世界来源：%s" % (t, MAP_OBJ))
        if N["world"] is None:
            return
        world = N["world"]
        if N["pc"] is None:
            N["pc"] = unreal.GameplayStatics.get_player_controller(world, 0)
            if N["pc"] is None:
                return
        pc = N["pc"]
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            return
        if N["det"] is None:
            for c in pawn.get_components_by_class(unreal.ActorComponent):
                if "InteractionDetector" in c.get_class().get_name():
                    N["det"] = c
                    break
        if N["target"] is None:
            for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                try:
                    n = a.get_class().get_name().lower()
                    if "end_of_the_game" in n or "endofthegame" in n:
                        N["target"] = a
                        loc = a.get_actor_location()
                        say("t=%.1f 找到目标 %s loc=(%.1f,%.1f,%.1f)" % (t, a.get_class().get_name(), loc.x, loc.y, loc.z))
                        comp = None
                        try: comp = a.get_interactable_component()
                        except Exception: pass
                        if comp is not None:
                            pr = None
                            try: pr = comp.get_editor_property("interaction_prompt")
                            except Exception: pass
                            say("  可交互组件=%s 提示词=[%s] enabled=%s" % (comp.get_class().get_name(), str(pr), str(comp.is_enabled())))
                        else:
                            say("  没有可交互组件！")
                        break
                except Exception:
                    pass
            if N["target"] is None:
                if t > 12.0 and N["stage"] == 0:
                    N["stage"] = 9
                    say("t=%.1f 没找到目标" % t)
                return

        tloc = N["target"].get_actor_location()
        ploc = pawn.get_actor_location()
        dist = math.sqrt((ploc.x-tloc.x)**2 + (ploc.y-tloc.y)**2 + (ploc.z-tloc.z)**2)

        # 自转采样（每 1 秒一次，共 3 次）
        if t >= N["next_yaw"] and len(N["yaws"]) < 4:
            N["next_yaw"] = t + 1.0
            r = N["target"].get_actor_rotation()
            N["yaws"].append(float(r.yaw))
            say("t=%.1f 自转采样 yaw=%.1f" % (t, r.yaw))
            if len(N["yaws"]) == 4:
                ds = [round((N["yaws"][i+1] - N["yaws"][i]) % 360.0, 1) for i in range(3)]
                say("每秒 yaw 增量: %s -> %s" % (str(ds), "持续旋转 OK" if all(d > 0.5 for d in ds) else "没在转 FAIL"))

        if N["stage"] == 0:
            N["stage"] = 1
            say("t=%.1f 角色距目标 %.0fcm" % (t, dist))

        if dist > 350.0:
            if t - N["last_tp"] > 1.5:
                N["tp"] = (N["tp"] + 1) % len(OFFSETS)
                ox, oy, oz = OFFSETS[N["tp"]]
                pawn.set_actor_location(unreal.Vector(tloc.x+ox, tloc.y+oy, tloc.z+oz), False, True)
                N["last_tp"] = t
            return

        cam_mgr = unreal.GameplayStatics.get_player_camera_manager(world, 0)
        cam = cam_mgr.get_camera_location() if cam_mgr else ploc
        aim(pc, cam, N["target"])

        if t > 6.0 and N["stage"] == 1:
            det = N["det"]
            if det is None:
                N["stage"] = 9
                say("t=%.1f 没找到 InteractionDetector" % t)
                return
            fa = det.get_focused_actor()
            pr = det.get_current_prompt()
            say("t=%.1f 距目标 %.0fcm focused=%s prompt=[%s]" % (t, dist, fa.get_name() if fa else "None", str(pr)))
            if fa is not None and fa.get_name() == N["target"].get_name():
                say("t=%.1f 已聚焦，调用 TryInteract()" % t)
                det.try_interact()
                N["quit_at"] = t
                N["stage"] = 2
            elif t > 20.0:
                N["stage"] = 9
                say("t=%.1f 超时未聚焦" % t)

        if N["stage"] == 2 and t - N["quit_at"] > 6.0:
            N["stage"] = 9
            say("t=%.1f 交互后 6 秒仍活着：退出流程没生效" % (t - N["quit_at"]))
    except Exception as exc:
        N["errs"] += 1
        if N["errs"] <= 4:
            say("tick 异常 %r" % exc)

say("=== runtime endgame+spin test 已加载 ===")
unreal.register_slate_post_tick_callback(tick)