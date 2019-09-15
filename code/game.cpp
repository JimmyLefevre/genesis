
extern "C" {
    int _fltused = 0x9875;
    
    // As far as I understand it, this procedure is called whenever the DLL is loaded (ie. when LoadLibrary
    // is called) in order to set up global constructors or whatever.
    // From MSDN: "When a DLL is loaded using LoadLibrary, existing threads do not call the entry-point function of the newly loaded DLL."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682583(v=vs.85).aspx
    int _DllMainCRTStartup() {
        return 1;
    }
}

#include              "basic.h"
#include          "bitpacker.h"
#include                "sse.h"
#include               "keys.h"
#include               "math.h"
#include          "utilities.h"
#include             "string.h"
#include                "log.h"
#include            "profile.h"
#include                "bmp.h"
#include              "input.h"
#include               "font.h"
#include             "assets.h"
#include              "audio.h"
#include              "synth.h"
#include   "compression_fast.h"
// #include               "menu.h"
#include         "render_new.h"
#include               "mesh.h"
#include               "game.h"
#include       "game_context.h"
#include         "interfaces.h"

static os_function_interface os_platform;
static Log *global_log;

#include        "bitpacker.cpp"
#include            "print.cpp"
#include           "string.cpp"
#include              "log.cpp"
#include              "bmp.cpp"
#include            "input.cpp"
// #include             "font.cpp"
#include       "render_new.cpp"
#include             "mesh.cpp"
#include           "assets.cpp"
#include            "audio.cpp"
#include            "synth.cpp"
#include "compression_fast.cpp"
// #include             "menu.cpp"
// #include          "profile.cpp"

static inline v2 unit_scale_to_world_space_offset(v2 p, f32 camera_zoom) {
    v2 result = p - V2(0.5f);
    f32 inv_camera_zoom = 1.0f / camera_zoom;
    result.x *= GAME_ASPECT_RATIO_X * inv_camera_zoom;
    result.y *= GAME_ASPECT_RATIO_Y * inv_camera_zoom;
    return result;
}

static inline v2 rotate_around(v2 p, v2 center, v2 rotation) {
    v2 rel = p - center;
    v2 result = v2_complex_prod(rel, rotation);
    return (p + result);
}

static void fill_poly(const v2s * const pts, u32 pt_count, u8 * const texture, u32 tex_width, u8 value, s32 ymin, s32 ymax) {
    v2s a = pts[0];
    v2s b;
    s16 min_xs[32];
    s16 max_xs[32];
    
    const s32 line_count = ymax - ymin + 1;
    
    a = pts[pt_count - 1];
    for(u32 i = 0; i < pt_count; ++i) {
        b = pts[i];
        
        bool down = true;
        s16 *xs = min_xs;
        
        s16 ax = (s16)(a.x);
        s16 ay = (s16)(a.y);
        s16 bx = (s16)(b.x);
        s16 by = (s16)(b.y);
        
        s16 dy = by - ay;
        
        // Our polys are convex and not degenerate, so we only need to take care of vertical edges
        // to fill the shape.
        if(dy) {
            if(dy > 0) {
                down = false;
                xs = max_xs;
                dy = -dy;
                
                s16 xswap = ax;
                ax = bx;
                bx = xswap;
                
                s16 yswap = ay;
                ay = by;
                by = yswap;
            }
            
            bool start_mark = down;
            
            s16 dx = bx - ax;
            s16 xinc = 1;
            if(dx < 0) {
                start_mark = !start_mark;
                dx = -dx;
                xinc = -xinc;
            }
            
            s16 at_x = ax;
            s16 at_y = ay;
            
            // @Robustness: In here I use >> 1 instead of / 2. The only difference is that
            // / 2 potentially nullifies the LSB, but I haven't seen any reason to do so!
            s16 error = dx + dy;
            if(start_mark) {
                for(;;) {
                    xs[at_y - ymin] = at_x;
                    
                    // In the case where (error < dy/2), the line is going down
                    // enough so that we don't need to compute an x advance.
                    if(error >= (dy >> 1)) {
                        s16 loop_backsolve = (((dx >> 1) - error) / dy) + 1;
                        at_x += xinc * loop_backsolve;
                        error += dy * loop_backsolve;
                    }
                    
                    error += dx;
                    --at_y;
                    if(at_y > by) {
                        continue;
                    }
                    xs[at_y - ymin] = at_x;
                    break;
                }
            } else {
                for(;;) {
                    if(error >= (dy >> 1)) {
                        s16 loop_backsolve = (((dx >> 1) - error) / dy) + 1;
                        at_x += xinc * loop_backsolve;
                        error += dy * loop_backsolve;
                        
                        // at_x is now the start of the next scanline, so
                        // we need to offset the pixel write by 1.
                        xs[at_y - ymin] = at_x - xinc;
                    } else {
                        xs[at_y - ymin] = at_x;
                    }
                    error += dx;
                    
                    --at_y;
                    if(at_y > by) {
                        continue;
                    }
                    xs[at_y - ymin] = bx;
                    break;
                }
            }
        }
        a = b;
    }
    
    s32 at_y = ymax;
    for(s32 si = 0; si < line_count; ++si) {
        s16 xmin = min_xs[si];
        s16 xmax = max_xs[si];
        u8 *scan = texture + at_y * tex_width + xmin;
        s16 scan_width = xmax - xmin + 1;
        
        for(s16 ix = 0; ix < scan_width; ++ix) {
            scan[ix] = value;
        }
        
        --at_y;
    }
}

inline void init_bresenham(s32 a_x, s32 a_y, s32 b_x, s32 b_y, s32 *out_at_x, s32 *out_at_y,
                           s32 *out_end_x, s32 *out_end_y, s32 *out_dx, s32 *out_dy, s32 *out_xinc,
                           s32 *out_yinc, s32 *out_deviation) {
    s32 at_x, at_y, end_x, end_y;
    s32 dx = b_x - a_x;
    s32 dy = b_y - a_y;
    s32 xinc = 1;
    s32 yinc = -1;
    
    if(dy > 0) {
        dy = -dy;
        yinc = 1;
    }
    
    at_x = a_x;
    at_y = a_y;
    end_x = b_x;
    end_y = b_y;
    
    if(dx < 0) {
        dx = -dx;
        xinc = -1;
    }
    
    *out_at_x = at_x;
    *out_at_y = at_y;
    *out_end_x = end_x;
    *out_end_y = end_y;
    *out_dx = dx;
    *out_dy = dy;
    *out_xinc = xinc;
    *out_yinc = yinc;
    *out_deviation = dx + dy;
}

// dx is assumed positive, dy is assumed negative.
inline void do_bresenham(s32 *at_x, s32 *at_y, s32 *deviation, s32 dx, s32 dy, s32 xinc, s32 yinc) {
    // Doubling gives us the deviation for the next cell.
    s32 doubled_deviation = 2 * *deviation;
    if(doubled_deviation >= dy) {
        *deviation += dy;
        *at_x += xinc;
    }
    if(doubled_deviation <= dx) {
        *deviation += dx;
        *at_y += yinc;
    }
}

inline void update_player_dp(const f32 input_dp, f32 * const _dp, const f32 accel, const f32 max_speed, const f32 dt32) {
    f32 dp = *_dp;
    
    if(!input_dp) {
        const f32 absdp = f_abs(dp);
        f32 decay = (0.2f * dt32) / absdp;
        
        if(decay < 0.1f) {
            decay = 0.1f;
        } else if(decay > 1.0f) {
            decay = 1.0f;
        }
        
        dp *= 1.0f - decay;
    }
    dp += input_dp * accel;
    dp = f_symmetrical_clamp(dp, max_speed);
    
    *_dp = dp;
}

inline void ray_to_outer_box_intersect(v2 ray0, v2 dir, f32 left, f32 bottom, f32 right, f32 top, v2 *intersect) {
    f32 tx, ty;
    
    if(dir.x <= 0.0f) {
        tx = (left - ray0.x) / dir.x;
    } else {
        tx = (right - ray0.x) / dir.x;
    }
    if(dir.y <= 0.0f) {
        ty = (bottom - ray0.y) / dir.y;
    } else {
        ty = (top - ray0.y) / dir.y;
    }
    
    *intersect = ray0 + dir * f_min(tx, ty);
}

