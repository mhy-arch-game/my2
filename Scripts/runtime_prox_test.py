import unreal

STATE = "E:/BaiduNetdiskDownload/my2/Scripts/runtime_prox_state.txt"
N = {"t": 0.0, "last": -1.0, "fired": False, "done": False}

def say(m):
    try:
        with open(STATE, "a", encoding="utf-8") as fh:
            fh.write(str(m) + "\n")
    except Exception:
        pass

def tick(dt):
    if N["done"]:
        return
    N["t"] += dt
    t = N["t"]
    try:
        gi = unreal.SystemLibrary.get_game_instance()
        if not gi:
            if N["last"] < 0:
                N["last"] = 0.0
                say("t=0 还没有 game instance")
            return
        pc = None
        try:
            pc = unreal.GameplayStatics.get_player_controller(gi, 0)
        except Exception:
            pass
        c = pc if pc else gi
        actors = unreal.GameplayStatics.get_all_actors_of_class(c, unreal.StaticMeshActor)
        wall = None
        for a in actors:
            try:
                if str(a.get_actor_label()) == "jiguanqiang1":
                    wall = a
                    break
            except Exception:
                pass
        if wall is None:
            if N["last"] < 0:
                N["last"] = 0.0
                say("t=0 找不到 jiguanqiang1（mesh actor %d 个）" % len(actors))
            return
        comps = list(wall.get_components_by_class(unreal.ActorComponent))
        meshes = [x for x in comps if "StaticMeshComponent" in x.get_class().get_name()]
        m = meshes[0] if meshes else None
        ps = [x for x in comps if "ProximitySequence" in x.get_class().get_name()]
        if not N["fired"]:
            N["fired"] = True
            say("找到墙: %s" % [x.get_class().get_name() for x in comps])
            loc = wall.get_actor_location()
            say("调用前: loc=(%.1f,%.1f,%.1f) actorCol=%s meshCol=%s prof=%s visible=%s hidden=%s" % (
                loc.x, loc.y, loc.z, wall.get_actor_enable_collision(),
                m.get_collision_enabled() if m else "?", m.get_collision_profile_name() if m else "?",
                m.is_visible() if m else "?", wall.get_editor_property("bHidden")))
            if ps:
                say("PS: Trigger=%s bOnce=%s bBlockOnFinish=%s bApplyInitial=%s SlowZone=%s SlowSpeed=%s" % (
                    ps[0].get_editor_property("TriggerDistance"), ps[0].get_editor_property("bOnce"),
                    ps[0].get_editor_property("bBlockOnFinish"), ps[0].get_editor_property("bApplyInitialStateOnBeginPlay"),
                    ps[0].get_editor_property("SlowZoneDistance"), ps[0].get_editor_property("SlowSpeed")))
                ps[0].trigger_now()
                say("已调用 TriggerNow")
            else:
                say("没有 PS 组件!")
        if t - N["last"] >= 1.0:
            N["last"] = t
            loc = wall.get_actor_location()
            say("t=%4.1fs loc=(%.1f,%.1f,%.1f) actorCol=%s meshCol=%s visible=%s blocking=%s" % (
                t, loc.x, loc.y, loc.z, wall.get_actor_enable_collision(),
                m.get_collision_enabled() if m else "?", m.is_visible() if m else "?",
                ps[0].is_blocking() if ps else "?"))
        if t > 16.0:
            N["done"] = True
            say("测试结束，退出")
            try:
                unreal.SystemLibrary.quit_game(c, None, unreal.QuitPreference.QUIT, False)
            except Exception as exc:
                say("quit 失败 %r" % exc)
    except Exception as exc:
        say("tick 异常 %r" % exc)

say("=== 脚本加载 ===")
unreal.register_slate_post_tick_callback(tick)