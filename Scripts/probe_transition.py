# -*- coding: utf-8 -*-
import unreal, json

out = {}

def has_prop(cls, name):
    try:
        cls.lookup_property(name)
        return True
    except Exception:
        return False

def has_func(cls, name):
    try:
        return cls.find_function(name) is not None
    except Exception:
        return False

# ---- transition payload struct ----
try:
    ctx = unreal.TeleportTransitionContext
    out["context_fields"] = sorted([p.name for p in ctx.fields()])
except Exception as e:
    out["context_fields"] = "ERR %s" % e

# ---- transition receiver interface ----
for path in ["/Script/MHY_ARCH_GAME.TeleportTransitionInterface"]:
    try:
        c = unreal.load_class(None, path)
        out["interface_loaded"] = str(c)
    except Exception as e:
        out["interface_loaded"] = "ERR %s" % e

# ---- portal component surface ----
try:
    pc = unreal.TimeEraPortalComponent
    out["portal_props"] = {
        "transition_delay": has_prop(pc, "transition_delay"),
        "dispatch_transition_to_interface_listeners": has_prop(pc, "dispatch_transition_to_interface_listeners"),
        "on_transition_begin_field": has_prop(pc, "on_transition_begin"),
        "on_transition_end_field": has_prop(pc, "on_transition_end"),
    }
    out["portal_funcs"] = {
        "is_transitioning": has_func(pc, "is_transitioning"),
        "try_use_portal": has_func(pc, "try_use_portal"),
        "can_switch_era": has_func(pc, "can_switch_era"),
    }
    names = [n for n in dir(pc) if "transition" in n.lower()]
    out["portal_dir_transition"] = names
except Exception as e:
    out["portal"] = "ERR %s" % e

# ---- gravity preset surface ----
try:
    gz = unreal.GravityZoneComponent
    out["gravity_funcs"] = {
        "configure_preserving_jump_height": has_func(gz, "configure_preserving_jump_height"),
        "get_jump_height_scale": has_func(gz, "get_jump_height_scale"),
        "get_air_time_scale": has_func(gz, "get_air_time_scale"),
    }
    out["gravity_props"] = {
        "gravity_scale_inside": has_prop(gz, "gravity_scale_inside"),
        "scale_jump_velocity": has_prop(gz, "scale_jump_velocity"),
    }
except Exception as e:
    out["gravity"] = "ERR %s" % e

unreal.log("[PROBE] " + json.dumps(out, ensure_ascii=False, indent=1, sort_keys=True))