inline bool aabb_aabb_overlap(v2 pa, v2 halfdima, v2 pb, v2 halfdimb) {
    v2 minkowski_p = pa - pb;
    v2 minkowski_halfdim = halfdima + halfdimb;
    
    f32 left = minkowski_p.x - minkowski_halfdim.x;
    f32 right = minkowski_p.x + minkowski_halfdim.x;
    f32 bottom = minkowski_p.y - minkowski_halfdim.y;
    f32 top = minkowski_p.y + minkowski_halfdim.y;
    
    return ((left < 0.0f) && (right > 0.0f) &&
            (bottom < 0.0f) && (top > 0.0f));
}

inline v2 aabb_aabb_response(v2 pa, v2 halfdima, v2 pb, v2 halfdimb) {
    f32 flipy = 1.0f;
    f32 flipx = 1.0f;
    f32 disty = pa.y - pb.y;
    f32 distx = pa.x - pb.x;
    
    if(disty < 0.0f) {
        disty = -disty;
        flipy = -1.0f;
    }
    if(distx < 0.0f) {
        distx = -distx;
        flipx = -1.0f;
    }
    
    f32 sepy = disty - halfdima.y - halfdimb.y;
    f32 sepx = distx - halfdima.x - halfdimb.x;
    
    if((sepy < 0.0f) && (sepx < 0.0f)) {
        if(sepx < sepy) {
            return V2(0.0f, sepy * flipy);
        } else {
            return V2(sepx * flipx, 0.0f);
        }
    } else {
        return V2();
    }
}

inline v2 obb_closest(const v2 p, const v2 halfdim, const v2 rotation, const v2 to) {
    const v2 rot_perp = v2_perp(rotation);
    const v2 dp = to - p;
    f32 dotx = v2_inner(dp, rotation);
    f32 doty = v2_inner(dp, rot_perp);
    dotx = f_symmetrical_clamp(dotx, halfdim.x);
    doty = f_symmetrical_clamp(doty, halfdim.y);
    const v2 closest = p + rotation * dotx + rot_perp * doty;
    return closest;
}

inline bool disk_obb_overlap_radius_squared(const v2 disk_p, const f32 radius_sq, const v2 obb_p, const v2 halfdim, const v2 rotation) {
    const v2 closest = obb_closest(obb_p, halfdim, rotation, disk_p);
    const f32 dist_sq = v2_length_sq(disk_p - closest);
    return (dist_sq < radius_sq);
}

inline bool disk_obb_overlap(const v2 disk_p, const f32 radius, const v2 obb_p, const v2 halfdim, const v2 rotation) {
    return disk_obb_overlap_radius_squared(disk_p, radius * radius, obb_p, halfdim, rotation);
}

inline bool disk_disk_overlap(const v2 pa, const f32 radiusa, const v2 pb, const f32 radiusb) {
    const f32 dist_sq = v2_length_sq(pa - pb);
    const f32 minkowski_rad = radiusa + radiusb;
    const f32 radius_sq = minkowski_rad * minkowski_rad;
    return (dist_sq < radius_sq);
}

static void update_ballistic_dp(f32 *_dp, Input in, u32 minus_button, u32 plus_button, const f32 dt32,
                                const f32 accel, const f32 decay_max, const f32 decay_factor) {
    f32 dp = *_dp;
    f32 in_dp = 0.0f;
    
    if(button_down(in, minus_button)) {
        in_dp -= 1.0f;
    }
    if(button_down(in, plus_button)) {
        in_dp += 1.0f;
    }
    
    dp += in_dp * accel * dt32;
    
    f32 speed = f_abs(dp);
    f32 decay = f_max(speed, decay_max) * decay_factor * dt32;
    if(decay >= speed) {
        dp = 0.0f;
    } else {
        f32 factor = (speed - decay) / speed;
        dp *= factor;
    }
    
    *_dp = dp;
}

static void mark_entity_for_removal(Game *g, u8 type, s32 index) {
    // Checking that we don't mark the same entity multiple times. @Speed?
    FORI(0, g->removals.count) {
        if((g->removals.types[i] == type) && (g->removals.indices[i] == index)) {
            return;
        }
    }
    
    s32 insert = g->removals.count;
    ASSERT(insert < MAX_LIVE_ENTITY_COUNT);
    
    g->removals.types[insert] = type;
    g->removals.indices[insert] = index;
    
    g->removals.count += 1;
}

static s32 add_bullet(Game *g, v2 p, v2 direction, f32 speed, bool explodes = false) {
    const s32 insert = g->bullets.count;
    ASSERT(insert < MAX_LIVE_ENTITY_COUNT);
    
    g->bullets.ps                [insert] = p;
    g->bullets.directions        [insert] = direction;
    g->bullets.speeds            [insert] = speed;
    g->bullets.explodes_on_impact[insert] = explodes;
    
    g->bullets.count += 1;
    return insert;
}

static v2 closest_in_array(v2 from, v2 *array, s32 count, f32 *out_dist_sq = 0) {
    v2 result = from;
    
    if(count) {
        result = array[0];
        f32 best = v2_length_sq(result - from);
        
        FORI(0, count) {
            const v2 it = array[i];
            const f32 dist_sq = v2_length_sq(it - from);
            
            if(dist_sq < best) {
                best = dist_sq;
                result = it;
            }
        }
        
        if(out_dist_sq) {
            *out_dist_sq = best;
        }
    }
    
    return result;
}

static v2 closest_in_array_with_direction(v2 from, v2 *array, s32 count, f32 *out_dist_sq, v2 *direction) {
    ASSERT(out_dist_sq && direction);
    v2 result = closest_in_array(from, array, count, out_dist_sq);
    
    *direction = (result - from) / f_sqrt(*out_dist_sq);
    
    return result;
}

#if 0
struct Game_Level {
    s32 index;
    
    v2 map_halfdim;
    // Some list of associated assets.
    
    Any_Entity_Array entities[];
    // ie.
    s16 entity_type;
    s16 entity_count;
    // with type-specific fields right after this; SOA format?
    // Entities can/should be serialised, ie.
    
    struct {
        s32   count;
        // v2    camera_ps        [     MAX_PLAYER_COUNT];
        // f32   camera_zooms     [     MAX_PLAYER_COUNT];
        // v2    dps              [     MAX_PLAYER_COUNT];
        v2    ps               [     MAX_PLAYER_COUNT];
        // v2    xhair_offsets    [     MAX_PLAYER_COUNT];
        s32   partner_counts   [     MAX_PLAYER_COUNT];
        s32   partners         [     MAX_PLAYER_COUNT][MAX_PARTNERS_PER_PLAYER];
    } players;
};
#endif

static usize layout_serialised_level_data(Serialised_Level_Data *level, Level_Header *header) {
    return 0;
}

