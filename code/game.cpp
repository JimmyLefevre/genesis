
#include "basic.h"
#if OS_WINDOWS
extern "C" {
#include "dll_nocrt.c"
}
#endif

#include "game_headers.h"

#include "game_context.h"
#include "interfaces.h"
static os_function_interface os_platform;
static Log *global_log;

#include "game_code.cpp"

inline v2 unit_scale_to_world_space_offset(v2 p, f32 camera_zoom) {
    v2 result = p - V2(0.5f);
    f32 inv_camera_zoom = 1.0f / camera_zoom;
    result.x *= GAME_ASPECT_RATIO_X * inv_camera_zoom;
    result.y *= GAME_ASPECT_RATIO_Y * inv_camera_zoom;
    return result;
}

inline v2 rotate_around(v2 p, v2 center, v2 rotation) {
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

void Implicit_Context::draw_game(Rendering_Info *render_info, Game *g, Asset_Storage *assets) {
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
        
        Texture_ID blank_texture   = FIND_TEXTURE_ID(render_info, blank);
        Texture_ID player_texture  = FIND_TEXTURE_ID(render_info, player);
        Texture_ID hitbox_texture  = FIND_TEXTURE_ID(render_info, default_hitbox);
        Texture_ID hurtbox_texture = FIND_TEXTURE_ID(render_info, default_hurtbox);
        Texture_ID untexture       = FIND_TEXTURE_ID(render_info, untextured);
        Texture_ID solid_texture   = FIND_TEXTURE_ID(render_info, default_solid);
        
        queue_shader(render_info, SHADER_INDEX_TEXTURE);
        queue_transform_game_camera(render_info, camera_p, camera_dir, camera_zoom);
        
        { // Players
            const v4 color = V4(0.0f, 1.0f, 1.0f, 1.0f);
            const u32 count = g->players.count;
            
            for(u32 i = 0; i < count; ++i) {
                const v2 p = g->players.ps[i];
                const v2 halfdim = V2(PLAYER_HALFWIDTH, PLAYER_HALFHEIGHT);
                
                queue_quad(render_info, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Partner
            const v4 color = V4(0.2f, 1.0f, 0.0f, 1.0f);
            
            FORI(0, g->partners.count) {
                const v2 p = g->partners.ps[i];
                const v2 halfdim = V2(PARTNER_HALFWIDTH, PARTNER_HALFHEIGHT);
                
                queue_quad(render_info, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Trees
            const v4 color = V4(0.0f, 0.1f, 0.0f, 1.0f);
            const s32 count = g->trees.count;
            
            FORI(0, count) {
                const v2 p = g->trees.ps[i];
                const v2 halfdim = V2(TREE_HALFWIDTH, TREE_HALFHEIGHT);
                
                queue_quad(render_info, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Turrets
            const v4 color = V4(0.3f, 0.1f, 0.1f, 1.0f);
            const s32 count = g->turrets.count;
            
            FORI(0, count) {
                const v2 p = g->turrets.ps[i];
                const v2 halfdim = V2(TURRET_HALFWIDTH, TURRET_HALFHEIGHT);
                
                queue_quad(render_info, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
        
        { // Bullets
            const v4 color = anti_all_color;
            const s32 count = g->bullets.count;
            
            FORI(0, count) {
                const v2 p = g->bullets.ps[i];
                
                queue_circle_pradius(render_info, p, BULLET_RADIUS, color);
            }
        }
        
        { // Dogs
            const v4 color = anti_partner_color;
            const s32 count = g->dogs.count;
            
            FORI(0, count) {
                const v2 p = g->dogs.ps[i];
                
                queue_circle_pradius(render_info, p, DOG_RADIUS, color);
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
                    queue_line_strip(render_info, pts, ARRAY_LENGTH(pts), 0.01f, anti_partner_color);
                }
                
                queue_quad(render_info, blank_texture, p, halfdim, V2(1.0f, 0.0f), color);
            }
        }
    }
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

#if GENESIS_BUILD_ASSETS_ON_STARTUP
void Implicit_Context::files_convert_wav_to_facs(string *wav_names, s32 file_count, Memory_Block *temp_block, string *out_files) {
    string asset_directory = os_platform.debug_asset_directory;
    
    for(s32 ifile = 0; ifile < file_count; ++ifile) {
        SCOPE_MEMORY(&temporary_memory);
        string wav_name = concat(&temporary_memory, asset_directory, wav_names[ifile]);
        
        Sound_Asset wav_file = load_wav(wav_name);
        const u32 samples_per_channel = wav_file.samplesPerChannel;
        s16 *wav_uninterleaved_samples = push_array(temp_block, s16, wav_file.samplesPerChannel * wav_file.channelCount);
        
        u16 channel_count  = wav_file.channelCount;
        s16 **in_channels  = (s16 **)push_array(temp_block, s16 *, channel_count);
        s16 **out_channels = (s16 **)push_array(temp_block, s16 *, channel_count);
        
        for(u32 ichannel = 0; ichannel < channel_count; ++ichannel) {
            s16 *in = wav_file.samples + ichannel;
            s16 *out = wav_uninterleaved_samples + samples_per_channel * ichannel;
            
            for(u32 isample = 0; isample < samples_per_channel; ++isample) {
                *out++ = *in;
                in += channel_count;
            }
            
            in_channels[ichannel] = in;
            out_channels[ichannel] = out;
        }
        
        {
            FACS_Header header;
            header.sample_rate = 44100;
            header.channel_count = (u8)wav_file.channelCount;
            header.bytes_per_sample = 2;
            header.chunk_size = 65536;
            u32 chunk_count = channel_count * (samples_per_channel + header.chunk_size - 1) / header.chunk_size;
            ASSERT(chunk_count < U16_MAX);
            // @Cleanup: Our audio system expects this division, so chunk_count is ambiguous in this scope.
            header.chunk_count = (u16)chunk_count / channel_count;
            header.encoding_flags = FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING;
            
            u64 file_size = sizeof(FACS_Header) + header.chunk_size * header.chunk_count * header.channel_count * header.bytes_per_sample;
            if(header.encoding_flags & FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING) {
                file_size += 2 * header.chunk_count * header.channel_count * header.bytes_per_sample;
            }
            string write_file;
            write_file.length = file_size;
            write_file.data = push_array(temp_block, u8, write_file.length);
            zero_mem(write_file.data, write_file.length);
            FACS_Header *out_header = (FACS_Header *)write_file.data;
            *out_header = header;
            
            u32 chunks_left = header.chunk_count;
            ASSERT(chunks_left);
            s32 chunk_sample_count = header.chunk_size;
            s16 *facs_samples = (s16 *)(out_header + 1);
            s32 out_channel_stride = header.chunk_size;
            if(header.encoding_flags & FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING) {
                ++out_channel_stride;
            }
            
            for(u32 i = 0; i < channel_count; ++i) {
                in_channels[i] = wav_uninterleaved_samples + i * samples_per_channel;
                out_channels[i] = facs_samples + i * out_channel_stride;
            }
            
            u32 zero_pad = chunk_sample_count * header.chunk_count - samples_per_channel;
            do {
                if(chunks_left == 1) {
                    chunk_sample_count = samples_per_channel - chunk_sample_count * (header.chunk_count - 1);
                    if(header.encoding_flags & FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING) {
                        ++chunk_sample_count;
                    }
                }
                
                for(u32 ichannel = 0; ichannel < channel_count; ++ichannel) {
                    mem_copy(in_channels[ichannel], out_channels[ichannel], sizeof(s16) * chunk_sample_count);
                }
                
                if(header.encoding_flags & FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING) {
                    for(u32 ichannel = 0; ichannel < channel_count; ++ichannel) {
                        out_channels[ichannel][chunk_sample_count] = in_channels[ichannel][chunk_sample_count];
                        // We're accounting for all of the padding bytes because
                        // we're skipping over every channel at the end anyway.
                        out_channels[ichannel] += channel_count;
                    }
                }
                
                --chunks_left;
                for(u32 ichannel = 0; ichannel < channel_count; ++ichannel) {
                    in_channels[ichannel]  += chunk_sample_count;
                    out_channels[ichannel] += channel_count * chunk_sample_count;
                }
            } while(chunks_left);
            
            for(u32 ichannel = 0; ichannel < channel_count; ++ichannel) {
                zero_mem(out_channels[ichannel], zero_pad);
            }
            
            out_files[ifile] = write_file;
        }
    }
}

string Implicit_Context::files_convert_bmp_to_ta(string *bmp_names, v2 *bmp_halfdims, v2 *bmp_offsets, u32 file_count, Memory_Block *temp_block) {
    SCOPE_MEMORY(&temporary_memory);
    string asset_directory = os_platform.debug_asset_directory;
    
    Loaded_BMP *bmps = push_array(temp_block, Loaded_BMP, file_count);
    v2s16 *rect_dims = push_array(temp_block, v2s16, file_count);
    
    for(u32 ibmp = 0; ibmp < file_count; ++ibmp) {
        SCOPE_MEMORY(&temporary_memory);
        
        string bmp_name = concat(&temporary_memory, asset_directory, bmp_names[ibmp]);
        Loaded_BMP bmp = load_bmp(bmp_name);
        
        v2s16 dim = V2S16((s16)(bmp.width + 2), (s16)(bmp.height + 2));
        
        bmps[ibmp] = bmp;
        rect_dims[ibmp] = dim;
    }
    
    // @Incomplete: Adjust atlas dimensions based on BMPs' dimensions?
    TA_Header header;
    header.texture_count = (u16)file_count;
    header.atlas_width = 1024;
    header.atlas_height = 1024;
    header.texture_encoding = 0;
    
    // 24 bytes per texture entry: 8 for ID, 8*2 for UVs.
    u64 file_size = sizeof(TA_Header) + sizeof(Texture_Framing) * header.texture_count +
        4 * header.atlas_width * header.atlas_height;
    string write_file = make_string(push_array(temp_block, u8, file_size), file_size);
    zero_mem(write_file.data, write_file.length);
    
    TA_Header *out_header = (TA_Header *)write_file.data;
    *out_header = header;
    
    Texture_Framing *out_framings = (Texture_Framing *)(out_header + 1);
    
    v2s16 *rect_ps = push_array(temp_block, v2s16, file_count);
    { // Packing.
        ASSERT((header.atlas_width < S16_MAX) && (header.atlas_height < S16_MAX));
        v2s16 target_dim = V2S16(header.atlas_width, header.atlas_height);
        
        s16 *line_lefts = push_array(&temporary_memory, s16, 256);
        s16 *line_ys = push_array(&temporary_memory, s16, 256);
        s16 line_count;
        rect_pack_init(line_lefts, line_ys, &line_count, target_dim.x);
        rect_pack(rect_dims, (s16)file_count, line_lefts, line_ys, &line_count,
                  target_dim.x, target_dim.y, rect_ps);
    }
    
    u32 *texture = (u32 *)(out_framings + file_count);
    const f32 f_atlas_width = (f32)header.atlas_width;
    const f32 f_atlas_height = (f32)header.atlas_height;
    const f32 inv_atlas_width = 1.0f / f_atlas_width;
    const f32 inv_atlas_height = 1.0f / f_atlas_height;
    const f32 inv_255 = 1.0f / 255.0f;
    for(u32 iuv = 0; iuv < file_count; ++iuv) {
        Loaded_BMP bmp = bmps[iuv];
        v2s16 rect_min = rect_ps[iuv] + V2S16(1, 1);
        v2s16 rect_max = rect_ps[iuv] + rect_dims[iuv] - V2S16(1, 1);
        
        const s32 width = rect_max.x - rect_min.x;
        const v2 uvmin = V2(((f32)rect_min.x) * inv_atlas_width,
                            ((f32)rect_min.y) * inv_atlas_height);
        const v2 uvmax = V2(((f32)rect_max.x) * inv_atlas_width,
                            ((f32)rect_max.y) * inv_atlas_height);
        
        Texture_Framing framing;
        framing.offset = bmp_offsets[iuv];
        framing.halfdim = bmp_halfdims[iuv];
        
        for(s32 y = rect_min.y; y <= rect_max.y; ++y) {
            const s32 yoffset = (y - rect_min.y) * width;
            for(s32 x = rect_min.x; x <= rect_max.x; ++x) {
                s32 itexel = yoffset + x - rect_min.x;
                const u32 read_pixel = bmp.data[itexel];
                
                // Premultiplying alpha:
                f32 r = (read_pixel & 0xFF) * inv_255;
                f32 g = ((read_pixel >> 8) & 0xFF) * inv_255;
                f32 b = ((read_pixel >> 16) & 0xFF) * inv_255;
                f32 a = ((read_pixel >> 24) & 0xFF) * inv_255;
                r *= a;
                g *= a;
                b *= a;
                ASSERT((r >= 0.0f) && (r <= 1.0f));
                ASSERT((g >= 0.0f) && (g <= 1.0f));
                ASSERT((b >= 0.0f) && (b <= 1.0f));
                ASSERT((a >= 0.0f) && (a <= 1.0f));
                
                u32 write_pixel = ((u32)(r * 255.0f)) | ((u32)(g * 255.0f) << 8) | 
                    ((u32)(b * 255.0f) << 16) | ((u32)(a * 255.0f) << 24);
                texture[y * header.atlas_width + x] = write_pixel;
            }
        }
        
        framing.uvs.min = uvmin;
        framing.uvs.max = uvmax;
        out_framings[iuv] = framing;
    }
    
    return write_file;
}
#endif

void ta_to_texture_ids(string ta, Rendering_Info *render_info, Texture_ID *out_ids) {
    TA_Header header = *(TA_Header *)ta.data;
    Texture_Framing *file_framings = (Texture_Framing *)(ta.data + sizeof(TA_Header));
    u8 *texture = (u8 *)(file_framings + header.texture_count);
    
    render_add_atlas(render_info, texture, header.atlas_width, header.atlas_height, header.texture_count, file_framings, out_ids);
}

// @Cleanup
static unsigned int stb_bit_reverse(unsigned int n)
{
    n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
    n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
    n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
    n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
    return (n >> 16) | (n << 16);
}

// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
// as required by the specification. fast(?) implementation from stb.h
// @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
static int ilog(s32 n)
{
    static signed char log2_4[16] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };
    
    if (n < 0) return 0; // signed n returns 0
    
    // 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29)
    if (n < (1 << 14))
        if (n < (1 <<  4))            return  0 + log2_4[n      ];
    else if (n < (1 <<  9))       return  5 + log2_4[n >>  5];
    else                     return 10 + log2_4[n >> 10];
    else if (n < (1 << 24))
        if (n < (1 << 19))       return 15 + log2_4[n >> 15];
    else                     return 20 + log2_4[n >> 20];
    else if (n < (1 << 29))       return 25 + log2_4[n >> 25];
    else                     return 30 + log2_4[n >> 30];
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
    Rendering_Info * const render_info = &info->render_info;
    Audio_Info     * const  audio_info = &info->audio;
    Text_Info      * const   text_info = &info->text;
    Menu           * const        menu = &info->menu;
    
    // Memory partitioning:
    sub_block(&render_info->render_queue, game_block, KiB(128));
    sub_block(&text_info->font_block    , game_block, MiB(  4));
    sub_block(&menu->block              , game_block, KiB( 64));
    
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
                    program_state->window_size[0] = config->window_dim.x;
                    program_state->window_size[1] = config->window_dim.y;
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
    
    { // Audio.
        const s32 output_hz       = program_state->audio_output_rate;
        const s32 output_channels = program_state->audio_output_channels;
        
        // This should not necessarily be core_count. Rather, it should be the number of threads
        // we want working at the same time on audio jobs.
        audio_info->core_count = core_count;
        
        // Processing buffers:
        audio_info->temp_buffer = push_array(game_block, f32  , output_hz      );
        audio_info->mix_buffers = push_array(game_block, f32 *, output_channels);
        for(s32 i = 0; i < output_channels; ++i) {
            audio_info->mix_buffers[i] = push_array(game_block, f32, output_hz);
        }
        audio_info->out_buffer = push_array(game_block, s16, output_hz * output_channels);
        audio_info->channel_count = (u8)output_channels;
        init(&audio_info->loads, game_block);
        
        // Chunk buffers:
        Audio_Page_Identifier bad_id;
        bad_id.all = 0xFFFFFFFFFFFFFFFF;
        for(s32 i = 0; i < MAX_SOUND_COUNT * 2; ++i) {
            audio_info->audio_pages[i] = push_array(game_block, s16, AUDIO_PAGE_SIZE + 1);
            audio_info->audio_page_ids[i] = bad_id;
        }
        
        { // Threaded read bitfield:
            Audio_Load_Queue **queues = push_array(game_block, Audio_Load_Queue *, core_count);
            FORI(0, core_count) {
                queues[i] = push_struct(game_block, Audio_Load_Queue, 64); // @Hardcoded cache line size
            }
            audio_info->load_queues = queues;
            push_size(game_block, 0, 64);
            
            audio_info->last_gather_first_frees = push_array(game_block, s32, core_count);
        }
        
        // We're assuming current_volume was initialised when loading the config.
        audio_info->target_volume.master                            = audio_info->current_volume.master;
        audio_info->target_volume.by_category[AUDIO_CATEGORY_SFX  ] = audio_info->current_volume.by_category[AUDIO_CATEGORY_SFX  ];
        audio_info->target_volume.by_category[AUDIO_CATEGORY_MUSIC] = audio_info->current_volume.by_category[AUDIO_CATEGORY_MUSIC];
        
        // When we get to unloading sounds, be sure to reset these values to -1.
        FORI(0, SOUND_UID_COUNT) {
            audio_info->sounds_by_uid[i] = -1;
        }
        FORI(0, PLAYING_SOUND_HANDLE_BITFIELD_SIZE) {
            audio_info->free_commands[i] = 0xFFFFFFFFFFFFFFFF;
        }
        FORI(0, MAX_SOUND_COUNT) {
            audio_info->play_commands[i].old_volume = push_array(game_block, f32, output_channels);
            audio_info->play_commands[i].volume = push_array(game_block, f32, output_channels);
        }
    }
    
#if GENESIS_DEV
    { // Profiler:
        Profiler profiler = {};
        
        profile_init(&profiler, game_block, core_count);
        
        info->profiler = profiler;
        
#if GENESIS_DEV // @Duplication
        global_profiler = &info->profiler;
#endif
    }
#endif
    
    { // Menu.
        Menu_Page *pages      = push_array(&menu->block, Menu_Page, 8);
        s32        page_count = 0;
        const rect2 main_rect = rect2_minmax(V2(0.0f), V2(1.0f));
        
        { // Audio slider page.
            Menu_Page page = {};
            rect2 ui_rect = main_rect;
            f32 y_margin = 0.05f;
            f32 x_margin = y_margin * GAME_ASPECT_RATIO;
            ui_rect.  left += x_margin;
            ui_rect. right -= x_margin;
            ui_rect.bottom += y_margin;
            ui_rect.   top -= y_margin;
            
            rect2      *slider_aabbs = push_array(&menu->block,      rect2, 8);
            Slider     *sliders      = push_array(&menu->block,     Slider, 8);
            s32         slider_count = 0;
            
            rect2      *button_aabbs = push_array(&menu->block,      rect2, 8);
            Button     *buttons      = push_array(&menu->block,     Button, 8);
            s32         button_count = 0;
            
            rect2      *toggle_aabbs = push_array(&menu->block,      rect2, 8);
            Toggle     *toggles      = push_array(&menu->block,     Toggle, 8);
            s32         toggle_count = 0;
            
            rect2    *dropdown_aabbs = push_array(&menu->block,      rect2, 8);
            Dropdown *dropdowns      = push_array(&menu->block,   Dropdown, 8);
            s32       dropdown_count = 0;
            
            Menu_Label *labels       = push_array(&menu->block, Menu_Label, 8);
            s32         label_count  = 0;
            
            Dropdown_Option *dropdown_options = push_array(&menu->block, Dropdown_Option, 8);
            s32              dropdown_option_count = 0;
            
            // @Duplication all of these label calls.
            
            add_slider(slider_aabbs, sliders, &slider_count, ui_rect,
                       V2(0.2f, 0.50f), 1.0f, 0.1f, 0.0f, 1.0f, &audio_info->target_volume.master);
            add_slider(slider_aabbs, sliders, &slider_count, ui_rect,
                       V2(0.2f, 0.25f), 1.0f, 0.1f, 0.0f, 1.0f, &audio_info->target_volume.by_category[AUDIO_CATEGORY_SFX]);
            add_slider(slider_aabbs, sliders, &slider_count, ui_rect,
                       V2(0.2f, 0.10f), 1.0f, 0.1f, 0.0f, 1.0f, &audio_info->target_volume.by_category[AUDIO_CATEGORY_MUSIC]);
            add_label(labels, &label_count, ui_rect,
                      V2(0.0f, 0.50f + 0.1f), 0.065f, STRING("Volumes:"), FONT_REGULAR, false);
            add_label(labels, &label_count, ui_rect,
                      V2(0.0f, 0.50f),        0.03f, STRING("Master")  , FONT_REGULAR, false);
            add_label(labels, &label_count, ui_rect,
                      V2(0.0f, 0.25f),        0.03f, STRING("Effects") , FONT_REGULAR, false);
            add_label(labels, &label_count, ui_rect,
                      V2(0.0f, 0.10f),        0.03f, STRING("Music")   , FONT_REGULAR, false);
            
            add_label(labels, &label_count, ui_rect,
                      V2(0.15f, 0.85f + 0.1f * 0.5f + 0.02f), 0.03f, STRING("Page 1"), FONT_REGULAR, true);
            add_button(button_aabbs, buttons, &button_count, ui_rect,
                       V2(0.15f, 0.85f), V2(0.1f), make_any32(1), (any32 *)&menu->current_page);
            
            add_label(labels, &label_count, ui_rect,
                      V2(0.50f, 0.85f + 0.1f * 0.5f + 0.02f), 0.03f, STRING("Godmode?"), FONT_REGULAR, true);
#if GENESIS_DEV
            add_toggle(toggle_aabbs, toggles, &toggle_count, ui_rect,
                       V2(0.50f, 0.85f), V2(0.1f), &g_cl->g.godmode);
#endif
            
            {
                Dropdown *dropdown = add_dropdown(dropdown_aabbs, dropdowns, &dropdown_count, ui_rect,
                                                  V2(0.85f), V2(0.1f), 4, (any32 *)&menu->sliders_to_display,
                                                  0, &menu->block);
                add_label(labels, &label_count, ui_rect,
                          V2(0.85f, 0.85f + 0.1f * 0.5f + 0.02f), 0.03f, STRING("Volume sliders"), FONT_REGULAR, true);
                
                Dropdown_Option options[] = {
                    {{0}, STRING("None")},
                    {{1}, STRING("One")},
                    {{2}, STRING("Two")},
                    {{3}, STRING("Three")},
                };
                Dropdown_Option *added_options = add_dropdown_options(dropdown_options, &dropdown_option_count,
                                                                      options, ARRAY_LENGTH(options));
                dropdown->options = added_options;
            }
            
            ASSERT(  slider_count < 8);
            ASSERT(  button_count < 8);
            ASSERT(  toggle_count < 8);
            ASSERT(dropdown_count < 8);
            ASSERT(   label_count < 8);
            ASSERT(dropdown_option_count < 8);
            
            page.slider_aabbs    = slider_aabbs;
            page.sliders         = sliders;
            page.slider_count    = slider_count;
            
            page.button_aabbs    = button_aabbs;
            page.buttons         = buttons;
            page.button_count    = button_count;
            
            page.toggle_aabbs    = toggle_aabbs;
            page.toggles         = toggles;
            page.toggle_count    = toggle_count;
            
            page.dropdown_aabbs = dropdown_aabbs;
            page.dropdowns      = dropdowns;
            page.dropdown_count = dropdown_count;
            
            page.labels         = labels;
            page.label_count    = label_count;
            
            pages[page_count++] = page;
        }
        
        { // Bindings and stuff page.
            Menu_Page page = {};
            rect2 ui_rect = main_rect;
            f32 y_margin = 0.05f;
            f32 x_margin = y_margin * GAME_ASPECT_RATIO;
            ui_rect.  left += x_margin;
            ui_rect. right -= x_margin;
            ui_rect.bottom += y_margin;
            ui_rect.   top -= y_margin;
            
            rect2      *slider_aabbs = push_array(&menu->block,    rect2, 8);
            Slider     *sliders      = push_array(&menu->block,   Slider, 8);
            s32         slider_count = 0;
            
            rect2      *button_aabbs = push_array(&menu->block,    rect2, 8);
            Button     *buttons      = push_array(&menu->block,   Button, 8);
            s32         button_count = 0;
            
            rect2      *toggle_aabbs = push_array(&menu->block,    rect2, 8);
            Toggle     *toggles      = push_array(&menu->block,   Toggle, 8);
            s32         toggle_count = 0;
            
            rect2    *dropdown_aabbs = push_array(&menu->block,    rect2, 8);
            Dropdown *dropdowns      = push_array(&menu->block, Dropdown, 8);
            s32       dropdown_count = 0;
            
            rect2   *keybind_aabbs   = push_array(&menu->block,    rect2, 8);
            Keybind *keybinds        = push_array(&menu->block,  Keybind, 8);
            s32      keybind_count   = 0;
            
            Menu_Label *labels       = push_array(&menu->block, Menu_Label, 8);
            s32         label_count  = 0;
            
            add_button(button_aabbs, buttons, &button_count, ui_rect,
                       V2(0.50f, 0.50f), V2(0.10f), make_any32(  0), (any32 *)&menu->current_page);
            add_label(labels, &label_count, ui_rect,
                      V2(0.50f, 0.50f + 0.10f * 0.5f + 0.02f), 0.03f, STRING("Page 0"), FONT_REGULAR, true);
            
            add_button(button_aabbs, buttons, &button_count, ui_rect,
                       V2(0.05f, 0.50f), V2(0.04f), make_any32(800), (any32 *)&program_state->window_size[1]);
            add_label(labels, &label_count, ui_rect,
                      V2(0.05f, 0.50f + 0.04f * 0.5f + 0.02f), 0.03f, STRING("Make the window 1422*800"), FONT_REGULAR, true);
            
            add_slider(slider_aabbs, sliders, &slider_count, ui_rect,
                       V2(0.2f, 0.35f), 1.0f, 0.1f, 0.05f, 1.0f, &g_cl->time_scale);
            add_label(labels, &label_count, ui_rect,
                      V2(0.0f, 0.35f), 0.03f, STRING("Simulation speed"), FONT_REGULAR, false);
            
            add_toggle(toggle_aabbs, toggles, &toggle_count, ui_rect,
                       V2(0.75f, 0.50f), V2(0.10f), &program_state->should_close);
            add_label(labels, &label_count, ui_rect,
                      V2(0.75f, 0.50f + 0.10f * 0.5f + 0.02f), 0.03f, STRING("Close the game"), FONT_REGULAR, true);
            
#if 0
            add_keybind(keybind_aabbs, keybinds, &keybind_count, ui_rect,
                        V2(0.8f, 0.3f), V2(0.1f), &program_state->input_settings.bindings[GAME_BUTTON_INDEX_left]);
#endif
            
            ASSERT(  slider_count < 8);
            ASSERT(  button_count < 8);
            ASSERT(  toggle_count < 8);
            ASSERT(dropdown_count < 8);
            ASSERT(   label_count < 8);
            
            page.slider_aabbs   = slider_aabbs;
            page.sliders        = sliders;
            page.slider_count   = slider_count;
            
            page.button_aabbs   = button_aabbs;
            page.buttons        = buttons;
            page.button_count   = button_count;
            
            page.toggle_aabbs   = toggle_aabbs;
            page.toggles        = toggles;
            page.toggle_count   = toggle_count;
            
            page.dropdown_aabbs = dropdown_aabbs;
            page.dropdowns      = dropdowns;
            page.dropdown_count = dropdown_count;
            
            page.keybind_aabbs  = keybind_aabbs;
            page.keybinds       = keybinds;
            page.keybind_count  = keybind_count;
            
            page.labels      = labels;
            page.label_count = label_count;
            
            pages[page_count++] = page;
        }
        
        menu-> interaction = make_interaction();
        menu->       hover = make_interaction();
        menu->       pages = pages;
        menu->current_page = 0;
    }
    
    { // Rendering.
        gl_setup();
        render_info->aspect_ratio = 16.0f / 9.0f;
        glGenBuffers(1, &render_info->idGeneralVertexBuffer);
        
        { // Shaders.
            Shader texture_shader;
            
            texture_shader.program = GLTextureShader();
            texture_shader.transform = glGetUniformLocation(texture_shader.program, "transform");
            texture_shader.texture_sampler = glGetUniformLocation(texture_shader.program, "txSampler");
            texture_shader.color = glGetAttribLocation(texture_shader.program, "vertColor");
            texture_shader.p = glGetAttribLocation(texture_shader.program, "p");
            texture_shader.uv = glGetAttribLocation(texture_shader.program, "vertUV");
            
            render_info->shaders[SHADER_INDEX_TEXTURE] = texture_shader;
            render_info->last_shader_queued = SHADER_INDEX_COUNT;
        }
        
        { // Transforms.
            f32 *game_transform = (f32 *)(render_info->transforms[TRANSFORM_INDEX_GAME]);
            f32 *rhus_transform = (f32 *)(render_info->transforms[TRANSFORM_INDEX_RIGHT_HANDED_UNIT_SCALE]);
            
            f32 game_scale_x = 2.0f / GAME_ASPECT_RATIO_X;
            f32 game_scale_y = 2.0f / GAME_ASPECT_RATIO_Y;
            game_transform[0] = game_scale_x;
            game_transform[5] = game_scale_y;
            game_transform[12] = 0.0f;
            game_transform[13] = 0.0f;
            game_transform[15] = 1.0f;
            
            rhus_transform[0] = 2.0f;
            rhus_transform[5] = 2.0f;
            rhus_transform[12] = -1.0f;
            rhus_transform[13] = -1.0f;
            rhus_transform[15] = 1.0f;
        }
    }
    
    { // Text info:
        init(&text_info->glyph_hash, game_block);
        rect_pack_init(text_info->glyph_atlas_lines.lefts, text_info->glyph_atlas_lines.ys,
                       &text_info->glyph_atlas_lines.count, GLYPH_ATLAS_SIZE);
        u32 *zeroes = push_array(game_block, u32, GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE);
        text_info->glyph_atlas = gl_add_texture_linear_rgba(GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, zeroes);
    }
    
#if GENESIS_BUILD_ASSETS_ON_STARTUP
    { // Asset build on startup.
        string *data_files = push_array(&temporary_memory, string, ASSET_UID_COUNT);
        usize data_file_count = 0;
        
        { // Fonts
            string font_names[] = {
                STRING("fonts/Hack-Regular.ttf"),
                STRING("fonts/Charter Italic.ttf"),
                STRING("fonts/Charter Regular.ttf"),
            };
            
            SCOPE_MEMORY(&temporary_memory);
            
            for(s32 ifont = 0; ifont < ARRAY_LENGTH(font_names); ++ifont) {
                string font_full_path = concat(&temporary_memory, os_platform.debug_asset_directory,
                                               font_names[ifont]);
                string font_file = os_platform.read_entire_file(font_full_path);
                
                data_files[data_file_count++] = font_file;
            }
        }
        
        { // Audio files
            string wav_files[] = {
                STRING("music/Danse Macabre.wav"),
                STRING("sounds/erase4.wav"),
            };
            const usize wav_file_count = ARRAY_LENGTH(wav_files);
            files_convert_wav_to_facs(wav_files, ARRAY_LENGTH(wav_files), game_block, data_files + data_file_count);
            
            //
            //
            // @Temporary
            // Audio compression research
            //
            //
            {
                string facs_file = data_files[data_file_count];
                string compressed = push_string(game_block, facs_file.length);
                auto *header = (FACS_Header *)facs_file.data;
                ssize bytes_per_chunk = header->chunk_size + (header->encoding_flags & 1) * header->bytes_per_sample;
                ssize samples_per_chunk = bytes_per_chunk / header->bytes_per_sample;
                
                f32 *scratch = push_array(game_block, f32, samples_per_chunk * 4);
                zero_mem(scratch, sizeof(f32) * (samples_per_chunk - 1));
                
                *(FACS_Header *)compressed.data = *header;
                usize written = sizeof(FACS_Header);
                
                // @Cleanup: See todo.txt.
                ssize actual_samples_per_chunk = samples_per_chunk - 1;
                
                ssize no_filter_error = 0;
                ssize delta_error = 0;
                ssize gradient_error = 0;
                ssize tri_error = 0;
                FORI_NAMED(chunk_index, 2, header->chunk_count) {
                    FORI_NAMED(channel_index, 0, header->channel_count) {
                        s16 *chunk_samples = ((s16 *)(header + 1)) + samples_per_chunk * (chunk_index * header->channel_count) + 
                            samples_per_chunk * channel_index;
                        
                        s16 *output_samples = (s16 *)(compressed.data + written);
                        
                        union {
                            s16 _s16[8];
                            __m128i _m128;
                        } last = {};
                        FORI(0, samples_per_chunk) {
                            s16 current = chunk_samples[i];
                            output_samples[i] = current - (last._s16[0] << 1) + last._s16[1];
                            
                            no_filter_error += s_abs(current);
                            delta_error += s_abs(current - last._s16[0]);
                            gradient_error += s_abs(current - (last._s16[0] << 1) + last._s16[1]);
                            tri_error += s_abs(current - (last._s16[0]) + (last._s16[1] * 3) - (last._s16[2]));
                            
                            last._m128 = _mm_slli_si128(last._m128, 2);
                            last._s16[0] = current;
                        }
                        
#if 0
                        // Slow MDCT:
                        const ssize n = actual_samples_per_chunk;
                        const ssize N = actual_samples_per_chunk >> 1;
                        const f32 pi_over_2N = PI / (f32)(N << 1);
                        const f32 N_plus_one_over_two = ((f32)N + 1.0f) * 0.5f;
                        FORI_NAMED(out, 0, samples_per_chunk >> 1) {
                            const f32 out_plus_one = 2.0f * (f32)out + 1.0f;
                            f32 acc = 0.0f;
                            FORI_NAMED(in, 0, samples_per_chunk) {
                                f32 angle = pi_over_2N * ((f32)in + N_plus_one_over_two) * out_plus_one;
                                
                                angle = f_mod(angle, PI * 2.0f);
                                acc += (f32)chunk_samples[in] * f_cos(angle);
                            }
                            scratch[out] = acc;
                        }
#else
                        // Fast MDCT
                        // MDCT preparation:
                        f32 *u = scratch;
                        
                        ssize n = (s32)actual_samples_per_chunk;
                        const ssize _3n_over_4 = (n * 3) >> 2;
                        FORI(0, n >> 2) {
                            u[i] = -(f32)chunk_samples[i + _3n_over_4];
                        }
                        FORI(n >> 2, n) {
                            u[i] = (f32)chunk_samples[i - (n >> 2)];
                        }
                        
                        // Twiddle factors:
                        f32 *A = push_array(game_block, f32, actual_samples_per_chunk);
                        f32 *B = push_array(game_block, f32, actual_samples_per_chunk);
                        f32 *C = push_array(game_block, f32, actual_samples_per_chunk);
                        
                        FORI(0, n >> 2) {
                            A[(i << 1)  ] = (float)  f_cos(f_mod(4*i*PI/n, 2.0f * PI));
                            A[(i << 1)+1] = (float) -f_sin(f_mod(4*i*PI/n, 2.0f * PI));
                            B[(i << 1)  ] = (float)  f_cos(f_mod(((i << 1)+1)*PI/n/2, 2.0f * PI));
                            B[(i << 1)+1] = (float)  f_sin(f_mod(((i << 1)+1)*PI/n/2, 2.0f * PI));
                        }
                        FORI(0, n >> 3) {
                            C[(i << 1)  ] = (float)  f_cos(f_mod(2*((i << 1)+1)*PI/n, 2.0f * PI));
                            C[(i << 1)+1] = (float) -f_sin(f_mod(2*((i << 1)+1)*PI/n, 2.0f * PI));
                        }
                        
                        // Kernel:
                        
                        // Step 1
                        FORI(0, n >> 2) {
                            const ssize four_k = i << 2;
                            const f32 A2k      = A[i << 1];
                            const f32 A2kplus1 = A[(i << 1) + 1];
                            const f32 u4k_minus_unminus4kminus1      = u[four_k    ] - u[n - four_k - 1];
                            const f32 u4kplus2_minus_unminus4kminus3 = u[four_k + 2] - u[n - four_k - 3];
                            
                            u[n - four_k - 1] = (u4k_minus_unminus4kminus1 * A2k) -
                                (u4kplus2_minus_unminus4kminus3 * A2kplus1);
                            u[n - four_k - 3] = (u4k_minus_unminus4kminus1 * A2kplus1) +
                                (u4kplus2_minus_unminus4kminus3 * A2k);
                        }
                        
                        
                        // Step 2
                        FORI(0, n >> 3) {
                            const ssize four_k = i << 2;
                            const ssize nover2_plus_4k  = (n >> 1) + four_k;
                            const ssize nover2_minus_4k = (n >> 1) - four_k;
                            const f32 Anover2_minus_4_minus_4k = A[n/2 - 4 - four_k];
                            const f32 Anover2_minus_3_minus_4k = A[n/2 - 3 - four_k];
                            
                            const f32 v_fourkplus1 = u[four_k + 1];
                            const f32 v_fourkplus3 = u[four_k + 3];
                            const f32 v_nover2plus3plus4k = u[nover2_plus_4k + 3];
                            const f32 v_nover2plus1plus4k = u[nover2_plus_4k + 1];
                            
                            u[nover2_plus_4k + 3] = v_nover2plus3plus4k + v_fourkplus3;
                            u[nover2_plus_4k + 1] = v_nover2plus1plus4k + v_fourkplus1;
                            u[four_k + 3] = (v_nover2plus3plus4k - v_fourkplus3) * Anover2_minus_4_minus_4k - 
                                (v_nover2plus1plus4k - v_fourkplus1) * Anover2_minus_3_minus_4k;
                            u[four_k + 1] = (v_nover2plus1plus4k - v_fourkplus1) * Anover2_minus_4_minus_4k + 
                                (v_nover2plus3plus4k - v_fourkplus3) * Anover2_minus_3_minus_4k;
                        }
                        
                        // Step 3
                        s32 ld; // log2(n)
                        
                        bitscan_forward(n, &ld);
                        FORI_NAMED(l, 0, ld - 3) {
                            const ssize k0 = n >> (l + 2);
                            const ssize k1 = (ssize)1 << (l + 3);
                            
                            FORI_NAMED(r, 0, n >> (l + 4)) {
                                const ssize four_r = r << 2;
                                FORI_NAMED(s, 0, (ssize)1 << (l + 1)) {
                                    const ssize two_s = s << 1;
                                    const f32 Ark1        = A[r * k1];
                                    const f32 Ark1_plus_1 = A[r * k1 + 1];
                                    const f32 w0 = u[n - 1 - k0 * two_s       - four_r];
                                    const f32 w1 = u[n - 1 - k0 * (two_s + 1) - four_r];
                                    const f32 w2 = u[n - 3 - k0 * two_s       - four_r];
                                    const f32 w3 = u[n - 3 - k0 * (two_s + 1) - four_r];
                                    
                                    u[n - 1 - k0 * two_s - four_r] = w0 + w1;
                                    u[n - 3 - k0 * two_s - four_r] = w2 + w3;
                                    
                                    u[n - 1 - k0 * (two_s + 1) - four_r] = (w0 - w1) * Ark1 - (w2 - w3) * Ark1_plus_1;
                                    u[n - 3 - k0 * (two_s + 1) - four_r] = (w2 - w3) * Ark1 + (w0 - w1) * Ark1_plus_1;
                                }
                            }
                        }
                        
                        // Step 4
                        FORI(1, (n >> 3) - 1) {
                            // stb_vorbis uses ld - 3???
                            const ssize j = bit_reverse(i, ld - 3);
                            if(i < j) {
                                const ssize _8j = (j << 3);
                                const ssize _8i = (i << 3);
                                
                                const f32 u0 = u[_8i + 1];
                                const f32 u1 = u[_8j + 1];
                                const f32 u2 = u[_8i + 3];
                                const f32 u3 = u[_8j + 3];
                                const f32 u4 = u[_8i + 5];
                                const f32 u5 = u[_8j + 5];
                                const f32 u6 = u[_8i + 7];
                                const f32 u7 = u[_8j + 7];
                                
                                u[_8j + 1] = u0;
                                u[_8i + 1] = u1;
                                u[_8j + 3] = u2;
                                u[_8i + 3] = u3;
                                u[_8j + 5] = u4;
                                u[_8i + 5] = u5;
                                u[_8j + 7] = u6;
                                u[_8i + 7] = u7;
                            }
                        }
                        
                        // Step 5
                        FORI(0, n >> 1) {
                            u[i] = u[(i << 1) + 1];
                        }
                        
                        // Step 6
                        FORI(0, n >> 3) {
                            const ssize _2k = i << 1;
                            const ssize _4k = i << 2;
                            const f32 w0 = u[_4k];
                            const f32 w1 = u[_4k + 1];
                            const f32 w2 = u[_4k + 2];
                            const f32 w3 = u[_4k + 3];
                            
                            u[n - 1 - _2k] = w0;
                            u[n - 2 - _2k] = w1;
                            u[((n * 3) >> 2) - 1 - _2k] = w2;
                            u[((n * 3) >> 2) - 2 - _2k] = w3;
                        }
                        
                        // Step 7
                        FORI(0, n >> 3) {
                            const ssize _2k      = i << 1;
                            const ssize n_over_2 = n >> 1;
                            const f32 C2k      = C[_2k];
                            const f32 C2kplus1 = C[_2k + 1];
                            
                            const f32 u0 = u[n_over_2 + _2k    ];
                            const f32 u1 = u[n_over_2 + _2k + 1];
                            const f32 u2 = u[n - 2    - _2k    ];
                            const f32 u3 = u[n - 2    - _2k + 1];
                            const f32 u4 = u[n - 1    - _2k    ];
                            
                            
                            u[n_over_2 + _2k]     = (u0 + u2 +
                                                     C2kplus1 * (u0 - u2) +
                                                     C2k * (u1 + u3)) * 0.5f;
                            u[n_over_2 + _2k + 1] = (u1 - u4 +
                                                     C2kplus1 * (u1 + u4) -
                                                     C2k * (u0 - u2)) * 0.5f;
                            u[n - 2 - _2k] = (u0 + u2 -
                                              C2kplus1 * (u0 - u2) -
                                              C2k * (u1 + u3)) * 0.5f;
                            u[n - 1 - _2k] = (-u1 + u4 +
                                              C2kplus1 * (u1 + u4) -
                                              C2k * (u0 - u2)) * 0.5f;
                        }
                        
                        // Step 8
                        FORI(0, n >> 2) {
                            const f32 B2k      = B[i << 1];
                            const f32 B2kplus1 = B[2 * i + 1];
                            const f32 v0 = u[(i << 1) + (n >> 1)];
                            const f32 v1 = u[(i << 1) + (n >> 1) + 1];
                            
                            u[i] = v0 * B2k + v1 * B2kplus1;
                            u[(n >> 1) - 1 - i] = v0 * B2kplus1 - v1 * B2k;
                        }
#endif
                        
#if 0
                        {
                            SCOPE_MEMORY(game_block);
                            string output = push_string(game_block, 30 + 50 * samples_per_chunk);
                            usize printed = 0;
                            printed += print(output.data + printed, output.length - printed, "Some title\n");
                            output.length = printed;
                            os_platform.write_entire_file(STRING("blah"), output);
                        }
#endif
                        __debugbreak();
                    }
                }
                
                
                usize final_compressed_size = facs_file.length;
                os_platform.print(tprint("File size: %u -> %u (%f%%).\n",
                                         facs_file.length,
                                         final_compressed_size,
                                         ((f64)facs_file.length / (f64)final_compressed_size * 100.0)));
                program_state->should_close = true;
            }
            
            
            data_file_count += wav_file_count;
        }
        
#if 0
        // Compression benchmark.
        {
            string comp_names[] = {
                STRING("silesia/dickens"),
                STRING("silesia/mozilla"),
                STRING("silesia/mr"),
                STRING("silesia/nci"),
                STRING("silesia/ooffice"),
                STRING("silesia/osdb"),
                STRING("silesia/reymont"),
                STRING("silesia/samba"),
                STRING("silesia/sao"),
                STRING("silesia/webster"),
                STRING("silesia/xml"),
                STRING("silesia/x-ray"),
            };
            
            string uncompressed_files[ARRAY_LENGTH(comp_names)] = {};
            string compressed_files[ARRAY_LENGTH(comp_names)] = {};
            u64 decode_cycles[ARRAY_LENGTH(comp_names)] = {};
            
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                string uncompressed = os_platform.read_entire_file(comp_names[i]);
                string compressed = fastcomp_compress(uncompressed, game_block);
                uncompressed_files[i] = uncompressed;
                compressed_files[i] = compressed;
            }
            
            u8 *decode_buffer = push_array(game_block, u8, KiB(256));
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                string compressed = compressed_files[i];
                u64 tstart = READ_CPU_CLOCK();
                fastcomp_decompress(compressed, decode_buffer);
                u64 tend = READ_CPU_CLOCK();
                u64 dcy = tend - tstart;
                decode_cycles[i] = dcy;
            }
            
            u8 print_buffer[512];
            string prt;
            prt.data = print_buffer;
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                prt.length = print(print_buffer, 512, "%s : %u -> %u, %ucy\n",
                                   comp_names[i].data, uncompressed_files[i].length,
                                   compressed_files[i].length, decode_cycles[i]);
                os_platform.print(prt);
            }
        }
#endif
        
        { // Texture files
            // Input data needed for a file:
            string bmp_names[TEXTURE_UID_COUNT] = {
                STRING("textures/blank.bmp"),
                STRING("textures/untextured.bmp"),
                STRING("textures/circle.bmp"),
                STRING("textures/default_hitbox.bmp"),
                STRING("textures/default_hurtbox.bmp"),
                STRING("textures/default_solid.bmp"),
                STRING("textures/player.bmp"),
                STRING("textures/xhair.bmp"),
                STRING("textures/grid_tile.bmp"),
            };
            // Debug textures should have a scale of 1.0f so as to preserve input dimensions.
            v2 bmp_halfdims[TEXTURE_UID_COUNT] = {
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(0.02f * INV_GAME_ASPECT_RATIO, 0.02f),
                V2(1.0f),
            };
            // We specify these here as 0-1 and then prebake the multiplication by halfdim.
            v2 bmp_offsets[TEXTURE_UID_COUNT] = {
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
            };
            const u32 bmp_count = TEXTURE_UID_COUNT;
            for(s32 i = 0; i < bmp_count; ++i) {
                bmp_offsets[i] = v2_hadamard_prod(bmp_offsets[i], bmp_halfdims[i]);
            }
            string ta = files_convert_bmp_to_ta(bmp_names, bmp_halfdims, bmp_offsets, bmp_count, game_block);
            data_files[data_file_count++] = ta;
        }
        
        { // Levels
            { // 1
                usize allocated_size = KiB(4);
                string output = push_string(game_block, allocated_size);
                
                v2 map_halfdim = V2(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
                
                const s32 player_count = 1;
                
                v2 player_ps[player_count] = {
                    V2(),
                };
                s32 player_partner_counts[player_count] = {
                    3,
                };
                
                const s32 partner_count = 4;
                
                s32 player_partners[player_count][partner_count] = {
                    {
                        0, 1, 2, 3,
                    },
                };
                v2 partner_offsets[partner_count] = {
                    V2(-1.0f, 0.0f),
                    V2(0.0f, -1.0f),
                    V2(1.0f, 0.0f),
                    V2(0.0f, 1.0f),
                };
                
                const s32 turret_count = 1;
                
                v2 turret_ps[turret_count] = {
                    V2(0.0f, 2.0f),
                };
                f32 turret_cooldowns[turret_count] = {
                    1.0f,
                };
                
                
                const s32 dog_count = 1;
                
                v2 dog_ps[dog_count] = {
                    V2(2.0f),
                };
                
                const s32 wall_turret_count = 1;
                
                v2 wall_turret_ps[wall_turret_count] = {
                    V2(-1.0f),
                };
                
                /* @Cleanup: Do we care about trees?
                const s32 tree_count = 1;
                
                v2 
                */
                
                const s32 target_count = 1;
                
                v2 target_ps[target_count] = {
                    V2(-3.0f, 2.2f),
                };
                
                Serialised_Level_Data level;
                
                // @Cleanup: Use layout_serialised_level_data()
                
                level.header = (Level_Header *)output.data;
                
                level.player_ps = (v2 *)(level.header + 1);
                level.partner_offsets = (v2 *)(level.player_ps + player_count);
                level.turret_ps = (v2 *)(level.partner_offsets + partner_count);
                level.turret_cooldowns = (f32 *)(level.turret_ps + turret_count);
                level.dog_ps = (v2 *)(level.turret_cooldowns + turret_count);
                level.wall_turret_ps = (v2 *)(level.dog_ps + dog_count);
                // @Cleanup: Do we care about trees?
                level.target_ps = (v2 *)(level.wall_turret_ps + wall_turret_count);
                
                u8 *end_of_file = (u8 *)(level.target_ps + target_count);
                usize file_size = end_of_file - output.data;
                ASSERT(file_size < allocated_size);
                
                level.header->version = 1;
                level.header->index = 1;
                level.header->map_halfdim = map_halfdim;
                level.header->player_count = player_count;
                level.header->partner_count = partner_count;
                level.header->turret_count = turret_count;
                level.header->dog_count = dog_count;
                level.header->wall_turret_count = wall_turret_count;
                level.header->target_count = target_count;
                
                for(s32 i = 0; i < player_count; ++i) {
                    s32 this_partner_count = player_partner_counts[i];
                    
                    level.player_ps[i] = player_ps[i];
                }
                for(s32 i = 0; i < partner_count; ++i) {
                    level.partner_offsets[i] = partner_offsets[i];
                }
                for(s32 i = 0; i < turret_count; ++i) {
                    level.turret_ps[i] = turret_ps[i];
                    level.turret_cooldowns[i] = turret_cooldowns[i];
                }
                for(s32 i = 0; i < dog_count; ++i) {
                    level.dog_ps[i] = dog_ps[i];
                }
                FORI(0, wall_turret_count) {
                    level.wall_turret_ps[i] = wall_turret_ps[i];
                }
                for(s32 i = 0; i < target_count; ++i) {
                    level.target_ps[i] = target_ps[i];
                }
                
                data_files[data_file_count++] = output;
            }
        }
        
        { // Writing a datapack:
            string pack_name = STRING("1.datapack");
            ASSERT(data_file_count == ASSET_UID_COUNT);
            usize file_entries = data_file_count + 1;
            usize write_offset = sizeof(Datapack_Header) + sizeof(Datapack_Asset_Info) * file_entries;
            Datapack_Header header;
            header.version = 0;
            usize data_files_total_size = 0;
            
            for(usize i = 0; i < data_file_count; ++i) {
                data_files_total_size += data_files[i].length;
            }
            
            string pack_file = push_string(game_block, data_files_total_size + write_offset);
            Datapack_Asset_Info *asset_table = (Datapack_Asset_Info *)(((Datapack_Header *)pack_file.data) + 1);
            
            for(usize i = 0; i < data_file_count; ++i) {
                string source = data_files[i];
                ASSERT(source.data && source.length);
                
                Datapack_Asset_Info entry = {};
                // @Cleanup: We technically support more than 32-bit offsets.
                ASSERT(write_offset < U32_MAX);
                entry.location_low = (u32)write_offset;
                entry.compression = DATAPACK_COMPRESSION_NONE;
                asset_table[i] = entry;
                
                mem_copy(source.data, pack_file.data + write_offset, source.length);
                
                write_offset += source.length;
            }
            
            Datapack_Asset_Info last_entry;
            last_entry.all = write_offset;
            asset_table[data_file_count] = last_entry;
            
            *(Datapack_Header *)pack_file.data = header;
            ASSERT(write_offset == data_files_total_size + sizeof(Datapack_Header) + sizeof(Datapack_Asset_Info) * file_entries);
            
            os_platform.write_entire_file(pack_name, pack_file);
        }
    }
#endif
    
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
    
    { // Getting assets from the datapack:
        Datapack_Handle pack = g_cl->assets.datapack;
        
        { // TA:
            Asset_Metadata meta = get_asset_metadata(&pack, ASSET_UID_test_ta);
            
            string ta = push_string(game_block, meta.size);
            os_platform.read_file(meta.location.file_handle, ta.data, meta.location.offset, meta.size);
            
            ta_to_texture_ids(ta, render_info, render_info->textures_by_uid);
            
            render_info->blank_texture_id = FIND_TEXTURE_ID(render_info, blank);
            render_info->   untextured_id = FIND_TEXTURE_ID(render_info, untextured);
            render_info->       circle_id = FIND_TEXTURE_ID(render_info, circle);
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
    
    {
        g_cl->time_scale = 1.0f;
    }
    
    {
        for(s32 i = 0; i < 16; ++i) {
            os_platform.add_thread_job(&Implicit_Context::test_thread_job, (void *)(u64)i);
        }
    }
    
    
    // This is to make sure that game input is properly initted.
    close_menu(g_cl);
    
    info->clock.cur_time = os_platform.get_seconds();
    info->clock.sim_time = info->clock.cur_time;
}

static void draw_xhair(Rendering_Info * const render_info, const Texture_ID xhair_texture, const v2 xhairp) {
    ASSERT(f_in_range(xhairp.x, 0.0f, 1.0f) && f_in_range(xhairp.y, 0.0f, 1.0f));
    queue_quad(render_info, xhair_texture, xhairp, V2(1.0f), V2(1.0f, 0.0f), V4(1.0f));
}

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
    Game_Block_Info * const info        = (Game_Block_Info *)game_block->mem;
    Game_Client     * const g_cl        = &info->client;
    Rendering_Info  * const render_info = &info->render_info;
    Audio_Info      * const  audio_info = &info->audio;
    Text_Info       * const   text_info = &info->text;
    Menu            * const menu        = &info->menu;
    
    // Clock:
    const f64 cur_time = os_platform.get_seconds();
    const f64 sim_time = info->clock.sim_time;
    
    f64        sim_dt = cur_time -             sim_time;
    const f64 real_dt = cur_time - info->clock.cur_time;
    
    f64 this_frame_sim_time = 0.0;
    
    const f64 game_time_scale = (f64)g_cl->time_scale;
    sim_dt *= game_time_scale;
    
    // Log:
    if(!global_log) {
        global_log = &info->log;
    }
    global_log->temporary_buffer.length = 0;
    global_log->heads_up_alpha -= (f32)real_dt;
    
    temporary_memory.used = 0;
    
    { // Parsing the program state:
        if(program_state) {
            maybe_update_render_size(render_info, program_state->window_size);
            mem_copy(&render_info->draw_rect, &program_state->draw_rect, sizeof(rect2s));
        }
    }
    
    const u32 current_player = g_cl->g.current_player;
    // NOTE: We copy input because it's overall more robust to transitions.
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
            close_menu(g_cl);
            // If we don't do this, we send the same input to both the game and the menu,
            // which results in double presses which can lead to the menu reopening on the same
            // frame.
            advance_input(&game_input);
        } else {
            open_menu(g_cl);
        }
    }
    
    { // Game update:
        TIME_BLOCK;
        
        if(g_cl->in_menu) {
            update_menu(menu, render_info, &menu_input, program_state);
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
                g_full_update(&g_cl->g, &game_input, PHYSICS_DT, audio_info, &g_cl->assets.datapack);
                
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
                
                clone_game_state(&g_cl->g, &g_cl->visual_state);
                g_full_update(&g_cl->visual_state, &clone_input, frame_remainder, 0, 0);
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
    
    {
        Entire_Sound_Update_Payload payload;
        payload.audio = audio_info;
        payload.dt = (f32)real_dt;
        info->sound_update_payload = payload;
        os_platform.add_thread_job(&Implicit_Context::entire_sound_update, &info->sound_update_payload);
    }
    
    // Rendering:
    if(program_state->should_render) {
        queue_clear(render_info);
        
        draw_game(render_info, &g_cl->visual_state, &g_cl->assets);
        
        queue_shader(render_info, SHADER_INDEX_TEXTURE);
        queue_transform(render_info, TRANSFORM_INDEX_RIGHT_HANDED_UNIT_SCALE);
        
        // Drawing UI:
        if(g_cl->in_menu) {
            draw_menu(menu, g_cl, render_info, text_info);
        }
        
        if((global_log->heads_up_buffer.length > 0) && (global_log->heads_up_alpha > 0.0f)) {
            text_add_string(text_info, render_info, global_log->heads_up_buffer, V2(0.5f, 0.8f), 0.15f, FONT_MONO, true);
        }
        
        if(text_info->strings.count) {
            text_update(text_info, render_info);
        }
        
        draw_queue(render_info);
    }
    
#if GENESIS_DEV
    profile_update(profiler, program_state, text_info, render_info, &profiler_input);
    
    if(profiler->has_focus) {
        program_state->input = profiler_input;
    }
#endif
    
    ASSERT(sim_time <= cur_time);
    info->clock.cur_time = cur_time;
    info->clock.sim_time += this_frame_sim_time / game_time_scale;
    
    if(program_state->should_close) {
        { // Update the config.
            User_Config config = {};
            config.version = USER_CONFIG_VERSION;
            config.fullscreen = program_state->should_be_fullscreen;
            config.window_dim.x = (s16)program_state->window_size[0];
            config.window_dim.y = (s16)program_state->window_size[1];
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
    gl_load_needed_functions(os_import->opengl.all_functions);
    return game_export;
}