void Implicit_Context::g_full_update(Game *g, Input *_in, const f64 dt,
                                     Audio_Info *audio, Datapack_Handle *datapack) {
    TIME_BLOCK;
    const f32 dt32 = (f32)dt;
    Input in = *_in;
    
    //
    // Init:
    //
#if GENESIS_DEV
    if(g->godmode) {
        g->lost = false;
    }
#endif
    
    if((g->lost || !g->current_level_index) && datapack) {
#if 0
        u32 new_level_index = 1;
        bool fresh_level = g->current_level_index != new_level_index;
        
        *g = {};
        g->lost = false;
        
        // Level load:
        g->current_level_index = new_level_index;
        g->camera_zoom = 1.0f;
        
        g->players.count = 1;
        
        g->partners.count = 4;
        g->partners.offsets[0] = V2(0.0f, 1.0f);
        g->partners.offsets[1] = V2(-1.0f, 0.0f);
        g->partners.offsets[2] = V2(0.0f, -1.0f);
        g->partners.offsets[3] = V2(1.0f, 0.0f);
        
        g->trees.count = 3;
        g->trees.ps[0] = V2(-1.0f);
        g->trees.ps[1] = V2(-1.3f,  1.0f);
        g->trees.ps[2] = V2( 3.0f, -0.5f);
        
#if 1
        g->turrets.count = 1;
        g->turrets.ps       [0] = V2(0.0f, 2.0f);
        g->turrets.cooldowns[0] = 1.0f;
#endif
        
        g->dogs.count = 1;
        g->dogs.ps[0] = V2(2.0f);
        
#if 1
        g->wall_turrets.count = 1;
        g->wall_turrets.ps[0] = V2(-1.0f);
#endif
        
        g->map_halfdim = V2(TEST_MAP_WIDTH / 2.0f, TEST_MAP_HEIGHT / 2.0f);
        
        
        // Associated assets:
        // We only do this when loading a new level, not when resetting the current one.
        // :AudioInVisualState @Cleanup
        if(fresh_level && datapack) {
            // @Incomplete: Clear all sounds?
            LOAD_SOUND(audio, datapack, erase);
        }
#else
        s32 desired_level = 1; //g->desired_level_index;
        *g = {}; // @Cleanup @Speed
        
        g->desired_level_index = desired_level;
        
        g->camera_zoom = 1.0f;
        
        // @Incomplete: get_level(&temporary_memory, datapack, desired_level)
        string asset = read_entire_asset(&temporary_memory, datapack, ASSET_UID_1_lvl);
        Serialised_Level_Data serialised;
        
        serialised.header = (Level_Header *)asset.data;
        serialised.player_ps = (v2 *)(serialised.header + 1);
        serialised.partner_offsets = (v2 *)(serialised.player_ps + serialised.header->player_count);
        serialised.turret_ps = (v2 *)(serialised.partner_offsets + serialised.header->partner_count);
        serialised.turret_cooldowns = (f32 *)(serialised.turret_ps + serialised.header->turret_count);
        serialised.dog_ps = (v2 *)(serialised.turret_cooldowns + serialised.header->turret_count);
        serialised.wall_turret_ps = (v2 *)(serialised.dog_ps + serialised.header->dog_count);
        serialised.target_ps = (v2 *)(serialised.wall_turret_ps + serialised.header->wall_turret_count);
        
        ASSERT(serialised.header->version == 1);
        
        g->current_level_index = serialised.header->index;
        g->map_halfdim = serialised.header->map_halfdim;
        {
            s32 count = serialised.header->player_count;
            FORI(0, count) {
                g->players.ps[i] = serialised.player_ps[i];
            }
            g->players.count = count;
        }
        {
            s32 count = serialised.header->partner_count;
            FORI(0, count) {
                g->partners.offsets[i] = serialised.partner_offsets[i];
            }
            g->partners.count = count;
        }
        {
            s32 count = serialised.header->turret_count;
            FORI(0, count) {
                g->turrets.ps[i] = serialised.turret_ps[i];
                g->turrets.cooldowns[i] = serialised.turret_cooldowns[i];
            }
            g->turrets.count = count;
        }
        {
            s32 count = serialised.header->dog_count;
            FORI(0, count) {
                g->dogs.ps[i] = serialised.dog_ps[i];
            }
            g->dogs.count = count;
        }
        {
            s32 count = serialised.header->wall_turret_count;
            FORI(0, count) {
                g->wall_turrets.ps[i] = serialised.wall_turret_ps[i];
            }
            g->wall_turrets.count = count;
        }
        {
            s32 count = serialised.header->target_count;
            FORI(0, count) {
                g->targets.ps[i] = serialised.target_ps[i];
            }
            g->targets.count = count;
        }
        
#endif
    }
    
    //
    // At this point, level and entity data should be valid.
    //
    
    // Map constants:
    const v2 map_dim = g->map_halfdim * 2.0f;
    const v2 map_halfdim = g->map_halfdim;
    
    //
    // :ControlMovementSeparation
    // @Refactor: So far, we've elected to separate "AI", ie. non-movement updates,
    // from movement updates. Should we keep this structure or should we merge the
    // two?
    // We most likely want to see how our collision system turns out in order to be
    // able to answer this confidently, anyhow.
    // Separating the two makes our program more predictable, since we know every entity will
    // always be "seeing" every other entity's last frame position, and not some random mix of
    // last frame and current frame data.
    //
    
    { // Player controls.
        {
            f32 camera_zoom = g->camera_zoom;
            if(BUTTON_DOWN(in, slot1)) {
                camera_zoom = 1.0f;
            } else if(BUTTON_DOWN(in, slot2)) {
                camera_zoom = 0.5f;
            } else if(BUTTON_DOWN(in, slot3)) {
                camera_zoom = 2.0f;
            } else if(BUTTON_DOWN(in, slot4)) {
                camera_zoom = 1.0f;
            }
            
            g->camera_zoom = camera_zoom;
            g->xhair_offset = unit_scale_to_world_space_offset(in.xhairp, camera_zoom);
        }
        
        // @Incomplete: Choose between xhair_offset and xhair_position to resolve multi-player
        // aiming.
        const v2 xhair_offset = g->xhair_offset;
        const s32 count = g->players.count;
        for(s32 iplayer = 0; iplayer < count; ++iplayer) {
            v2 p = g->players.ps[iplayer];
            
            { // Movement.
                v2 dp = g->players.dps[iplayer];
                
                update_ballistic_dp(&dp.x, in, BUTTON_FLAG(left), BUTTON_FLAG(right), dt32,
                                    80.0f, 0.5f, 10.0f);
                update_ballistic_dp(&dp.y, in, BUTTON_FLAG(down), BUTTON_FLAG(up)   , dt32,
                                    80.0f, 0.5f, 10.0f);
                
                g->players.dps[iplayer] = dp;
            }
            
            { // Action.
                if(BUTTON_PRESSED(&in, attack)) {
                    const f32 spawn_dist = (BULLET_RADIUS + PLAYER_HALFHEIGHT + PROJECTILE_SPAWN_DIST_EPSILON);
                    
                    FORI_NAMED(_partner, 0, g->partners.count) {
                        v2 partner_offset = g->partners.offsets[_partner];
                        v2 dir = v2_normalize(xhair_offset + partner_offset);
                        
                        v2 spawn_offset = dir * spawn_dist;
                        
                        add_bullet(g, p + spawn_offset, dir, 10.0f);
                        
                        // :AudioInVisualState We don't want to think about whether we're in a visual or an actual update.
                        if(audio) {
                            Audio_Play_Commands *commands = play_sound(audio, FIND_SOUND_ID(audio, erase));
                            if(commands) {
                                u8 channel_count = audio->channel_count;
                                compute_volumes_for_position(commands->volume, channel_count, spawn_offset);
                                mem_copy(commands->volume, commands->old_volume, sizeof(f32) * channel_count);
                            }
                        }
                    }
                }
            }
        }
    }
    
    { // Turret control.
        const s32 count = g->turrets.count;
        FORI(0, count) {
            f32 cooldown = g->turrets.cooldowns[i];
            const v2 p = g->turrets.ps[i];
            
            if(cooldown <= 0.0f) {
                const f32 spawn_dist = (BULLET_RADIUS + f_sqrt(TURRET_HALFWIDTH*TURRET_HALFWIDTH + TURRET_HALFHEIGHT*TURRET_HALFHEIGHT) + PROJECTILE_SPAWN_DIST_EPSILON);
                f32 dist_sq;
                v2 direction;
                v2 target_p = closest_in_array_with_direction(p, g->players.ps, g->players.count, &dist_sq, &direction);
                
                {
                    f32 other_dist_sq;
                    v2 other_direction;
                    v2 partner_p = closest_in_array_with_direction(p, g->partners.ps, g->partners.count, &other_dist_sq, &other_direction);
                    
                    if(other_dist_sq < dist_sq) {
                        target_p = partner_p;
                        direction = other_direction;
                    }
                }
                
                v2 spawn_p = p + direction * spawn_dist;
                
                add_bullet(g, spawn_p, direction, 2.0f);
                
                cooldown = 1.0f;
            }
            
            cooldown -= dt32;
            
            g->turrets.cooldowns[i] = cooldown;
        }
    }
    
    { // Dog control.
        const s32 count = g->dogs.count;
        FORI(0, count) {
            const f32 speed = 10.0f;
            v2 p       = g->dogs.ps [i];
            v2 dp      = g->dogs.dps[i];
            
            f32 dist_to_target_sq;
            v2 direction;
            v2 target_p = closest_in_array_with_direction(p, g->partners.ps, g->partners.count, &dist_to_target_sq, &direction);
            v2 ddp = direction * speed;
            
            { // Compensate for our own momentum:
                v2 dir_perp = v2_perp(direction);
                f32 perp_dot = v2_inner(dp, dir_perp);
                if(perp_dot > 0.1f) {
                    ddp -= dir_perp;
                } else if(perp_dot < -0.1f) {
                    ddp += dir_perp;
                }
            }
            
            { // Readjust our trajectory if we've passed the target:
                if(v2_inner(direction, dp) < 0.0f) {
                    dp *= 0.99f;
                    ddp += -dp * 0.5f;
                }
            }
            
            { // Cap our speed:
                f32 current_speed = v2_length(dp);
                if(current_speed > 10.0f) {
                    dp = dp / current_speed * 10.0f;
                }
            }
            
            dp += ddp * dt32;
            
            g->dogs.dps[i] = dp;
        }
    }
    
    { // Bullet control.
        s32 count = g->bullets.count;
        
        FORI_TYPED(s32, 0, count) {
            const v2 p = g->bullets.ps[i];
            
            if((p.x < -map_halfdim.x) || (p.x >= map_halfdim.x) ||
               (p.y < -map_halfdim.y) || (p.y >= map_halfdim.y)) {
                mark_entity_for_removal(g, ENTITY_TYPE_BULLET, i);
            }
        }
        
        g->bullets.count = count;
    }
    
    { // Player movement.
        const s32 count = g->players.count;
        FORI(0, count) {
            v2 p = g->players.ps[i];
            v2 dp = g->players.dps[i];
            
            p += dp * dt32;
            
            g->players.ps[i] = p;
        }
    }
    
    { // Bullet movement.
        const s32 count = g->bullets.count;
        FORI(0, count) {
            v2 p      = g->bullets.ps         [i];
            v2 dir    = g->bullets.directions [i];
            f32 speed = g->bullets.speeds     [i];
            
            p += dir * speed * dt32;
            
            g->bullets.ps[i] = p;
        }
    }
    
    { // Dog movement.
        const s32 count = g->dogs.count;
        
        FORI(0, count) {
            g->dogs.ps[i] += g->dogs.dps[i] * dt32;
        }
    }
    
    //
    // At this point, we should have emitted every move.
    //
    // Check for solid collisions.
    
    // Trying a "gather in buckets" approach.
    // The advantage is that buckets share some attributes, which reduces branching,
    // but can still be treated homogeneously.
    // HOWEVER, some passes would really rather have the ability to specify which
    // arrays they want (notably partner/player).
    
    // Entities with disk collision.
    const u8 disk_entity_types_for_arrays[] = {
        ENTITY_TYPE_DOG,
    };
    const s32      disk_counts_for_arrays[] = {
        g->dogs.count,
    };
    v2              * const disk_p_arrays[] = {
        g->dogs.ps,
    };
    const f32       disk_radii_for_arrays[] = {
        DOG_RADIUS,
    };
    
    // Entities with box collision.
    const u8 obb_entity_types_for_arrays[] = {
        ENTITY_TYPE_PLAYER,
        ENTITY_TYPE_PARTNER,
        ENTITY_TYPE_TURRET,
        ENTITY_TYPE_WALL_TURRET,
    };
    const s32      obb_counts_for_arrays[] = {
        g->     players.count,
        g->    partners.count,
        g->     turrets.count,
        g->wall_turrets.count,
    };
    v2              * const obb_p_arrays[] = {
        g->     players.ps,
        g->    partners.ps,
        g->     turrets.ps,
        g->wall_turrets.ps,
    };
    const v2     obb_halfdims_for_arrays[] = {
        V2(     PLAYER_HALFWIDTH,      PLAYER_HALFHEIGHT),
        V2(    PARTNER_HALFWIDTH,     PARTNER_HALFHEIGHT),
        V2(     TURRET_HALFWIDTH,      TURRET_HALFHEIGHT),
        V2(WALL_TURRET_HALFWIDTH, WALL_TURRET_HALFHEIGHT),
    };
    
    { // Enforcing map bounds.
        FORI_TYPED(u32, 0, ARRAY_LENGTH(disk_entity_types_for_arrays)) {
            v2 *ps = disk_p_arrays[i];
            s32 count = disk_counts_for_arrays[i];
            f32 radius = disk_radii_for_arrays[i];
            v2 minkowski_map_halfdim = map_halfdim - V2(radius);
            
            FORI_NAMED(typed, 0, count) {
                v2 p = ps[typed];
                
                p.x = f_symmetrical_clamp(p.x, minkowski_map_halfdim.x);
                p.y = f_symmetrical_clamp(p.y, minkowski_map_halfdim.y);
                
                ps[typed] = p;
            }
        }
        
        FORI_TYPED(u32, 0, ARRAY_LENGTH(obb_entity_types_for_arrays)) {
            v2 *ps = obb_p_arrays[i];
            s32 count = obb_counts_for_arrays[i];
            v2 halfdim = obb_halfdims_for_arrays[i];
            v2 minkowski_map_halfdim = map_halfdim - halfdim;
            
            FORI_NAMED(typed, 0, count) {
                v2 p = ps[typed];
                
                p.x = f_symmetrical_clamp(p.x, minkowski_map_halfdim.x);
                p.y = f_symmetrical_clamp(p.y, minkowski_map_halfdim.y);
                
                ps[typed] = p;
            }
        }
    }
    
    //
    // At this point, every solid has found its final position for the frame.
    //
    // Emit overlap queries.
    
    { // Snapping partners in position.
        // The partner P is a direct snap.
        // However, we wait until after the collision resolve to do it because we want to make sure the position
        // is correct, which it wouldn't be if we snapped and the player ended up hitting a wall and getting nudged out of it.
        v2 player_p = g->players.ps[g->current_player];
        v2 xhair_offset = g->xhair_offset;
        v2 xhair_p = xhair_offset + player_p;
        FORI(0, g->partners.count) {
            g->partners.ps[i] = xhair_p + g->partners.offsets[i];
        }
    }
    
    { // Bullet overlaps.
        const s32 bullets = g->bullets.count;
        
        FORI_TYPED_NAMED(s32, ib, 0, bullets) {
            const v2   bp                 = g->bullets.ps[ib];
            const bool explodes_on_impact = g->bullets.explodes_on_impact[ib];
            f32 radius           = BULLET_RADIUS;
            f32 explosion_radius = BULLET_EXPLOSION_RADIUS;
            bool hit_any = false;
            
            bullet_overlap_begin:
            FORI_TYPED(u32, 0, ARRAY_LENGTH(disk_entity_types_for_arrays)) {
                v2 *typed_ps = disk_p_arrays[i];
                const f32 typed_radius = disk_radii_for_arrays[i];
                const u8 type = disk_entity_types_for_arrays[i];
                const s32 count = disk_counts_for_arrays[i];
                
                FORI_TYPED_NAMED(s32, typed, 0, count) {
                    if(!disk_disk_overlap(bp, radius, typed_ps[typed], typed_radius)) {
                        continue;
                    }
                    
                    // We've detected an overlap.
                    
                    // @Speed @Hack?!? We probably want to handle explosions separately.
                    if((radius != explosion_radius) && explodes_on_impact) {
                        radius = explosion_radius;
                        goto bullet_overlap_begin;
                    }
                    
                    hit_any = true;
                    mark_entity_for_removal(g, type, typed);
                }
            }
            
            FORI_TYPED(u32, 0, ARRAY_LENGTH(obb_entity_types_for_arrays)) {
                v2 *typed_ps = obb_p_arrays[i];
                const v2 typed_halfdim = obb_halfdims_for_arrays[i];
                const u8 type = obb_entity_types_for_arrays[i];
                const s32 count = obb_counts_for_arrays[i];
                
                FORI_TYPED_NAMED(s32, typed, 0, count) {
                    if(!disk_obb_overlap(bp, radius, typed_ps[typed], typed_halfdim, V2(1.0f, 0.0f))) {
                        continue;
                    }
                    
                    // We've detected an overlap.
                    
                    // @Speed @Hack?!? We probably want to handle explosions separately.
                    if((radius != explosion_radius) && explodes_on_impact) {
                        radius = explosion_radius;
                        goto bullet_overlap_begin;
                    }
                    
                    hit_any = true;
                    mark_entity_for_removal(g, type, typed);
                }
            }
            
            if(hit_any) {
                mark_entity_for_removal(g, ENTITY_TYPE_BULLET, ib);
            }
        }
    }
    
    { // Dog overlaps.
        const s32 count = g->dogs.count;
        FORI(0, count) {
            v2 p = g->dogs.ps[i];
            
            FORI_TYPED_NAMED(s32, partner, 0, g->partners.count) {
                if(disk_obb_overlap(p, DOG_RADIUS * DOG_RADIUS, g->partners.ps[partner], V2(PARTNER_HALFWIDTH, PARTNER_HALFHEIGHT), V2(1.0f, 0.0f))) {
                    mark_entity_for_removal(g, ENTITY_TYPE_PARTNER, partner);
                }
            }
        }
    }
    
    { // Wall turret overlaps.
        const s32 count = g->wall_turrets.count;
        FORI(0, count) {
            v2 p = g->wall_turrets.ps[i];
            
            f32 dist_sq;
            v2 wall_direction;
            v2 target_p = closest_in_array_with_direction(p, g->players.ps, g->players.count, &dist_sq, &wall_direction);
            
            FORI_TYPED_NAMED(s32, partner, 0, g->partners.count) {
                v2 to_partner = g->partners.ps[partner] - p;
                
                f32 from_ray = f_abs(v2_inner(to_partner, v2_perp(wall_direction)));
                f32 towards_ray = v2_inner(to_partner, wall_direction);
                
                if((towards_ray > 0.0f) && (from_ray < (PARTNER_HALFWIDTH))) {
                    mark_entity_for_removal(g, ENTITY_TYPE_PARTNER, partner);
                }
            }
        }
    }
    
    //
    // At this point, all collision results should be available.
    //
    
    { // Processing entity deaths.
        // We're assuming that we don't have duplicate deaths.
        FORI(0, g->removals.count) {
            u8 type = g->removals.types[i];
            s32 index = g->removals.indices[i];
            
            switch(type) {
                case ENTITY_TYPE_PLAYER:
                case ENTITY_TYPE_PARTNER: {
                    g->lost = true;
                } break;
                
                case ENTITY_TYPE_BULLET: {
                    s32 replace = g->bullets.count - 1;
                    ASSERT(replace >= 0);
                    
                    g->bullets.ps                [index] = g->bullets.ps                [replace];
                    g->bullets.directions        [index] = g->bullets.directions        [replace];
                    g->bullets.speeds            [index] = g->bullets.speeds            [replace];
                    g->bullets.explodes_on_impact[index] = g->bullets.explodes_on_impact[replace];
                    
                    g->bullets.count -= 1;
                } break;
                
                case ENTITY_TYPE_TURRET: {
                    s32 replace = g->turrets.count - 1;
                    ASSERT(replace >= 0);
                    
                    g->turrets.ps       [index] = g->turrets.ps       [replace];
                    g->turrets.cooldowns[index] = g->turrets.cooldowns[replace];
                    
                    g->turrets.count -= 1;
                } break;
                
                case ENTITY_TYPE_DOG: {
                    s32 replace = g->dogs.count - 1;
                    ASSERT(replace >= 0);
                    
                    g->dogs.ps          [index] = g->dogs.ps          [replace];
                    g->dogs.dps         [index] = g->dogs.dps         [replace];
                    
                    g->dogs.count -= 1;
                } break;
                
                case ENTITY_TYPE_WALL_TURRET: {
                    s32 replace = g->wall_turrets.count - 1;
                    ASSERT(replace >= 0);
                    
                    g->wall_turrets.ps  [index] = g->wall_turrets.ps  [replace];
                    
                    g->wall_turrets.count -= 1;
                } break;
            }
            
            // Either way, we log it.
            // @Incomplete: Once we know what information we want out of this,
            // do something better than % when our queue is full.
            g->logged_removals.types[g->logged_removals.count++] = type;
            g->logged_removals.count %= MAX_LIVE_ENTITY_COUNT;
        }
        
        g->removals.count = 0;
    }
    
    if(!g->targets.count) {
        g->desired_level_index += 1;
    }
    
    //
    // Frame end
    //
    
    advance_input(_in);
}

THREAD_JOB_PROC(draw_game_threaded) {
#if 0 // @XXX
    // Begin
    auto payload = CAST(Draw_Game_Threaded_Payload*, data);
    auto render  = CAST(Render_Thread_Payload*, &payload->render);
    
    auto renderer = payload->renderer;
    
    s32 render_threads = renderer->core_count;
    
    s32 batch_index = 0;
    Command_Batch* current_batch = &render->command_batches[batch_index];
    Render_Vertex* vertex_buffer = CAST(Render_Vertex*, current_batch->list->vertex_buffer_memory);
    s32 vert_count = 0;
    s32 tri_count = 0;
    
    bool done = false;
    while(!done) {
        // Do some stuff
#if 0
        vertex_buffer[vert_count++] = make_render_vertex(V2(0.0f, 0.7f));
        vertex_buffer[vert_count++] = make_render_vertex(V2(-0.4f, -0.5f));
        vertex_buffer[vert_count++] = make_render_vertex(V2( 0.4f, -0.5f));
#endif
        
        done = true;
        
        if((vert_count == 3) || done) {
            // Queue up the batch.
            s32 queued = ATOMIC_INC32(CAST(volatile long*, &renderer->command_batches_queued));
            if(!(queued % render_threads)) { 
                // Flush.
                // This also increases some batch_index waitable value.
                
                // @Hardcoded
                os_platform.submit_commands(current_batch->list, &vert_count, 1, false);
                renderer->command_flushes += 1;
            }
            
            if(!done) {
                // In any case, flip our current batch.
                batch_index += 1;
                current_batch = &render->command_batches[batch_index & 1];
                vertex_buffer = CAST(Render_Vertex*, current_batch->list->vertex_buffer_memory);
                vert_count = 0;
                
                // We also need to wait for it to be free again, just in case we finish 2 batches
                // before some other thread gets to finish 1.
                
                // This is an example of doing this:
                // os_platform->wait_for_submitted_batches_to_be_this_value(batch_index - 1);
                // ... Where we would be waiting on an event.
                
                // This is another, admittedly dumb, way of doing this:
                // while(renderer->command_flushes < batch_index - 1) {
                //   ;
                // }
                // HOWEVER, events and waits are pretty heavyweight and probably slow, so they should
                // only be used in a hypothetical worst case scenario.
                // 
                // A smarter way of going about this, then, is probably to do this:
                // if(renderer->command_flushes < batch_index - 1) {
                //   os_platform->wait_for_submitted_batches_to_be_this_value(batch_index - 1);
                // }
                // Where, hopefully, in 99.9% of cases, we just jump over the event mess.
            }
        }
    }
#endif
}

void Implicit_Context::draw_game_single_threaded(Renderer *renderer, Game *g, Asset_Storage *assets) {
#if 0
    TIME_BLOCK;
    if(g->current_level_index) {
        const u32 focused_player = g->current_player;
        const v2 camera_p = g->players.ps[focused_player];
        v2 camera_dir = V2(1.0f, 0.0f);
        const f32 camera_zoom = g->camera_zoom;
        const v2 viewport_halfdim = V2(GAME_ASPECT_RATIO_X, GAME_ASPECT_RATIO_Y) * camera_zoom * 0.5f;
        const v2 map_halfdim = g->map_halfdim;
        const v2 map_dim = map_halfdim * 2.0f;
        
        const v4 anti_player_color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        const v4 anti_partner_color = V4(0.0f, 0.0f, 1.0f, 1.0f);
        const v4 anti_all_color = anti_player_color + anti_partner_color;
        
#if 0
        Texture_Handle blank_texture   = GET_TEXTURE_ID(renderer, blank);
        Texture_Handle player_texture  = GET_TEXTURE_ID(renderer, player);
        Texture_Handle hitbox_texture  = GET_TEXTURE_ID(renderer, default_hitbox);
        Texture_Handle hurtbox_texture = GET_TEXTURE_ID(renderer, default_hurtbox);
        Texture_Handle untexture       = GET_TEXTURE_ID(renderer, untextured);
        Texture_Handle solid_texture   = GET_TEXTURE_ID(renderer, default_solid);
#endif
        
        render_set_shader_texture_rgba(renderer);
        render_set_transform_game_camera(renderer, camera_p, camera_dir, camera_zoom);
        
        { // Players
            const v4 color = V4(0.0f, 1.0f, 1.0f, 1.0f);
            const u32 count = g->players.count;
            
            for(u32 i = 0; i < count; ++i) {
                const v2 p = g->players.ps[i];
                const v2 halfdim = V2(PLAYER_HALFWIDTH, PLAYER_HALFHEIGHT);
                
                render_quad(renderer, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Partner
            const v4 color = V4(0.2f, 1.0f, 0.0f, 1.0f);
            
            FORI(0, g->partners.count) {
                const v2 p = g->partners.ps[i];
                const v2 halfdim = V2(PARTNER_HALFWIDTH, PARTNER_HALFHEIGHT);
                
                render_quad(renderer, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Trees
            const v4 color = V4(0.0f, 0.1f, 0.0f, 1.0f);
            const s32 count = g->trees.count;
            
            FORI(0, count) {
                const v2 p = g->trees.ps[i];
                const v2 halfdim = V2(TREE_HALFWIDTH, TREE_HALFHEIGHT);
                
                render_quad(renderer, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Turrets
            const v4 color = V4(0.3f, 0.1f, 0.1f, 1.0f);
            const s32 count = g->turrets.count;
            
            FORI(0, count) {
                const v2 p = g->turrets.ps[i];
                const v2 halfdim = V2(TURRET_HALFWIDTH, TURRET_HALFHEIGHT);
                
                render_quad(renderer, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Bullets
            const v4 color = anti_all_color;
            const s32 count = g->bullets.count;
            
            FORI(0, count) {
                const v2 p = g->bullets.ps[i];
                
                render_circle_prad(renderer, p, BULLET_RADIUS, color);
            }
        }
        
        { // Dogs
            const v4 color = anti_partner_color;
            const s32 count = g->dogs.count;
            
            FORI(0, count) {
                const v2 p = g->dogs.ps[i];
                
                render_circle_prad(renderer, p, DOG_RADIUS, color);
            }
        }
        
        { // Wall turrets.
            const v4 color = V4(0.1f, 0.1f, 0.3f, 1.0f);
            const s32 count = g->wall_turrets.count;
            
            
            FORI(0, count) {
                const v2 p = g->wall_turrets.ps[i];
                const v2 halfdim = V2(WALL_TURRET_HALFWIDTH, WALL_TURRET_HALFHEIGHT);
                
                // @Speed: We're already computing this in full_update.
                f32 dist_sq;
                v2 wall_dir;
                const v2 target_p = closest_in_array_with_direction(p, g->players.ps, g->players.count, &dist_sq, &wall_dir);
                
                { // Drawing the partner wall.
                    // @Incomplete: We're drawing the ray from the turret, so going far enough away from it makes it so we can see
                    // the end of it.
                    // Right now, we don't care because our test map is simple and because we might add an actual maximum length
                    // for the wall anyway.
                    f32 ray_length = f_sqrt(map_dim.x*map_dim.x + map_dim.y*map_dim.y);
                    v2 pts[] = {
                        p,
                        p + wall_dir * ray_length,
                    };
                    queue_line_strip(renderer, pts, ARRAY_LENGTH(pts), 0.01f, anti_partner_color);
                }
                
                queue_quad(renderer, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
    }
#endif
}

string Implicit_Context::tprint(char *literal, ...) {
    char *varargs;
    BEGIN_VARARG(varargs, literal);
    u8 *buffer = temporary_memory.mem + temporary_memory.used;
    string result;
    result.data = buffer;
    result.length = _print((char *)buffer, temporary_memory.size - temporary_memory.used, literal, varargs);
    temporary_memory.used += result.length;
    
    return result;
}

THREAD_JOB_PROC(test_thread_job) {
    TIME_BLOCK;
    
    string jobstr = tprint("Test job %d, thread %d.\n", (u64)data, thread_index);
    
    os_platform.print(jobstr);
}

static s32 *triangulate_polygon(Memory_Block *block, v2 *verts, s32 vert_count) {
    s32 *vert_indices = push_array(block, s32, (vert_count - 2) * 3);
    
    // Dumb convex fan for now.
    s32 i0 = 0;
    s32 i1 = 1;
    s32 index_count = 0;
    FORI_TYPED(s32, 2, vert_count) {
        vert_indices[index_count++] = i0;
        vert_indices[index_count++] = i1;
        vert_indices[index_count++] = i;
        
        i1 = i;
    }
    
    return vert_indices;
}

GAME_INIT_MEMORY(Implicit_Context::g_init_mem) {
    TIME_BLOCK;
    
    const s32 core_count = program_state->core_count;
    
    Game_Block_Info * const info = push_struct(game_block, Game_Block_Info);
    info->log.temporary_buffer_size = 8192;
    info->log.temporary_buffer.data = push_array(game_block, u8, info->log.temporary_buffer_size);
    info->log.temporary_buffer.length = 0;
    info->log.heads_up_buffer_size = 64;
    info->log.heads_up_buffer.data = push_array(game_block, u8, info->log.heads_up_buffer_size);
    info->log.heads_up_buffer.length = 0;
    
    // @Duplication with g_run_frame.
    global_log = &info->log;
    
    Game_Client    * const        g_cl = &info->client;
    
    Renderer       * const    renderer = &info->renderer;
    Audio_Info     * const  audio_info = &info->audio;
    Text_Info      * const   text_info = &info->text;
    Synth          * const       synth = &info->synth;
    
    // Memory partitioning:
    // sub_block(&renderer->render_queue, game_block, KiB(128));
    sub_block(&text_info->font_block    , game_block, MiB(  4));
    
    { // Loading the config file.
        bool need_to_load_default_config = true;
        
        string config_file = os_platform.read_entire_file(STRING("main.config"));
        if(config_file.length) {
            // Load the file's settings.
            if(config_file.length == sizeof(User_Config)) {
                User_Config *config = (User_Config *)config_file.data;
                
                if(config->version == USER_CONFIG_VERSION) {
                    logprint(global_log, STRING("Correct version config found. Loading."));
                    program_state->should_be_fullscreen = config->fullscreen;
                    program_state->window_size = V2S(config->window_dim);
                    program_state->input_settings = config->input;
                    audio_info->current_volume    = config->volume;
                    
                    need_to_load_default_config = false;
                } else {
                    string s = tprint("Outdated config version (%d, expected %d). Ignoring.", config->version, USER_CONFIG_VERSION);
                    logprint(global_log, s);
                }
            } else {
                logprint(global_log, STRING("Config file was of incorrect length. Ignoring."));
            }
        }
        
        if(need_to_load_default_config) {
            { // Controls.
                BIND_BUTTON(program_state->input_settings.bindings,   slot1,          1);
                BIND_BUTTON(program_state->input_settings.bindings,   slot2,          2);
                BIND_BUTTON(program_state->input_settings.bindings,   slot3,          3);
                BIND_BUTTON(program_state->input_settings.bindings,   slot4,          4);
                BIND_BUTTON(program_state->input_settings.bindings,  attack,  LEFTCLICK);
                BIND_BUTTON(program_state->input_settings.bindings, attack2, RIGHTCLICK);
                BIND_BUTTON(program_state->input_settings.bindings,      up,          W);
                BIND_BUTTON(program_state->input_settings.bindings,    down,          S);
                BIND_BUTTON(program_state->input_settings.bindings,    left,          A);
                BIND_BUTTON(program_state->input_settings.bindings,   right,          D);
                BIND_BUTTON(program_state->input_settings.bindings,    menu,     ESCAPE);
                BIND_BUTTON(program_state->input_settings.bindings,    jump,   SPACEBAR);
                BIND_BUTTON(program_state->input_settings.bindings,  editor,        TAB);
                BIND_BUTTON(program_state->input_settings.bindings, profiler,     LCTRL);
                
                program_state->input_settings.mouse_sensitivity = 0.001f;
            }
            
            { // Audio config.
                audio_info->current_volume.master                            = 0.5f;
                audio_info->current_volume.by_category[AUDIO_CATEGORY_SFX  ] = 1.0f;
                audio_info->current_volume.by_category[AUDIO_CATEGORY_MUSIC] = 1.0f;
            }
        }
    }
    
    // Audio.
    
    audio_init(audio_info, game_block, program_state->audio_output_rate, program_state->audio_output_channels, core_count);
    
    init_synth(synth, game_block, CAST(u8, core_count), CAST(u8, program_state->audio_output_channels), program_state->audio_output_rate);
    
    
#if GENESIS_DEV
    { // Profiler:
        Profiler profiler = {};
        
        // profile_init(&profiler, game_block, core_count);
        
        info->profiler = profiler;
        
#if GENESIS_DEV // @Duplication
        global_profiler = &info->profiler;
#endif
    }
#endif
    
    render_init(renderer, game_block, 1);
    
#if 0
    { // Text info:
        init(&text_info->glyph_hash, game_block);
        rect_pack_init(text_info->glyph_atlas_lines.lefts, text_info->glyph_atlas_lines.ys,
                       &text_info->glyph_atlas_lines.count, GLYPH_ATLAS_SIZE);
        u32 *zeroes = push_array(game_block, u32, GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE);
        text_info->glyph_atlas = gl_add_texture_linear_rgba(GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, zeroes);
    }
#endif
    
#if 0
    { // Reading a datapack:
        void *handle = os_platform.open_file(STRING("1.datapack"));
        usize length = os_platform.file_size(handle);
        
        usize header_size = sizeof(Datapack_Header) + sizeof(Datapack_Asset_Info) * (ASSET_UID_COUNT + 1);
        ASSERT(length > header_size);
        void *header_memory = push_size(game_block, header_size);
        os_platform.read_file(handle, header_memory, 0, header_size);
        Datapack_Header *header = (Datapack_Header *)header_memory;
        ASSERT(header->version == DATAPACK_VERSION);
        
        Datapack_Handle pack_handle;
        pack_handle.file_handle = handle;
        pack_handle.assets = (Datapack_Asset_Info *)(header + 1);
        g_cl->assets.datapack = pack_handle;
    }
#endif
    
#if 0
    { // Getting assets from the datapack:
        Datapack_Handle pack = g_cl->assets.datapack;
        
        { // TA:
            Asset_Metadata meta = get_asset_metadata(&pack, ASSET_UID_test_ta);
            
            string ta = push_string(game_block, meta.size);
            os_platform.read_file(meta.location.file_handle, ta.data, meta.location.offset, meta.size);
            
            ta_to_texture_ids(ta, renderer, renderer->textures_by_uid);
            
            renderer->blank_texture_id = FIND_TEXTURE_ID(renderer, blank);
            renderer->   untextured_id = FIND_TEXTURE_ID(renderer, untextured);
            renderer->       circle_id = FIND_TEXTURE_ID(renderer, circle);
        }
        
        { // FACS:
            s16 stereo = LOAD_MUSIC(audio_info, &pack, danse_macabre);
            Audio_Music_Commands *commands = play_music(audio_info, stereo);
            commands->volume = 1.0f;
        }
        
        { // Fonts:
            init_font(&info->text, &pack, FONT_MONO   , ASSET_UID_hack_ttf           );
            init_font(&info->text, &pack, FONT_FANCY  , ASSET_UID_charter_italic_ttf );
            init_font(&info->text, &pack, FONT_REGULAR, ASSET_UID_charter_regular_ttf);
        }
    }
#endif
    
    {
        g_cl->time_scale = 1.0f;
    }
    
    {
        for(s32 i = 0; i < 16; ++i) {
            os_platform.add_thread_job(&Implicit_Context::test_thread_job, (void *)(u64)i);
        }
    }
    
    // @XX
    // This is to make sure that game input is properly initted.
    // close_menu(g_cl);
    
    info->sim_time = program_state->time;
}

#if 0 // @XX
static void draw_xhair(Renderer * const renderer, const Texture_Handle xhair_texture, const v2 xhairp) {
    ASSERT(f_in_range(xhairp.x, 0.0f, 1.0f) && f_in_range(xhairp.y, 0.0f, 1.0f));
    queue_quad(renderer, xhair_texture, xhairp, V2(1.0f), V2(1.0f, 0.0f), V4(1.0f));
}
#endif

inline void Implicit_Context::reset_temporary_memory() {
    temporary_memory.used = 0;
}

void Implicit_Context::clone_game_state(Game *source, Game *clone) {
    TIME_BLOCK;
    // Saving our dynamic buffers:
    // None for now!
    
    // Copying dynamic allocations:
    // None for now!
    
    // Copying the game state:
    mem_copy(source, clone, sizeof(Game));
    
    // Restoring dynamic allocations:
    // None for now!
}

GAME_RUN_FRAME(Implicit_Context::g_run_frame) { 
    TIME_BLOCK;
    temporary_memory.used = 0;
    
    Game_Block_Info * const info        = (Game_Block_Info *)game_block->mem;
    Game_Client     * const g_cl        = &info->client;
    Renderer        * const renderer = &info->renderer;
    Audio_Info      * const  audio_info = &info->audio;
    Text_Info       * const   text_info = &info->text;
    
    // Clock:
    const f64 cur_time = program_state->time;
    const f64 sim_time = info->sim_time;
    
    f64        sim_dt = cur_time -            sim_time;
    const f64 real_dt = cur_time - program_state->time;
    
    const f64 game_time_scale = (f64)g_cl->time_scale;
    sim_dt *= game_time_scale;
    
    f64 this_frame_sim_time = 0.0;
    
    // Log:
    if(!global_log) {
        global_log = &info->log;
    }
    global_log->temporary_buffer.length = 0;
    global_log->heads_up_alpha -= (f32)real_dt;
    
    { // Parsing the program state:
        if(program_state) {
            // @XX
            // I think we don't even need to take care of this in the game code anymore.
            // maybe_update_render_size(renderer, program_state->window_size);
            // mem_copy(&renderer->draw_rect, &program_state->draw_rect, sizeof(rect2s));
        }
    }
    
    // We copy input because it's overall more robust to transitions.
    Input game_input = program_state->input;
    Input menu_input = program_state->input;
    
#if GENESIS_DEV
    Profiler *profiler = &info->profiler;
    
    if(!global_profiler) {
        global_profiler = profiler;
    }
    
    Input profiler_input = program_state->input;
    
    if(BUTTON_PRESSED(&game_input, profiler)) {
        if(profiler->has_focus) {
            if(!g_cl->in_menu) {
                os_platform.capture_cursor();
            }
            profiler->target_alpha = 0.3f;
        } else {
            if(!g_cl->in_menu) {
                os_platform.release_cursor();
            }
            profiler->target_alpha = 1.0f;
        }
        
        profiler->has_focus = !profiler->has_focus;
        profiler->dalpha = -profiler->dalpha;
    }
    
    if(profiler->has_focus) {
        clear_input_state(&game_input);
        clear_input_state(&menu_input);
    } else {
        clear_input_state(&profiler_input);
    }
#endif
    
    if(BUTTON_PRESSED(&game_input, menu)) {
        if(g_cl->in_menu) {
            // @XX
            // close_menu(g_cl);
            // If we don't do this, we send the same input to both the game and the menu,
            // which results in double presses which can lead to the menu reopening on the same
            // frame.
            advance_input(&game_input);
        } else {
            // @XX
            // open_menu(g_cl);
        }
    }
    
    { TIME_BLOCK; // Game update:
        if(g_cl->in_menu) {
            // @XX
            // update_menu(menu, renderer, &menu_input, program_state);
            program_state->input = menu_input;
            this_frame_sim_time = sim_dt;
        } else {
            // If we're too much behind, just drop frames.
            if((sim_dt + PHYSICS_DT) >= (16.0 * PHYSICS_DT)) {
                this_frame_sim_time = sim_dt - PHYSICS_DT;
            }
            
            u32 frames = 0;
            while((this_frame_sim_time + PHYSICS_DT) <= sim_dt) {
                SCOPE_MEMORY(&temporary_memory);
                
                // Server frame: actual simulation.
                // @Temporary
                // g_full_update(&g_cl->g, &game_input, PHYSICS_DT, audio_info, &g_cl->assets.datapack);
                
                this_frame_sim_time += PHYSICS_DT;
                os_platform.print(global_log->temporary_buffer);
                global_log->temporary_buffer.length = 0;
                ++frames;
            }
            
            logprint(global_log, "Computed %d frames.\n", frames);
            
            {
                // Making a temporary game at the supposed frame boundary:
                const f64 frame_remainder = sim_dt - this_frame_sim_time;
                ASSERT(frame_remainder < PHYSICS_DT);
                logprint(global_log, "Frame remainder: %fms\n", frame_remainder * 1000.0);
                Input clone_input = game_input;
                
                // @Temporary
                // clone_game_state(&g_cl->g, &g_cl->visual_state);
                // g_full_update(&g_cl->visual_state, &clone_input, frame_remainder, 0, 0);
            }
            
            program_state->input = game_input;
        }
    }
    
    // Sound update:
    // Samples to update being 0 means we don't need to fill the buffer.
    // The cases where this happens have empirically been:
    // -  On the first frame.
    // -  During (OS/computer-level) stalls, such as window moving/resizing.
    // -  When our framerate is fast enough, since we're filling the buffer ahead of time.
    
    u32 samples_to_update = os_platform.begin_sound_update();
    s16* update_buffer = update_synth(&info->synth, samples_to_update);
    os_platform.end_sound_update(update_buffer, samples_to_update);
    
    // Rendering:
#if 0 // :ReenableGraphics
    if(program_state->should_render) {
        render_begin_frame_and_clear(renderer, V4(1.0f, 0.0f, 1.0f, 1.0f));
        
        // draw_game_single_threaded(renderer, &g_cl->visual_state, &g_cl->assets);
        
        render_set_transform_right_handed_unit_scale(renderer->command_queue.command_list_handle);
        
        // Drawing UI:
        if(g_cl->in_menu) {
            f32 render_target_height = CAST(f32, program_state->draw_rect.top - program_state->draw_rect.bottom);
            draw_menu(menu, g_cl, renderer, text_info, render_target_height);
        }
        
        if((global_log->heads_up_buffer.length > 0) && (global_log->heads_up_alpha > 0.0f)) {
            // @XX
            // text_add_string(text_info, renderer, global_log->heads_up_buffer, V2(0.5f, 0.8f), 0.15f, FONT_MONO, true);
        }
        
        if(text_info->strings.count) {
            text_update(text_info, renderer);
        }
        
        os_platform.submit_commands(&renderer->command_queue.command_list_handle, 1, true);
        render_end_frame(renderer);
    }
#else
    // Dumb debug frame
    render_begin_frame_and_clear(renderer, V4(1.0f, 0.0f, 1.0f, 1.0f));
    
    render_set_transform_game_camera(renderer->command_queue.command_list_handle, V2(), V2(1.0f, 0.0f), 1.0f);
    
    static bool first_pass = true;
    if(first_pass) { // Generate basic meshes.
        first_pass = !first_pass;
        
        { // Basic quad.
            s32 index_count = 6;
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), 4, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            ASSERT(handle == RESERVED_MESH_HANDLE::QUAD);
            
            vertex_mapped[0] = make_render_vertex(V2(0.5f, 0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[1] = make_render_vertex(V2(-0.5f, 0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[2] = make_render_vertex(V2(-0.5f, -0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[3] = make_render_vertex(V2(0.5f, -0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            
            index_mapped[0] = 0;
            index_mapped[1] = 1;
            index_mapped[2] = 2;
            index_mapped[3] = 0;
            index_mapped[4] = 2;
            index_mapped[5] = 3;
            
            os_platform.update_editable_mesh(renderer->command_queue.command_list_handle, handle, index_count, true);
        }
        
        { // Color picker.
            // @Hack: The actual rendered color picker doesn't match the colors we sample from it. This is a quick and dirty
            // way of approximating the results.
            // Normally you'd interpolate in HSV and then convert to RGB rather than interpolating directly in RGB.
            s32 index_count = 6;
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), 4, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            ASSERT(handle == RESERVED_MESH_HANDLE::COLOR_PICKER);
            
            vertex_mapped[0] = make_render_vertex(V2(0.5f, 0.5f), V4(1.0f, 0.0f, 0.0f, 1.0f));
            vertex_mapped[1] = make_render_vertex(V2(-0.5f, 0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[2] = make_render_vertex(V2(-0.5f, -0.5f), V4(0.0f, 0.0f, 0.0f, 1.0f));
            vertex_mapped[3] = make_render_vertex(V2(0.5f, -0.5f), V4(0.0f, 0.0f, 0.0f, 1.0f));
            
            index_mapped[0] = 0;
            index_mapped[1] = 1;
            index_mapped[2] = 2;
            index_mapped[3] = 0;
            index_mapped[4] = 2;
            index_mapped[5] = 3;
            
            renderer->color_picker_color = &vertex_mapped[0].color;
            
            os_platform.update_editable_mesh(renderer->command_queue.command_list_handle, handle, index_count, false);
        }
        
        Output_Mesh mesh = {};
        { // Default edit mesh.
            // @Hardcoded
#define MAX_EDITABLE_MESH_VERTEX_COUNT 256
            s32 index_count = 3;
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), MAX_EDITABLE_MESH_VERTEX_COUNT, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            
            vertex_mapped[0] = make_render_vertex(V2( 0.0f, 0.5f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[1] = make_render_vertex(V2(-0.5f, 0.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[2] = make_render_vertex(V2( 0.5f, 0.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            
            index_mapped[0] = 0;
            index_mapped[1] = 1;
            index_mapped[2] = 2;
            
            os_platform.update_editable_mesh(renderer->command_queue.command_list_handle, handle, index_count, false);
            
            mesh_init(&info->mesh_editor);
            
            mesh.handle = handle;
            mesh.vertex_mapped = vertex_mapped;
            mesh.index_mapped = index_mapped;
            mesh.vert_count = 3;
            mesh.index_count = 3;
            mesh.default_offset = V2();
            mesh.default_scale = V2(1.0f);
            mesh.default_rot = V2(1.0f, 0.0f);
            info->edit_mesh = add_mesh(&info->mesh_editor, &mesh, MAX_EDITABLE_MESH_VERTEX_COUNT, V4(0.3f, 0.07f, 0.0f, 1.0f));
        }
    }
    
    mesh_update(&info->mesh_editor, renderer, &menu_input);
    program_state->input = menu_input;
    
    os_platform.submit_commands(&renderer->command_queue.command_list_handle, 1, true);
    
    render_end_frame(renderer);
#endif
    
#if GENESIS_DEV
    profile_update(profiler, program_state, text_info, renderer, &profiler_input);
    
    if(profiler->has_focus) {
        program_state->input = profiler_input;
    }
#endif
    
    ASSERT(sim_time <= cur_time);
    program_state->time = cur_time;
    info->sim_time += this_frame_sim_time / game_time_scale;
    
    if(program_state->should_close) {
        { // Update the config.
            User_Config config = {};
            config.version = USER_CONFIG_VERSION;
            config.fullscreen = program_state->should_be_fullscreen;
            config.window_dim = V2S16(program_state->window_size);
            config.input = program_state->input_settings;
            config.volume = audio_info->target_volume;
            
            string config_file;
            config_file.data = (u8 *)&config;
            config_file.length = sizeof(User_Config);
            os_platform.write_entire_file(STRING("main.config"), config_file);
        }
    }
}

extern "C" GAME_GET_API(g_get_api) {
    os_platform = os_import->os;
    
    Game_Interface game_export;
    game_export.init_mem  = &Implicit_Context::g_init_mem;
    game_export.run_frame = &Implicit_Context::g_run_frame;
    return game_export;
}

// @XX Sigh.
void Implicit_Context::text_update(Text_Info *,Renderer *) {}
void Implicit_Context::profile_update(Profiler *,Program_State *,Text_Info *,Renderer *,Input *) {}