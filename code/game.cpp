
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
#include           "bitfield.h"
#include                "log.h"
#include            "profile.h"
#include                "bmp.h"
#include              "input.h"
#include               "font.h" // @Cleanup
#include             "assets.h"
#include              "audio.h" // @Cleanup
#include              "synth.h"
#include   "compression_fast.h"
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
#include         "bitfield.cpp"
#include              "log.cpp"
#include              "bmp.cpp"
#include            "input.cpp"
#include       "render_new.cpp"
#include             "mesh.cpp"
#include           "assets.cpp"
#include            "audio.cpp" // @Cleanup
#include            "synth.cpp"
#include "compression_fast.cpp"
// #include          "profile.cpp"

static inline v2 unit_scale_to_world_space_offset(v2 p, f32 camera_zoom) {
    v2 result = p - V2(0.5f);
    f32 inv_camera_zoom = 1.0f / camera_zoom;
    result.x *= GAME_ASPECT_RATIO_X * inv_camera_zoom;
    result.y *= GAME_ASPECT_RATIO_Y * inv_camera_zoom;
    return result;
}

static inline v2 world_space_offset_to_unit_scale(v2 p, f32 camera_zoom) {
    v2 camera_dim = V2(GAME_ASPECT_RATIO_X, GAME_ASPECT_RATIO_Y) * (1.0f / camera_zoom);
    
    v2 result = (p + camera_dim * 0.5f);
    result.x /= camera_dim.x;
    result.y /= camera_dim.y;
    
    return result;
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

static inline void update_player_dp(const f32 input_dp, f32 * const _dp, const f32 accel, const f32 max_speed, const f32 dt32) {
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

//
// Some collision stuff
//

static inline void init_bresenham(s32 a_x, s32 a_y, s32 b_x, s32 b_y, s32 *out_at_x, s32 *out_at_y,
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
static inline void do_bresenham(s32 *at_x, s32 *at_y, s32 *deviation, s32 dx, s32 dy, s32 xinc, s32 yinc) {
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

static inline void ray_to_outer_box_intersect(v2 ray0, v2 dir, f32 left, f32 bottom, f32 right, f32 top, v2 *intersect) {
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

static inline bool aabb_aabb_overlap(v2 pa, v2 halfdima, v2 pb, v2 halfdimb) {
    v2 minkowski_p = pa - pb;
    v2 minkowski_halfdim = halfdima + halfdimb;
    
    f32 left = minkowski_p.x - minkowski_halfdim.x;
    f32 right = minkowski_p.x + minkowski_halfdim.x;
    f32 bottom = minkowski_p.y - minkowski_halfdim.y;
    f32 top = minkowski_p.y + minkowski_halfdim.y;
    
    return ((left < 0.0f) && (right > 0.0f) &&
            (bottom < 0.0f) && (top > 0.0f));
}

static inline v2 aabb_aabb_response(v2 pa, v2 halfdima, v2 pb, v2 halfdimb) {
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

static inline v2 edge_closest(const v2 a, const v2 b, const v2 p) {
    const v2 ab = b - a;
    const v2 ap = p - a;
    const f32 abab = v2_inner(ab, ab);
    const f32 abap = v2_inner(ab, ap);
    const f32 t = f_clamp(abap / abab, 0.0f, 1.0f);
    const v2 closest = a + ab * t;
    return closest;
}

static inline v2 obb_closest(const v2 p, const v2 halfdim, const v2 rotation, const v2 to) {
    const v2 rot_perp = v2_perp(rotation);
    const v2 dp = to - p;
    f32 dotx = v2_inner(dp, rotation);
    f32 doty = v2_inner(dp, rot_perp);
    dotx = f_symmetrical_clamp(dotx, halfdim.x);
    doty = f_symmetrical_clamp(doty, halfdim.y);
    const v2 closest = p + rotation * dotx + rot_perp * doty;
    return closest;
}

static inline bool disk_obb_overlap_radius_squared(const v2 disk_p, const f32 radius_sq, const v2 obb_p, const v2 halfdim, const v2 rotation) {
    const v2 closest = obb_closest(obb_p, halfdim, rotation, disk_p);
    const f32 dist_sq = v2_length_sq(disk_p - closest);
    return (dist_sq < radius_sq);
}

static inline bool disk_obb_overlap(const v2 disk_p, const f32 radius, const v2 obb_p, const v2 halfdim, const v2 rotation) {
    return disk_obb_overlap_radius_squared(disk_p, radius * radius, obb_p, halfdim, rotation);
}

static inline bool disk_disk_overlap(const v2 pa, const f32 radiusa, const v2 pb, const f32 radiusb) {
    const f32 dist_sq = v2_length_sq(pa - pb);
    const f32 minkowski_rad = radiusa + radiusb;
    const f32 radius_sq = minkowski_rad * minkowski_rad;
    return (dist_sq < radius_sq);
}

static inline bool disk_capsule_overlap(const v2 p, const f32 disk_radius, const v2 a, const v2 b, const f32 capsule_radius) {
    const v2 closest = edge_closest(a, b, p);
    const f32 dist_sq = v2_length_sq(p - closest);
    const f32 minkowski_rad = disk_radius + capsule_radius;
    const f32 minkowski_rad_sq = minkowski_rad * minkowski_rad;
    return (dist_sq < minkowski_rad_sq);
}

//
// Collision
//

static inline u32 get_collision_indices(Game_Collision *collision, u16 handle) {
    ASSERT(handle);
    u32 result = collision->handle_minus_one_to_indices[handle - 1];
    
    ASSERT(result);
    
    return result;
}

static u16 add_collision(Game_Collision *collision) {
    FORI(0, MAX_COLLISION_VOLUME_COUNT) {
        u32 indices = collision->handle_minus_one_to_indices[i];
        
        if(!indices) {
            u16 handle = CAST(u16, i + 1);
            
            ASSERT(collision->plane_count < MAX_COLLISION_VOLUME_COUNT);
            indices = collision->plane_count++;
            indices |= (collision->plane_count++ & 0xFFFF) << 16;
            
            collision->handle_minus_one_to_indices[i] = indices;
            
            return handle;
        }
    }
    return 0;
}

static void update_collision(Game_Collision *collision, u16 handle, Entity_Reference reference, v2 p, f32 radius) {
    u32 indices = get_collision_indices(collision, handle);
    Collision_Plane *left = &collision->planes[indices & 0xFFFF];
    Collision_Plane *right = &collision->planes[indices >> 16];
    
    left->x = p.x - radius;
    left->handle = handle;
    left->left = true;
    right->x = p.x + radius;
    right->handle = handle;
    right->left = false;
    
    collision->references[handle] = reference;
}

static inline void remove_collision(Game_Collision *collision, u16 handle) {
    ASSERT(collision->removal_count < MAX_COLLISION_VOLUME_COUNT);
    collision->removals[collision->removal_count++] = handle;
}

static void overlap_update(Game_Collision *collision) {
    { // Removals.
        u16 to_remove[512];
        s32 to_remove_count = 0;
        
        // @Speed? We're sorting the indices to remove in decreasing order so that
        // unordered remove still works.
        FORI(0, collision->removal_count) {
            u16 handle = collision->removals[i];
            
            u32 indices = get_collision_indices(collision, handle);
            
            to_remove[to_remove_count++] = indices & 0xFFFF;
            to_remove[to_remove_count++] = indices >> 16;
            
            collision->handle_minus_one_to_indices[handle - 1] = 0;
        }
        
        FORI_NAMED(_, 0, to_remove_count) {
            FORI_NAMED(i, 0, to_remove_count - 1) {
                if(to_remove[i] < to_remove[i+1]) {
                    SWAP(to_remove[i], to_remove[i+1]);
                }
            }
        }
        
        FORI(0, to_remove_count) {
            collision->planes[to_remove[i]] = collision->planes[--collision->plane_count];
        }
        
        collision->removal_count = 0;
    }
    
    { // Sort the planes. @Speed: Bubble sort.
        // @Speed: A merge sort would be good not only complexity-wise, but also because we could
        // put entities into tiles in a sorted order so we can just do a single merge when adding
        // new tiles to the working set.
        FORI_NAMED(_, 0, collision->plane_count) {
            FORI_NAMED(i, 0, collision->plane_count - 1) {
                if(collision->planes[i].x > collision->planes[i+1].x) {
                    SWAP(collision->planes[i], collision->planes[i+1]);
                }
            }
        }
    }
    
    { // Overlap check.
        s32 overlap_pair_count = 0;
        
        u16 open_spans[MAX_COLLISION_VOLUME_COUNT];
        s32 open_span_count = 0;
        
        FORI(0, collision->plane_count) {
            Collision_Plane *plane = &collision->planes[i];
            u32 indices = get_collision_indices(collision, plane->handle);
            
            if(plane->left) {
                FORI_NAMED(span_index, 0, open_span_count) {
                    ASSERT(overlap_pair_count < MAX_COLLISION_VOLUME_COUNT * 4);
                    collision->overlap_pairs[overlap_pair_count  ][0] = open_spans[span_index];
                    collision->overlap_pairs[overlap_pair_count++][1] = plane->handle;
                }
                
                open_spans[open_span_count++] = plane->handle;
                
                indices &= ~0xFFFF;
                indices |= i & 0xFFFF;
            } else {
                FORI_NAMED(span_index, 0, open_span_count) {
                    if(open_spans[span_index] == plane->handle) {
                        open_spans[span_index] = open_spans[--open_span_count];
                        break;
                    }
                }
                
                indices &= 0xFFFF;
                indices |= i << 16;
            }
            
            collision->handle_minus_one_to_indices[plane->handle-1] = indices;
        }
        
        collision->overlap_pair_count = overlap_pair_count;
    }
    
    { // Translating handles to references.
        FORI(0, collision->overlap_pair_count) {
            collision->results[i][0] = collision->references[collision->overlap_pairs[i][0]];
            collision->results[i][1] = collision->references[collision->overlap_pairs[i][1]];
        }
    }
}

//
//
//

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

static void mark_entity_for_removal(Game *g, Entity_Reference reference) {
    // Checking that we don't mark the same entity multiple times. @Speed?
    FORI(0, g->entity_removal_count) {
        Entity_Reference *removal = &g->entity_removals[i];
        if((removal->type == reference.type) && (removal->index == reference.index)) {
            return;
        }
    }
    
    Entity_Reference *removal = &g->entity_removals[g->entity_removal_count++];
    ASSERT(g->entity_removal_count < MAX_LIVE_ENTITY_COUNT);
    *removal = reference;
}

static inline void mark_entity_for_removal(Game *g, u8 type, s32 index) {
    Entity_Reference reference = make_entity_reference(type, index);
    mark_entity_for_removal(g, reference);
}

struct V2_Closest_Result {
    v2 closest;
    f32 dist_sq;
};

static void v2_closest(v2 from, v2 *array, s32 count, V2_Closest_Result *result) {
    ASSERT(result);
    v2 closest = from;
    f32 best_dist_sq = 0.0f;
    
    if(count) {
        closest = array[0];
        best_dist_sq = v2_length_sq(closest - from);
        
        FORI(0, count) {
            const v2 it = array[i];
            const f32 dist_sq = v2_length_sq(it - from);
            
            if(dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                closest = it;
            }
        }
    }
    
    result->closest = closest;
    result->dist_sq = best_dist_sq;
}

static inline v2 sdf_to_tile(v2 sdf_p) {
    v2 result = (sdf_p * SDF_TO_GAME_FACTOR) - V2(MAP_TILE_HALFDIM);
    ASSERT(f_in_range(result.x, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM) && f_in_range(result.y, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM));
    return result;
}

static inline v2 tile_to_sdf(v2 game_p) {
    ASSERT(f_in_range(game_p.x, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM) && f_in_range(game_p.y, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM));
    v2 result = (game_p + V2(MAP_TILE_HALFDIM)) * GAME_TO_SDF_FACTOR;
    return result;
}

static inline v2s working_set_to_cell(v2 p) {
    v2s result;
    result.x = f_floor_s((p.x + WORKING_SET_HALFDIM) * GAME_TO_SDF_FACTOR);
    result.y = f_floor_s((p.y + WORKING_SET_HALFDIM) * GAME_TO_SDF_FACTOR);
    return result;
}
static inline v2 sdf_to_working_set(v2s cell, v2 uv) {
    v2 result;
    
    // @Float
    result.x = ((CAST(f32, cell.x) + uv.x) * SDF_TO_GAME_FACTOR) - WORKING_SET_HALFDIM;
    result.y = ((CAST(f32, cell.y) + uv.y) * SDF_TO_GAME_FACTOR) - WORKING_SET_HALFDIM;
    
    return result;
}
static inline Precise_Position global_to_precise(v2f64 global) {
    Precise_Position result;
    
    result.tile.x = f64_floor_s(global.x / CAST(f64, MAP_TILE_DIM));
    result.tile.y = f64_floor_s(global.y / CAST(f64, MAP_TILE_DIM));
    
    result.local.x = CAST(f32, global.x - CAST(f64, result.tile.x) * CAST(f64, MAP_TILE_DIM));
    result.local.y = CAST(f32, global.y - CAST(f64, result.tile.y) * CAST(f64, MAP_TILE_DIM));
    
    ASSERT(f_in_range(result.local.x, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM) && f_in_range(result.local.y, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM));
    
    return result;
}
static inline v2 precise_to_working_set(Precise_Position precise, v2s working_set_tile_p) {
    v2 result = precise.local + v2_hadamard_prod(V2(MAP_TILE_DIM), V2(precise.tile - working_set_tile_p));
    return result;
}
static inline v2 global_to_working_set(v2f64 global, v2s working_set_tile_p) {
    return precise_to_working_set(global_to_precise(global), working_set_tile_p);
}

static inline void working_set_to_sdf(v2 p, v2s *cell, v2 *uv) {
    // @Float
    v2 uncentered = (p + V2(WORKING_SET_HALFDIM)) * GAME_TO_SDF_FACTOR;
    
    cell->x = f_floor_s(uncentered.x);
    cell->y = f_floor_s(uncentered.y);
    
    uv->x = uncentered.x - CAST(f32, cell->x);
    uv->y = uncentered.y - CAST(f32, cell->y);
}

static inline void sdf_sample4(f32 *sdf, s32 left, s32 bottom, f32 radius, f32 *dist) {
    ASSERT((left) < TILE_SDF_RESOLUTION);
    ASSERT((bottom) < TILE_SDF_RESOLUTION);
    dist[0] = sdf[ bottom      * PADDED_TILE_SDF_RESOLUTION +  left     ] - radius;
    dist[1] = sdf[ bottom      * PADDED_TILE_SDF_RESOLUTION + (left + 1)] - radius;
    dist[2] = sdf[(bottom + 1) * PADDED_TILE_SDF_RESOLUTION +  left     ] - radius;
    dist[3] = sdf[(bottom + 1) * PADDED_TILE_SDF_RESOLUTION + (left + 1)] - radius;
}

static u8 *push_entity(Game_Map_Storage *storage, Game_Tile *tile, ssize size) {
    Entity_Block *block = &tile->entities;
    while(block && ((ENTITY_BLOCK_DATA_SIZE - block->header.allocated) < 0)) {
        block = block->header.next;
    }
    
    if(!block) {
        // Grab a new block.
        block = storage->free_entity_blocks;
        
        ASSERT(block);
        storage->free_entity_blocks = block->header.next;
        
        block->header.next = tile->entities.header.next;
        block->header.count = 0;
        block->header.allocated = 0;
    }
    
    u8 *result = block->data + block->header.allocated;
    block->header.allocated += size;
    block->header.count += 1;
    
    return result;
}
#define PUSH_ENTITY(type, storage, tile) CAST(type *, push_entity(storage, tile, sizeof(type)))

#define PLACE_ENTITY_BEGIN(type) Precise_Position precise = global_to_precise(global_p); \
Game_Tile *tile = get_stored_tile(storage, precise.tile); \
type *entity = PUSH_ENTITY(type, storage, tile);

static void place_tree(Game_Map_Storage *storage, v2f64 global_p) {
    PLACE_ENTITY_BEGIN(Serialized_Tree);
    
    entity->header.type = Entity_Type::TREE;
    entity->p = precise.local;
}

static void place_item(Game_Map_Storage *storage, v2f64 global_p, Item_Contents contents) {
    PLACE_ENTITY_BEGIN(Serialized_Item);
    
    entity->header.type = Entity_Type::ITEM;
    entity->p = precise.local;
    entity->contents = contents;
}

static void place_turret(Game_Map_Storage *storage, v2f64 global_p) {
    PLACE_ENTITY_BEGIN(Serialized_Turret);
    
    entity->header.type = Entity_Type::TURRET;
    entity->p = precise.local;
}

#undef PLACE_ENTITY_BEGIN

static void load_dumb_level(Game *g) {
    Game_Map_Storage *storage = &g->storage;
    
    // Level load:
    g->current_level_index = 1;
    g->camera_zoom = 1.0f;
    g->map_dim = V2(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
    
    {
        Game_Collision *collision = &g->collision;
    }
    
    { // Spawning the player and positioning the working set.
        v2f64 player_global_p = V2F64(0.5f, -0.5f);
        
        Precise_Position precise = global_to_precise(player_global_p + V2F64(g->map_dim) * 0.5);
        
        g->working_set_tile_p = precise.tile;
        
        g->PLAYER.count = 1;
        g->PLAYER.ps[0] = precise_to_working_set(precise, g->working_set_tile_p);
        g->PLAYER.rots[0] = V2(1.0f, 0.0f);
        g->PLAYER.weapons[0].type = Weapon_Type::SLASH;
        g->PLAYER.collision_handles[0] = add_collision(&g->collision);
    }
    
    { // Initializing the static geometry SDF.
        s32 tile_bias_x = storage->dim.x;
        
        any32 default_distance;
        default_distance.f = TILE_SDF_MAX_DISTANCE;
        FORI(0, storage->dim.x * storage->dim.y) {
            mem_set(storage->tiles[i].sdf, default_distance.s, sizeof(f32) * PADDED_TILE_SDF_RESOLUTION * PADDED_TILE_SDF_RESOLUTION);
        }
        
        v2f64 tree_global_ps[] = {
            V2F64(-1.0),
            V2F64(-1.3, 1.0),
            V2F64(3.0, -0.5),
        };
        
        FORI_NAMED(tree_index, 0, ARRAY_LENGTH(tree_global_ps)) {
            v2f64 global_p = tree_global_ps[tree_index];
            
            // Map-relative coordinates
            Precise_Position precise = global_to_precise(global_p + V2F64(g->map_dim) * 0.5);
            
            rect2 aabb = rect2_phalfdim(precise.local, V2(TREE_RADIUS + TILE_SDF_MAX_DISTANCE));
            
            v2 sdf_p = tile_to_sdf(precise.local);
            
            rect2 sdf_aabb_f = rect2_phalfdim(sdf_p, V2((TREE_RADIUS + TILE_SDF_MAX_DISTANCE) * GAME_TO_SDF_FACTOR));
            rect2s sdf_aabb = rect2s_conservative_aabb(sdf_aabb_f);
            
            s32 min_tile_x = f_floor_s(aabb.left / MAP_TILE_DIM);
            s32 min_tile_y = f_floor_s(aabb.bottom / MAP_TILE_DIM);
            s32 max_tile_x = f_ceil_s(aabb.right / MAP_TILE_DIM);
            s32 max_tile_y = f_ceil_s(aabb.top / MAP_TILE_DIM);
            
            ASSERT((precise.tile.x - min_tile_x) >= 0);
            ASSERT((precise.tile.y - min_tile_y) >= 0);
            ASSERT((precise.tile.x + max_tile_x) < storage->dim.x);
            ASSERT((precise.tile.y + max_tile_y) < storage->dim.y);
            
            g->TREE.ps[tree_index] = precise_to_working_set(precise, g->working_set_tile_p);
            
            FORI_TYPED_NAMED(s32, rel_tile_y, min_tile_y, max_tile_y + 1) {
                FORI_TYPED_NAMED(s32, rel_tile_x, min_tile_x, max_tile_x + 1) {
                    v2s tile_p = V2S(precise.tile.x + rel_tile_x, precise.tile.y + rel_tile_y);
                    f32 *sdf = get_stored_tile(&g->storage, tile_p)->sdf;
                    
                    rect2s local_aabb = sdf_aabb;
                    local_aabb.left -= rel_tile_x * TILE_SDF_RESOLUTION;
                    local_aabb.bottom -= rel_tile_y * TILE_SDF_RESOLUTION;
                    local_aabb.right -= rel_tile_x * TILE_SDF_RESOLUTION;
                    local_aabb.top -= rel_tile_y * TILE_SDF_RESOLUTION;
                    
                    local_aabb.left = s_max(local_aabb.left, 0);
                    local_aabb.bottom = s_max(local_aabb.bottom, 0);
                    local_aabb.right = s_min(local_aabb.right, TILE_SDF_RESOLUTION - 1);
                    local_aabb.top = s_min(local_aabb.top, TILE_SDF_RESOLUTION - 1);
                    
                    FORI_TYPED_NAMED(s32, y, local_aabb.bottom, local_aabb.top + 1) {
                        FORI_TYPED_NAMED(s32, x, local_aabb.left, local_aabb.right + 1) {
                            f32 old_length = sdf[y * PADDED_TILE_SDF_RESOLUTION + x];
                            
                            v2 sd_p = V2(x, y);
                            sd_p = sdf_to_tile(sd_p);
                            
                            ASSERT(f_in_range(sd_p.x, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM) && f_in_range(sd_p.y, -MAP_TILE_HALFDIM, MAP_TILE_HALFDIM));
                            ASSERT((y * TILE_SDF_RESOLUTION + x) < (TILE_SDF_RESOLUTION * TILE_SDF_RESOLUTION));
                            f32 new_length = v2_length_or_zero(sd_p - precise.local) - TREE_RADIUS;
                            
                            if(new_length < old_length) {
                                sdf[y * PADDED_TILE_SDF_RESOLUTION + x] = new_length;
                            }
                        }
                    }
                }
            }
        }
        
        // @Speed!!!
        // Adding padding for bilinear filtering.
        FORI_REVERSE_TYPED_NAMED(s32, sdf_y, storage->dim.y - 2, 0) {
            FORI_REVERSE_TYPED_NAMED(s32, sdf_x, storage->dim.x - 2, 0) {
                f32 *sdf = get_stored_tile(&g->storage, V2S(sdf_x, sdf_y))->sdf;
                
                f32 *top_sdf = get_stored_tile(&g->storage, V2S(sdf_x, sdf_y + 1))->sdf;
                f32 *right_sdf = get_stored_tile(&g->storage, V2S(sdf_x + 1, sdf_y))->sdf;
                
                FORI_NAMED(cell_x, 0, PADDED_TILE_SDF_RESOLUTION) {
                    sdf[(PADDED_TILE_SDF_RESOLUTION - 1) * PADDED_TILE_SDF_RESOLUTION + cell_x] = top_sdf[cell_x];
                }
                FORI_NAMED(cell_y, 0, PADDED_TILE_SDF_RESOLUTION) {
                    sdf[cell_y * PADDED_TILE_SDF_RESOLUTION + (PADDED_TILE_SDF_RESOLUTION - 1)] = right_sdf[cell_y * PADDED_TILE_SDF_RESOLUTION];
                }
            }
        }
    }
    
    {
        v2f64 tree_global_ps[] = {
            V2F64(-1.0),
            V2F64(-1.3, 1.0),
            V2F64(3.0, -0.5),
        };
        
        v2f64 item_global_ps[] = {
            V2F64(0.0, 1.0),
        };
        Item_Contents item_contents[] = {
            make_weapon_item(Item_Type::WEAPON, Weapon_Type::THRUST),
        };
        
        v2f64 turret_global_ps[] = {
            V2F64(0.0, 2.0),
        };
        
        FORI(0, ARRAY_LENGTH(tree_global_ps)) {
            place_tree(storage, tree_global_ps[i] + V2F64(g->map_dim) * 0.5);
        }
        
        FORI(0, ARRAY_LENGTH(item_global_ps)) {
            place_item(storage, item_global_ps[i] + V2F64(g->map_dim) * 0.5, item_contents[i]);
        }
        
        FORI(0, ARRAY_LENGTH(turret_global_ps)) {
            place_turret(storage, turret_global_ps[i] + V2F64(g->map_dim) * 0.5);
        }
    }
    
    // Loading the tiles into the working set.
    FORI_TYPED_NAMED(s32, sdf_y, 0, WORKING_SET_GRID_HEIGHT) {
        FORI_TYPED_NAMED(s32, sdf_x, 0, WORKING_SET_GRID_WIDTH) {
            s32 global_tile_x = g->working_set_tile_p.x + sdf_x - 1;
            s32 global_tile_y = g->working_set_tile_p.y + sdf_y - 1;
            
            ASSERT(global_tile_x >= 0 && global_tile_x < storage->dim.x);
            ASSERT(global_tile_y >= 0 && global_tile_y < storage->dim.y);
            
            g->sdfs_by_tile[sdf_y * WORKING_SET_GRID_WIDTH + sdf_x] = get_stored_tile(&g->storage, V2S(global_tile_x, global_tile_y))->sdf;
        }
    }
}

#define ADD_ENTITY_BEGIN(type) s32 add = g->##type##.count++; \
ASSERT(g->##type##.count < MAX_LIVE_ENTITY_COUNT); \
Entity_Reference reference = make_entity_reference(Entity_Type::##type, add);

static Entity_Reference add_bullet(Game *g, v2 p, v2 direction, f32 speed) {
    ADD_ENTITY_BEGIN(BULLET);
    rect2 aabb = rect2_phalfdim(p, BULLET_RADIUS);
    
    g->BULLET.ps                [add] = p;
    g->BULLET.directions        [add] = direction;
    g->BULLET.speeds            [add] = speed;
    g->BULLET.hit_level_geometry[add] = false;
    g->BULLET.collision_handles [add] = add_collision(&g->collision);
    
    return reference;
}

static Entity_Reference add_turret(Game *g, v2 p) {
    ADD_ENTITY_BEGIN(TURRET);
    rect2 aabb = rect2_phalfdim(p, TURRET_RADIUS);
    
    g->TURRET.ps[add] = p;
    g->TURRET.cooldowns[add] = 1.0f;
    g->TURRET.collision_handles[add] = add_collision(&g->collision);
    
    return reference;
}

static Entity_Reference add_tree(Game *g, v2 p) {
    ADD_ENTITY_BEGIN(TREE);
    rect2 aabb = rect2_phalfdim(p, TREE_RADIUS);
    
    g->TREE.ps[add] = p;
    g->TREE.collision_handles[add] = add_collision(&g->collision);
    
    return reference;
}

static Entity_Reference add_item(Game *g, v2 p, Item_Contents contents) {
    ADD_ENTITY_BEGIN(ITEM);
    rect2 aabb = rect2_phalfdim(p, ITEM_RADIUS);
    
    g->ITEM.ps[add] = p;
    g->ITEM.contents[add] = contents;
    g->ITEM.collision_handles[add] = add_collision(&g->collision);
    
    return reference;
}
#undef GET_INSERT_INDEX

// @Flags
static inline bool is_persistent_type(u8 type) {
    using namespace Entity_Type;
    bool result = (type == TURRET) || (type == TREE) || (type == ITEM);
    return result;
}

void Implicit_Context::g_full_update(Game *g, Input *_in, const f64 dt, Audio_Info *audio, Datapack_Handle *datapack) {
    TIME_BLOCK;
    const f32 dt32 = (f32)dt;
    Input in = *_in;
    const bool non_visual_update = (audio || datapack);
    
#if GENESIS_DEV
    if(g->godmode) {
        g->lost = false;
    }
#endif
    
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
    
    if(g->PLAYER.count) { // Player controls.
        {
            f32 camera_zoom = g->camera_zoom;
            if(BUTTON_DOWN(in, slot1)) {
                camera_zoom = 1.0f;
            } else if(BUTTON_DOWN(in, slot2)) {
                camera_zoom = 0.5f;
            } else if(BUTTON_DOWN(in, slot3)) {
                camera_zoom = 0.25f;
            } else if(BUTTON_DOWN(in, slot4)) {
                camera_zoom = 2.0f;
            }
            
            g->camera_zoom = camera_zoom;
            g->xhair_offset = unit_scale_to_world_space_offset(in.xhairp, camera_zoom);
            
            v2_normalize_if_nonzero(g->xhair_offset, &g->PLAYER.rots[0]);
        }
        
        const v2 xhair_offset = g->xhair_offset;
        const s32 count = g->PLAYER.count;
        for(s32 iplayer = 0; iplayer < count; ++iplayer) {
            v2 p = g->PLAYER.ps[iplayer];
            v2 rot = g->PLAYER.rots[iplayer];
            
            { // Movement.
                v2 dp = g->PLAYER.dps[iplayer];
                
                update_ballistic_dp(&dp.x, in, BUTTON_FLAG(left), BUTTON_FLAG(right), dt32,
                                    80.0f, 0.5f, 10.0f);
                update_ballistic_dp(&dp.y, in, BUTTON_FLAG(down), BUTTON_FLAG(up)   , dt32,
                                    80.0f, 0.5f, 10.0f);
                
                g->PLAYER.dps[iplayer] = dp;
            }
            
            { // Action.
                Player_Animation_State *anim_state = &g->PLAYER.animation_states[iplayer];
                Weapon *weapon = &g->PLAYER.weapons[iplayer];
                
                anim_state->time_since_last_attack += dt32;
                
                if(!anim_state->animating || anim_state->cancellable) {
                    if(BUTTON_PRESSED(&in, attack)) {
                        f32 attack_dt = dt32;
                        
                        if(weapon->type == Weapon_Type::SLASH) {
                            // Shortsword
                            f32 old_rot0_y = g->PLAYER.weapons[iplayer].melee.slash.rot0.y;
                            f32 old_rot1_y = g->PLAYER.weapons[iplayer].melee.slash.rot1.y;
                            
                            v2 rot0 = v2_angle_to_complex(-PI * 0.25f);
                            v2 rot1 = v2_angle_to_complex( PI * 0.25f);
                            
                            if((anim_state->time_since_last_attack < 0.5f) && (old_rot0_y < old_rot1_y)) {
                                SWAP(rot0, rot1);
                            }
                            attack_dt = 6.0f * dt32;
                            
                            weapon->type = Weapon_Type::SLASH;
                            weapon->melee.slash.rot0 = rot0;
                            weapon->melee.slash.rot1 = rot1;
                        } else if(weapon->type == Weapon_Type::THRUST) {
                            // Rapier
                            v2 translate0 = V2(0.0f, 0.0f);
                            v2 translate1 = V2(1.0f, 0.0f);
                            
                            if(anim_state->time_since_last_attack < 0.5f) {
                                translate0 = V2(0.5f, 0.0f);
                            }
                            attack_dt = 5.0f * dt32;
                            
                            weapon->type = Weapon_Type::THRUST;
                            weapon->melee.thrust.translate0 = translate0;
                            weapon->melee.thrust.translate1 = translate1;
                        } else UNHANDLED;
                        
                        anim_state->t  = 0.0f;
                        anim_state->dt = attack_dt;
                        anim_state->animating = true;
                        anim_state->cancellable = false;
                    }
                }
            }
            
            { // Hitcap update.
                auto *anim_state = &g->PLAYER.animation_states[iplayer];
                
                if(anim_state->animating) {
                    Weapon *weapon = &g->PLAYER.weapons[iplayer];
                    
                    anim_state->t += anim_state->dt;
                    
                    if((weapon->type == Weapon_Type::SLASH) ||
                       (weapon->type == Weapon_Type::THRUST)) {
                        cap2 hitcap;
                        
                        v2 rot0 = V2(1.0f, 0.0f);
                        v2 rot1 = V2(1.0f, 0.0f);
                        
                        v2 translate0 = V2();
                        v2 translate1 = V2();
                        
                        u16 weapon_handle = g->PLAYER.weapon_collision_handles[iplayer];
                        
                        if(f_in_range(anim_state->t, 0.0f, 1.0f)) {
                            f32 t = anim_state->t;
                            f32 ease_t = t;
                            
                            // Consider computing weapon momentum to influence damage.
                            
                            switch(weapon->type) {
                                case Weapon_Type::SLASH: {
                                    ease_t = f_sin(t*t*t*t * PI * 0.5f);
                                    
                                    rot0 = weapon->melee.slash.rot0;
                                    rot1 = weapon->melee.slash.rot1;
                                } break;
                                
                                case Weapon_Type::THRUST: {
                                    ease_t = f_sin(t*t*t * PI * 0.5f);
                                    
                                    translate0 = weapon->melee.thrust.translate0;
                                    translate1 = weapon->melee.thrust.translate1;
                                } break;
                            }
                            
                            v2 angle = v2_lerp_n(rot0, rot1, ease_t);
                            v2 hitcap_rot = v2_complex_prod(rot, angle);
                            v2 hitcap_translate = v2_lerp(translate0, translate1, ease_t);
                            hitcap_translate = v2_complex_prod(hitcap_translate, hitcap_rot);
                            hitcap_translate += p;
                            
                            hitcap.a = hitcap_rot * PLAYER_HITCAP_A_OFFSET + hitcap_translate;
                            hitcap.b = hitcap_rot * PLAYER_HITCAP_B_OFFSET + hitcap_translate;
                            hitcap.radius = PLAYER_HITCAP_RADIUS;
                            
                            g->PLAYER.weapon_hitcaps[iplayer] = hitcap;
                            
                            if(!weapon_handle) {
                                weapon_handle = add_collision(&g->collision);
                            }
                        } else {
                            anim_state->animating = false;
                            anim_state->cancellable = false;
                            g->PLAYER.weapon_hitcaps[iplayer].radius = 0.0f;
                            
                            if(weapon_handle) {
                                remove_collision(&g->collision, weapon_handle);
                                weapon_handle = 0;
                            }
                        }
                        
                        g->PLAYER.weapon_collision_handles[iplayer] = weapon_handle;
                        
                        if(!anim_state->cancellable && (anim_state->t > 0.8f)) {
                            anim_state->time_since_last_attack = 0.0f;
                            anim_state->cancellable = true;
                        }
                    }
                }
            }
        }
    } else if(BUTTON_PRESSED(in, attack)) {
        // Reset the level.
        load_dumb_level(g);
    }
    
    { // Turret control.
        const s32 count = g->TURRET.count;
        FORI(0, count) {
            f32 cooldown = g->TURRET.cooldowns[i];
            const v2 p = g->TURRET.ps[i];
            
            if(cooldown <= 0.0f) {
                const f32 spawn_dist = (BULLET_RADIUS + TURRET_RADIUS + PROJECTILE_SPAWN_DIST_EPSILON);
                
                s32 targets_in_range = g->PLAYER.count;
                if(targets_in_range > 0) {
                    V2_Closest_Result result;
                    v2_closest(p, g->PLAYER.ps, g->PLAYER.count, &result);
                    
                    if(result.dist_sq) {
                        v2 direction = (result.closest - p) * f_inv_sqrt(result.dist_sq);
                        v2 spawn_p = p + direction * spawn_dist;
                        
                        add_bullet(g, spawn_p, direction, 2.0f);
                        cooldown = 1.0f;
                    }
                }
            }
            
            cooldown -= dt32;
            
            g->TURRET.cooldowns[i] = cooldown;
        }
    }
    
    { // Bullet control.
        s32 count = g->BULLET.count;
        
        FORI_TYPED(s32, 0, count) {
            const v2 p = g->BULLET.ps[i];
            v2 dir    = g->BULLET.directions [i];
            f32 speed = g->BULLET.speeds     [i];
            
            if((p.x < -MAP_TILE_HALFDIM) || (p.x >= MAP_TILE_HALFDIM) ||
               (p.y < -MAP_TILE_HALFDIM) || (p.y >= MAP_TILE_HALFDIM)) {
                mark_entity_for_removal(g, Entity_Type::BULLET, i);
            } else {
                g->BULLET.directions[i] = dir;
                g->BULLET.dps[i] = dir * speed;
            }
        }
        
        g->BULLET.count = count;
    }
    
    //
    // At this point, we should have emitted every move.
    //
    // Check for solid collisions.
    
    Collision_Batch collision_batches_by_type[] = {
        {
            g->PLAYER.ps,
            g->PLAYER.dps,
            g->PLAYER.collision_handles,
            PLAYER_RADIUS,
            &g->PLAYER.count,
            Entity_Type::PLAYER,
        },
        {
            g->TURRET.ps,
            0,
            g->TURRET.collision_handles,
            TURRET_RADIUS,
            &g->TURRET.count,
            Entity_Type::TURRET,
        },
        {
            g->BULLET.ps,
            g->BULLET.dps,
            g->BULLET.collision_handles,
            BULLET_RADIUS,
            &g->BULLET.count,
            Entity_Type::BULLET,
        },
        {
            g->TREE.ps,
            0,
            g->TREE.collision_handles,
            TREE_RADIUS,
            &g->TREE.count,
            Entity_Type::TREE,
        },
        {
            g->ITEM.ps,
            0,
            g->ITEM.collision_handles,
            ITEM_RADIUS,
            &g->ITEM.count,
            Entity_Type::ITEM,
        },
    };
    
    {
        Collision_Batch *movement_batches[] = {
            &collision_batches_by_type[Entity_Type::PLAYER],
            &collision_batches_by_type[Entity_Type::BULLET],
        };
        
        s32 batch_count = ARRAY_LENGTH(movement_batches);
        
        s32 cells_visited_capacity = 64;
        v2s *cells_visited = push_array(&temporary_memory, v2s, cells_visited_capacity);
        s32 cells_to_check_capacity = 64;
        v2s *cells_to_check = push_array(&temporary_memory, v2s, cells_to_check_capacity);
        
        FORI_NAMED(batch_index, 0, batch_count) {
            Collision_Batch *batch = movement_batches[batch_index];
            
            const f32 radius = batch->radius;
            const rect2 bounds = rect2_4f(-WORKING_SET_HALFDIM + radius, -WORKING_SET_HALFDIM + radius, WORKING_SET_HALFDIM - radius, WORKING_SET_HALFDIM - radius);
            ssize count = *(batch->count);
            
            FORI_NAMED(entity_index, 0, count) {
                v2 p = batch->ps[entity_index];
                v2 dp = batch->dps[entity_index] * dt32;
                
                f32 dp_length_sq = v2_length_sq(dp);
                v2 target = p + dp;
                
                p.x      = f_clamp(p.x,      bounds.left,   bounds.right);
                p.y      = f_clamp(p.y,      bounds.bottom, bounds.top);
                target.x = f_clamp(target.x, bounds.left,   bounds.right);
                target.y = f_clamp(target.y, bounds.bottom, bounds.top);
                
                f32 dist[4];
                f32 start_dist;
                v2 sdf_start_uv;
                v2s sdf_start_cell;
                v2 sdf_target_uv;
                v2s sdf_target_cell;
                u16 start_tile_index;
                {
                    working_set_to_sdf(p, &sdf_start_cell, &sdf_start_uv);
                    working_set_to_sdf(target, &sdf_target_cell, &sdf_target_uv);
                    
                    v2s tile_p = sdf_start_cell / TILE_SDF_RESOLUTION;
                    v2s local_cell = sdf_start_cell % TILE_SDF_RESOLUTION;
                    start_tile_index = CAST(u16, tile_p.y * WORKING_SET_GRID_WIDTH + tile_p.x);
                    
                    f32 *sdf = g->sdfs_by_tile[start_tile_index];
                    
                    sdf_sample4(sdf, local_cell.x, local_cell.y, radius, dist);
                    start_dist = f_sample_bilinear(dist[0], dist[1], dist[2], dist[3], sdf_start_uv);
                }
                
                if(0 && ((start_dist * start_dist) >= (dp_length_sq * 1.05f))) {
                    // We can skip the collision check.
                    batch->ps[entity_index] = target;
                } else {
                    // Go on to the test.
                    switch(batch->type) {
                        case Entity_Type::PLAYER: {
                            // Pathfind.
                            const f32 sdf_max_distance = GAME_TO_SDF_FACTOR * f_sqrt(dp_length_sq);
                            const f32 sdf_max_distance_sq = sdf_max_distance * sdf_max_distance;
                            const f32 sdf_walk_cell_radius = sdf_max_distance + 0.70710678f;
                            const f32 sdf_walk_cell_radius_sq = sdf_walk_cell_radius * sdf_walk_cell_radius;
                            
                            v2s best_sdf_cell = sdf_start_cell;
                            v2 best_sdf_uv = sdf_start_uv;
                            f32 best_sdf_dist_sq = sdf_max_distance_sq;
                            
                            s32 cells_visited_count = 0;
                            s32 cells_to_check_count = 0;
                            
                            v2u16 start_cell = V2U16(start_tile_index, CAST(u16, (sdf_start_cell.y << TILE_SDF_RESOLUTION_BITS) | sdf_start_cell.x));
                            cells_to_check[cells_to_check_count++] = sdf_start_cell;
                            
                            while(cells_to_check_count) {
                                v2s check_p = cells_to_check[--cells_to_check_count];
                                
                                f32 *sdf;
                                {
                                    v2s tile_p = check_p / TILE_SDF_RESOLUTION;
                                    v2s cell_p = check_p % TILE_SDF_RESOLUTION;
                                    
                                    sdf = g->sdfs_by_tile[tile_p.y * WORKING_SET_GRID_WIDTH + tile_p.x];
                                    sdf_sample4(sdf, cell_p.x, cell_p.y, radius, dist);
                                }
                                
                                // We're not taking the path we take to get to each cell into account.
                                // The result is that, when we're moving around an obstacle, we can go further than
                                // if we actually traced the path that took the long way around (in fact, we go
                                // exactly as far as if there was no obstacle, but this is different from tunneling,
                                // since we can't actually cross an obstacle if we aren't going fast enough to move
                                // around it).
                                // 
                                // However, this might not be undesirable at all; the alternative would be more complex and
                                // would also potentially mess with our trajectory in unpredictable ways.
                                v2 dp_from_start;
                                dp_from_start.x = CAST(f32, CAST(s32, check_p.x) - sdf_start_cell.x) + 0.5f - sdf_start_uv.x;
                                dp_from_start.y = CAST(f32, CAST(s32, check_p.y) - sdf_start_cell.y) + 0.5f - sdf_start_uv.y;
                                bool bottom_path = v2_length_sq(dp_from_start + V2(0.0f, -1.0f)) < sdf_walk_cell_radius_sq;
                                bool left_path   = v2_length_sq(dp_from_start + V2(-1.0f, 0.0f)) < sdf_walk_cell_radius_sq;
                                bool right_path  = v2_length_sq(dp_from_start + V2(1.0f, 0.0f)) < sdf_walk_cell_radius_sq;
                                bool top_path    = v2_length_sq(dp_from_start + V2(0.0f, 1.0f)) < sdf_walk_cell_radius_sq;
                                
                                v2 split_points[4];
                                s32 split_count = 0;
                                
                                if((dist[0] * dist[1]) < 0.0f) {
                                    // Split bottom
                                    f32 split_bottom = -dist[0] / (-dist[0] + dist[1]);
                                    v2 split_p = V2(-0.5f + split_bottom, -0.5f);
                                    
                                    split_points[split_count++] = split_p;
                                } else if(dist[0] < 0.0f) {
                                    bottom_path = false;
                                }
                                
                                if((dist[0] * dist[2]) < 0.0f) {
                                    // Split left
                                    f32 split_left = -dist[0] / (-dist[0] + dist[2]);
                                    v2 split_p = V2(-0.5f, -0.5f + split_left);
                                    
                                    split_points[split_count++] = split_p;
                                } else if(dist[0] < 0.0f) {
                                    left_path = false;
                                }
                                
                                if((dist[3] * dist[1]) < 0.0f) {
                                    // Split right
                                    f32 split_right = -dist[1] / (-dist[1] + dist[3]);
                                    v2 split_p = V2(0.5f, -0.5f + split_right);
                                    
                                    split_points[split_count++] = split_p;
                                } else if(dist[3] < 0.0f) {
                                    right_path = false;
                                }
                                
                                if((dist[2] * dist[3]) < 0.0f) {
                                    // Split top
                                    f32 split_top = -dist[2] / (-dist[2] + dist[3]);
                                    v2 split_p = V2(-0.5f + split_top, 0.5f);
                                    
                                    split_points[split_count++] = split_p;
                                } else if(dist[3] < 0.0f) {
                                    top_path = false;
                                }
                                
                                v2 cell_dp;
                                cell_dp.x = CAST(f32, sdf_target_cell.x - CAST(s32, check_p.x)) + sdf_target_uv.x - 0.5f;
                                cell_dp.y = CAST(f32, sdf_target_cell.y - CAST(s32, check_p.y)) + sdf_target_uv.y - 0.5f;
                                
                                v2 closest;
                                closest.x = f_clamp(cell_dp.x, -0.5f, 0.5f);
                                closest.y = f_clamp(cell_dp.y, -0.5f, 0.5f);
                                switch(split_count) {
                                    case 0: {
                                    } break;
                                    
                                    case 2: {
                                        // One split plane.
                                        v2 split_a = split_points[0];
                                        v2 split_b = split_points[1];
                                        
                                        v2 normal = v2_perp(split_a - split_b);
                                        
                                        v2 negative_vert;
                                        if(dist[0] < 0.0f) {
                                            negative_vert = V2(-0.5f);
                                        } else if(dist[1] < 0.0f) {
                                            negative_vert = V2(0.5f, -0.5f);
                                        } else if(dist[2] < 0.0f) {
                                            negative_vert = V2(-0.5f, 0.5f);
                                        } else {
                                            ASSERT(dist[3] < 0.0f);
                                            negative_vert = V2(0.5f, 0.5f);
                                        }
                                        
                                        if(v2_inner(normal, negative_vert - split_a) > 0.0f) {
                                            normal = -normal;
                                        }
                                        
                                        v2 response = split_a - closest;
                                        f32 dot = v2_inner(normal, response);
                                        if(dot > 0.0f) {
                                            closest = line_closest(split_a, split_b, closest);
                                        }
                                    } break;
                                    
                                    case 4: {
                                        // Two split planes.
                                        if(dist[0] < 0.0f) {
                                            // The two split planes are from the bottom left and the top right.
                                            {
                                                const v2 split_bottom = split_points[0];
                                                const v2 split_left = split_points[1];
                                                
                                                v2 normal = v2_perp(split_bottom - split_left);
                                                v2 response = split_bottom - closest;
                                                f32 dot = v2_inner(response, normal);
                                                
                                                if(dot > 0.0f) {
                                                    closest = line_closest(split_bottom, split_left, closest);
                                                }
                                            }
                                            
                                            {
                                                const v2 split_right = split_points[2];
                                                const v2 split_top = split_points[3];
                                                
                                                v2 normal = v2_perp(split_top - split_right);
                                                v2 response = split_top - closest;
                                                f32 dot = v2_inner(response, normal);
                                                
                                                if(dot > 0.0f) {
                                                    closest = line_closest(split_top, split_right, closest);
                                                }
                                            }
                                        }
                                    } break;
                                    
                                    default: UNHANDLED;
                                }
                                
                                f32 dist_sq = v2_length_sq(cell_dp - closest);
                                
                                if(dist_sq < best_sdf_dist_sq) {
                                    best_sdf_dist_sq = dist_sq;
                                    best_sdf_cell = check_p;
                                    best_sdf_uv = closest + V2(0.5f);
                                }
                                
                                v2s left_cell = V2S(check_p.x - 1, check_p.y);
                                v2s bottom_cell = V2S(check_p.x, check_p.y - 1);
                                v2s right_cell = V2S(check_p.x + 1, check_p.y);
                                v2s top_cell = V2S(check_p.x, check_p.y + 1);
                                
                                // @Speed: We're linearly searching through every visited cell.
                                FORI(0, cells_visited_count) {
                                    v2s visited = cells_visited[i];
                                    
                                    if(visited.all == bottom_cell.all) {
                                        bottom_path = false;
                                    } else if(visited.all == left_cell.all) {
                                        left_path = false;
                                    } else if(visited.all == right_cell.all) {
                                        right_path = false;
                                    } else if(visited.all == top_cell.all) {
                                        top_path = false;
                                    }
                                }
                                
                                if(bottom_path) {
                                    cells_to_check[cells_to_check_count++] = bottom_cell;
                                }
                                if(left_path) {
                                    cells_to_check[cells_to_check_count++] = left_cell;
                                }
                                if(right_path) {
                                    cells_to_check[cells_to_check_count++] = right_cell;
                                }
                                if(top_path) {
                                    cells_to_check[cells_to_check_count++] = top_cell;
                                }
                                
                                cells_visited[cells_visited_count++] = check_p;
                                
                                ASSERT(cells_to_check_count <= cells_to_check_capacity);
                                ASSERT(cells_visited_count <= cells_visited_capacity);
                            }
                            
                            v2 final_p = sdf_to_working_set(best_sdf_cell, best_sdf_uv);
                            
                            batch->ps[entity_index] = final_p;
                        } break;
                        
                        case Entity_Type::BULLET: {
                            // Raycast.
                            bool hit = false;
                            
                            f32 dist_remaining = f_sqrt(dp_length_sq);
                            v2 dp_n = dp / dist_remaining;
                            v2 at = p;
                            while(dist_remaining > 0.0f) {
                                v2s cell;
                                v2 uv;
                                working_set_to_sdf(at, &cell, &uv);
                                
                                v2s tile_p = cell / TILE_SDF_RESOLUTION;
                                v2s cell_p = cell % TILE_SDF_RESOLUTION;
                                
                                f32 *sdf = g->sdfs_by_tile[tile_p.y * WORKING_SET_GRID_WIDTH + tile_p.x];
                                
                                sdf_sample4(sdf, cell_p.x, cell_p.y, radius, dist);
                                
                                f32 sd = f_sample_bilinear(dist[0], dist[1], dist[2], dist[3], uv);
                                sd = f_min(sd, dist_remaining);
                                
                                
                                at += dp_n * sd;
                                dist_remaining -= sd;
                                
                                if(sd <= 0.0f) {
                                    hit = true;
                                    break;
                                }
                            }
                            
                            if(hit) {
                                // We just want to stop the entity for this phase of the frame, since entity logic
                                // is run after this; we could just log the fact that we hit level geometry here?
                                g->BULLET.hit_level_geometry[entity_index] = true;
                            }
                            
                            batch->ps[entity_index] = at;
                        } break;
                    }
                }
            }
        }
    }
    
    //
    // At this point, every solid has found its final position for the frame.
    //
    
    { // Player overlaps.
        const ssize count = g->PLAYER.count;
        
        FORI_NAMED(player_index, 0, count) {
            Entity_Reference reference = make_entity_reference(Entity_Type::PLAYER, CAST(s32, player_index));
            v2 p = g->PLAYER.ps[player_index];
            
            update_collision(&g->collision, g->PLAYER.collision_handles[player_index], reference, p, PLAYER_RADIUS);
            
            {
                cap2 hitcap = g->PLAYER.weapon_hitcaps[player_index];
                if(hitcap.radius) {
                    v2 bounding_p = (hitcap.a + hitcap.b) * 0.5f;
                    f32 bounding_radius = v2_length(hitcap.a - hitcap.b) * 0.5f + hitcap.radius;
                    
                    update_collision(&g->collision, g->PLAYER.weapon_collision_handles[player_index], reference, bounding_p, bounding_radius);
                }
            }
        }
    }
    
    { // Bullet overlaps.
        const s32 count = g->BULLET.count;
        
        FORI_NAMED(bullet_index, 0, count) {
            v2 p = g->BULLET.ps[bullet_index];
            bool hit_level_geometry = g->BULLET.hit_level_geometry[bullet_index];
            
            Entity_Reference reference = make_entity_reference(Entity_Type::BULLET, CAST(s32, bullet_index));
            update_collision(&g->collision, g->BULLET.collision_handles[bullet_index], reference, p, BULLET_RADIUS);
            
            if(hit_level_geometry) {
                mark_entity_for_removal(g, reference);
            }
        }
    }
    
    overlap_update(&g->collision);
    
    { // Overlap results.
        ssize item_selection = -1;
        f32 item_best_dist_sq = LARGE_FLOAT;
        
        FORI(0, g->collision.overlap_pair_count) {
            Entity_Reference a = g->collision.results[i][0];
            Entity_Reference b = g->collision.results[i][1];
            
            if(a.type > b.type) {
                SWAP(a, b);
            }
            
            switch(a.type) {
                case Entity_Type::PLAYER: {
                    u16 collision_handle = g->collision.overlap_pairs[i][0];
                    v2 player_p = g->PLAYER.ps[a.index];
                    
                    if(collision_handle == g->PLAYER.weapon_collision_handles[a.index]) {
                        cap2 hitcap = g->PLAYER.weapon_hitcaps[a.index];
                        
                        switch(b.type) {
                            case Entity_Type::TURRET: {
                                v2 turret_p = g->TURRET.ps[b.index];
                                
                                if(disk_capsule_overlap(turret_p, TURRET_RADIUS, hitcap.a, hitcap.b, hitcap.radius)) {
                                    mark_entity_for_removal(g, b);
                                }
                            } break;
                        }
                    } else {
                        switch(b.type) {
                            case Entity_Type::BULLET: {
                                v2 bullet_p = g->BULLET.ps[b.index];
                                
                                if(disk_disk_overlap(player_p, PLAYER_RADIUS, bullet_p, BULLET_RADIUS)) {
                                    mark_entity_for_removal(g, a);
                                }
                            } break;
                            
                            case Entity_Type::ITEM: {
                                v2 item_p = g->ITEM.ps[b.index];
                                
                                const f32 pickup_dist_sq = (PLAYER_RADIUS + ITEM_RADIUS) * (PLAYER_RADIUS + ITEM_RADIUS);
                                f32 dist_sq = v2_length_sq(item_p - player_p);
                                if((dist_sq < pickup_dist_sq) && (dist_sq < item_best_dist_sq)) {
                                    item_selection = b.index;
                                    item_best_dist_sq = dist_sq;
                                }
                            } break;
                        }
                    }
                }
            }
        }
        
        if((item_selection != -1) && BUTTON_PRESSED(in, use)) {
            Item_Contents *contents = &g->ITEM.contents[item_selection];
            
            if(contents->type == Item_Type::WEAPON) {
                SWAP(g->PLAYER.weapons[0].type, contents->weapon.type);
            }
        }
        
        g->PLAYER.item_selections[0] = CAST(s32, item_selection);
    }
    
    //
    // At this point, all collision results should be available.
    //
    
    //
    // Maybe updating working set position. (This is possibly where we'll fire off streaming requests later on?)
    //
    if(non_visual_update) {
        v2s working_set_tile_p = g->working_set_tile_p;
        v2 player_p = g->PLAYER.ps[g->current_player];
        
        v2s working_set_tile_dp = V2S();
        
#define NUDGE_COORD(coord) { \
            if(player_p.##coord < -MAP_TILE_HALFDIM) { \
                working_set_tile_dp.##coord -= 1; \
            } else if(player_p.##coord > MAP_TILE_HALFDIM) { \
                working_set_tile_dp.##coord += 1; \
            } \
        }
        
        NUDGE_COORD(x);
        NUDGE_COORD(y);
#undef NUDGE_COORD
        
        if(working_set_tile_dp.x || working_set_tile_dp.y) {
            v2s new_working_set_tile_p = working_set_tile_p + working_set_tile_dp;
            
            // If we're on the map boundary, skip.
            if((new_working_set_tile_p.x > 0) && (new_working_set_tile_p.x < g->storage.dim.x) &&
               (new_working_set_tile_p.y > 0) && (new_working_set_tile_p.y < g->storage.dim.y)) {
                { // Update the working tile set.
                    FORI_TYPED_NAMED(s32, y_offset, -1, 2) {
                        s32 grid_y = new_working_set_tile_p.y + y_offset;
                        s32 row_offset = grid_y * g->storage.dim.x;
                        
                        FORI_TYPED_NAMED(s32, x_offset, -1, 2) {
                            s32 grid_x = new_working_set_tile_p.x + x_offset;
                            Game_Tile *tile = get_stored_tile(&g->storage, V2S(CAST(s32, grid_x), CAST(s32, grid_y)));
                            
                            g->sdfs_by_tile[(y_offset + 1) * WORKING_SET_GRID_WIDTH + (x_offset + 1)] = tile->sdf;
                            v2 tile_offset = V2(grid_x - working_set_tile_p.x, grid_y - working_set_tile_p.y) * MAP_TILE_DIM;
                            
                            Entity_Block *block = &tile->entities;
                            while(block) {
                                ssize offset = 0;
                                FORI(0, block->header.count) {
                                    Serialized_Entity_Header *header = CAST(Serialized_Entity_Header *, block->data + offset);
                                    
                                    switch(header->type) {
                                        case Entity_Type::TURRET: {
                                            Serialized_Turret *turret = CAST(Serialized_Turret *, header);
                                            
                                            v2 p = tile_offset + turret->p;
                                            
                                            add_turret(g, p);
                                            offset += sizeof(Serialized_Turret);
                                        } break;
                                        
                                        case Entity_Type::TREE: {
                                            Serialized_Tree *tree = CAST(Serialized_Tree *, header);
                                            
                                            v2 p = tile_offset + tree->p;
                                            
                                            add_tree(g, p);
                                            offset += sizeof(Serialized_Tree);
                                        } break;
                                        
                                        case Entity_Type::ITEM: {
                                            Serialized_Item *item = CAST(Serialized_Item *, header);
                                            
                                            v2 p = tile_offset + item->p;
                                            
                                            add_item(g, p, item->contents);
                                            offset += sizeof(Serialized_Item);
                                        } break;
                                        
                                        default: UNHANDLED;
                                    }
                                }
                                
                                block->header.count = 0;
                                block->header.allocated = 0;
                                
                                Entity_Block *empty_block = block;
                                
                                block = block->header.next;
                                
                                if(empty_block != &tile->entities) {
                                    // Recycle it.
                                    empty_block->header.next = g->storage.free_entity_blocks;
                                    g->storage.free_entity_blocks = empty_block;
                                } else {
                                    empty_block->header.next = 0;
                                }
                            }
                        }
                    }
                    
                    g->working_set_tile_p = new_working_set_tile_p;
                }
                
                { // Renormalize working set coordinates.
                    v2 dp = -V2(working_set_tile_dp) * MAP_TILE_DIM;
                    
                    rect2 working_set_bounds = rect2_minmax(V2(-WORKING_SET_HALFDIM), V2(WORKING_SET_HALFDIM));
                    FORI_NAMED(batch_index, 0, ARRAY_LENGTH(collision_batches_by_type)) {
                        auto batch = &collision_batches_by_type[batch_index];
                        ssize count = *(batch->count);
                        
                        FORI_NAMED(p_index, 0, count) {
                            v2 p = batch->ps[p_index];
                            
                            v2 displaced_p = p + dp;
                            if(!v2_in_rect2(displaced_p, working_set_bounds)) {
                                ASSERT(batch->type != Entity_Type::PLAYER);
                                // Unload it.
                                
                                if(is_persistent_type(batch->type)) {
                                    // Put it back in a tile.
                                    
                                    // @Cleanup: Use Precise_Position here instead?
                                    v2f64 global = V2F64(displaced_p) + V2F64(g->working_set_tile_p) * MAP_TILE_DIM;
                                    
                                    switch(batch->type) {
                                        case Entity_Type::TURRET: {
                                            place_turret(&g->storage, global);
                                        } break;
                                        
                                        case Entity_Type::TREE: {
                                            place_tree(&g->storage, global);
                                        } break;
                                        
                                        case Entity_Type::ITEM: {
                                            place_item(&g->storage, global, g->ITEM.contents[p_index]);
                                        } break;
                                        
                                        default: UNHANDLED;
                                    }
                                }
                                
                                // Either way, remove it from the working set.
                                mark_entity_for_removal(g, batch->type, CAST(s32, p_index));
                            } else {
                                if(batch->collision_handles) {
                                    update_collision(&g->collision, batch->collision_handles[p_index], make_entity_reference(batch->type, CAST(s32, p_index)), displaced_p, batch->radius);
                                }
                                
                                batch->ps[p_index] = displaced_p;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if(non_visual_update) { // Processing entity removals.
        // We're assuming that we don't have duplicate deaths.
        FORI(0, g->entity_removal_count) {
            Entity_Reference removal = g->entity_removals[i];
            s32 index = removal.index;
            
            Collision_Batch *batch = &collision_batches_by_type[removal.type];
            if(batch->collision_handles) {
                remove_collision(&g->collision, batch->collision_handles[removal.index]);
                // This is assuming we're doing [remove] = [--count] right after.
                batch->collision_handles[removal.index] = batch->collision_handles[*(batch->count) - 1];
            }
            
            switch(removal.type) {
                case Entity_Type::PLAYER: {
                    g->lost = true;
                } break;
                
                case Entity_Type::BULLET: {
                    s32 replace = --g->BULLET.count;
                    ASSERT(replace >= 0);
                    
                    g->BULLET.ps                [index] = g->BULLET.ps                [replace];
                    g->BULLET.directions        [index] = g->BULLET.directions        [replace];
                    g->BULLET.speeds            [index] = g->BULLET.speeds            [replace];
                    g->BULLET.hit_level_geometry[index] = false;
                } break;
                
                case Entity_Type::TURRET: {
                    s32 replace = --g->TURRET.count;
                    ASSERT(replace >= 0);
                    
                    g->TURRET.ps       [index] = g->TURRET.ps       [replace];
                    g->TURRET.cooldowns[index] = g->TURRET.cooldowns[replace];
                } break;
                
                case Entity_Type::TREE: {
                    s32 replace = --g->TREE.count;
                    ASSERT(replace >= 0);
                    
                    g->TREE.ps[index] = g->TREE.ps[replace];
                } break;
                
                case Entity_Type::ITEM: {
                    s32 replace = --g->ITEM.count;
                    ASSERT(replace >= 0);
                    
                    g->ITEM.ps[index] = g->ITEM.ps[replace];
                    g->ITEM.contents[index] = g->ITEM.contents[replace];
                } break;
                
                default: UNHANDLED;
            }
        }
        
        g->entity_removal_count = 0;
    }
    
    //
    // Frame end
    //
    
    if(g->lost) {
        g->PLAYER.count = 0;
    }
    
    advance_input(_in);
}

void Implicit_Context::draw_game_single_threaded(Renderer *renderer, Render_Command_Queue *command_queue, Game *g, Asset_Storage *assets) {
    TIME_BLOCK;
    if(g->current_level_index) {
        const u32 focused_player = g->current_player;
        const v2 camera_p = g->PLAYER.ps[focused_player];
        const v2 camera_dir = V2(1.0f, 0.0f);
        const f32 camera_zoom = g->camera_zoom;
        const v2 viewport_halfdim = V2(GAME_ASPECT_RATIO_X, GAME_ASPECT_RATIO_Y) * camera_zoom * 0.5f;
        
        render_set_transform_game_camera(command_queue->command_list_handle, camera_p, camera_dir, camera_zoom);
        
        render_set_texture(command_queue, 0);
        
        { // Players
            const u32 count = g->PLAYER.count;
            const u16 handle = GET_MESH_HANDLE(renderer, player);
            
            const v2 view_dir = v2_normalize(g->xhair_offset);
            const v2 rotation = -v2_perp(view_dir);
            
            {
                Mesh_Instance instance = {};
                instance.color = V4(1.0f);
                instance.scale = V2(PLAYER_RADIUS * 2.0f);
                instance.rot = rotation;
                
                for(u32 i = 0; i < count; ++i) {
                    instance.offset = g->PLAYER.ps[i];
                    
                    render_quad(renderer, command_queue, &instance);
                }
            }
            
            {
                Mesh_Instance hitcap = {};
                hitcap.color = V4(1.0f, 0.0f, 0.0f, 1.0f);
                hitcap.scale = V2(PLAYER_HITCAP_RANGE - PLAYER_HITCAP_RADIUS - PROJECTILE_SPAWN_DIST_EPSILON - PLAYER_RADIUS, PLAYER_HITCAP_RADIUS * 2.0f);
                FORI(0, count) {
                    if(g->PLAYER.animation_states[i].animating) {
                        cap2 cap = g->PLAYER.weapon_hitcaps[i];
                        
                        hitcap.offset = (cap.a + cap.b) * 0.5f;
                        hitcap.rot = v2_normalize(cap.b - cap.a);
                        
                        render_quad(renderer, command_queue, &hitcap);
                    }
                }
            }
        }
        
        render_set_texture(command_queue, 1);
        
        { // Trees
            const s32 count = g->TREE.count;
            Mesh_Instance *instances = reserve_instances(command_queue, GET_MESH_HANDLE(renderer, quad), 2 * count);
            
            Mesh_Instance trunk;
            trunk.color = V4(0.2f, 0.1f, 0.0f, 1.0f);
            trunk.rot = V2(1.0f, 0.0f);
            trunk.scale = V2(TREE_RADIUS * 2.0f);
            
            Mesh_Instance leaves;
            leaves.color = V4(0.2f, 0.8f, 0.0f, 1.0f);
            leaves.rot = V2(1.0f, 0.0f);
            leaves.scale = V2(1.0f, 1.0f);
            
            FORI(0, count) {
                trunk.offset = g->TREE.ps[i];
                trunk.offset.y += trunk.scale.y * 0.5f;
                leaves.offset = trunk.offset + V2(0.0f, 0.7f);
                
                instances[i << 1] = trunk;
                instances[(i << 1) + 1] = leaves;
            }
        }
        
        { // Turrets
            const s32 count = g->TURRET.count;
            
            Mesh_Instance instance;
            instance.color = V4(0.3f, 0.1f, 0.1f, 1.0f);
            instance.rot = V2(1.0f, 0.0f);
            instance.scale = V2(TURRET_RADIUS * 2.0f);
            
            FORI(0, count) {
                instance.offset = g->TURRET.ps[i];
                
                render_quad(renderer, command_queue, &instance);
            }
        }
        
        { // Bullets
            const s32 count = g->BULLET.count;
            
            Mesh_Instance instance;
            instance.color = V4(1.0f, 0.0f, 0.0f, 1.0f);
            instance.rot = V2(1.0f, 0.0f);
            instance.scale = V2(BULLET_RADIUS * 2.0f);
            
            FORI(0, count) {
                instance.offset = g->BULLET.ps[i];
                
                render_quad(renderer, command_queue, &instance);
            }
        }
        
        { // Items
            const s32 count = g->ITEM.count;
            
            Mesh_Instance instance;
            instance.rot = V2(0.707f, 0.707f);
            instance.scale = V2(ITEM_RADIUS);
            
            FORI(0, count) {
                instance.offset = g->ITEM.ps[i];
                
                instance.color = V4(0.0f, 0.8f, 0.2f, 1.0f);
                if(g->PLAYER.item_selections[0] == i) {
                    instance.color = V4(1.0f, 0.8f, 0.4f, 1.0f);
                }
                
                render_quad(renderer, command_queue, &instance);
            }
        }
        
#if 0
        { // SDF.
            Mesh_Instance instance = {};
            instance.scale = V2(0.1f);
            instance.rot = V2(1.0f, 0.0f);
            
            v2s cell_p = working_set_to_cell(g->PLAYER.ps[g->current_player]);
            v2s tile_p = cell_p / TILE_SDF_RESOLUTION;
            v2s local_cell = cell_p % TILE_SDF_RESOLUTION;
            
            f32 *sdf = g->sdfs_by_tile[tile_p.y * WORKING_SET_GRID_WIDTH + tile_p.x];
            FORI_TYPED_NAMED(s32, y, 0, TILE_SDF_RESOLUTION + 1) {
                FORI_TYPED_NAMED(s32, x, 0, TILE_SDF_RESOLUTION + 1) {
                    
                    if(s_max(s_abs(x - local_cell.x), s_abs(y - local_cell.y)) < 4) {
                        f32 distance = sdf[y * PADDED_TILE_SDF_RESOLUTION + x];
                        
                        instance.offset = -V2(WORKING_SET_HALFDIM) + V2(MAP_TILE_DIM * tile_p.x, MAP_TILE_DIM * tile_p.y) + V2(x, y) * SDF_TO_GAME_FACTOR;
                        instance.color = V4(f_clamp01(distance));
                        
                        render_quad(renderer, command_queue, &instance);
                    }
                }
            }
        }
#elif 0
        { // Tiles.
            Mesh_Instance instance = {};
            instance.scale = V2(MAP_TILE_DIM);
            instance.rot = V2(1.0f, 0.0f);
            instance.color = V4(1.0f, 1.0f, 0.0f, 1.0f);
            
            FORI_TYPED_NAMED(s32, tile_y, g->working_set_tile_p.y - 1, g->working_set_tile_p.y + 2) {
                FORI_TYPED_NAMED(s32, tile_x, g->working_set_tile_p.x - 1, g->working_set_tile_p.x + 2) {
                    Precise_Position precise;
                    precise.tile = V2S(tile_x, tile_y);
                    precise.local = V2(MAP_TILE_DIM * 0.5f);
                    
                    v2 tile_center = precise_to_working_set(precise, g->working_set_tile_p);
                    
                    instance.offset = tile_center;
                    
                    render_quad_outline(renderer, command_queue, &instance, 0.25f);
                }
            }
        }
#elif 0
        { // AABB tree.
            Mesh_Instance instance = {};
            instance.rot = V2(1.0f, 0.0f);
            v4 colors[] = {
                V4(1.0f, 0.0f, 0.0f, 1.0f),
                V4(0.0f, 1.0f, 0.0f, 1.0f),
                V4(0.0f, 0.0f, 1.0f, 1.0f),
                V4(1.0f, 1.0f, 0.0f, 1.0f),
                V4(1.0f, 0.0f, 1.0f, 1.0f),
                V4(0.0f, 1.0f, 1.0f, 1.0f),
                V4(1.0f, 1.0f, 1.0f, 1.0f),
            };
            
            Game_Collision *collision = &g->collision;
            
            u16 to_draw[256];
            to_draw[0] = 0;
            s32 to_draw_count = 1;
            while(to_draw_count > 0) {
                u16 node_index = to_draw[--to_draw_count];
                
                Aabb_Node *node = &collision->nodes[node_index];
                
                instance.scale = rect2_dim(node->aabb);
                instance.offset = rect2_center(node->aabb);
                
                if(node->inner) {
                    instance.color = V4(0.5f, 0.5f, 0.5f, 1.0f);
                    
                    to_draw[to_draw_count++] = node->left;
                    to_draw[to_draw_count++] = node->right;
                } else {
                    instance.color = V4(1.0f, 1.0f, 0.0f, 1.0f);
                }
                
                render_quad_outline(renderer, command_queue, &instance, 0.05f);
            }
        }
#endif
        
        {
            Mesh_Instance xhair = {};
            xhair.offset = camera_p + g->xhair_offset;
            xhair.scale = V2(0.1f);
            xhair.rot = V2(0.707f, 0.707f);
            xhair.color = V4(1.0f);
            
            render_quad(renderer, command_queue, &xhair);
        }
    }
    
    maybe_flush_draw_commands(command_queue);
}

static inline void open_menu(Game_Client *g_cl) {
    os_platform.release_cursor();
    g_cl->in_menu = true;
}

static inline void close_menu(Game_Client *g_cl) {
    os_platform.capture_cursor();
    g_cl->in_menu = false;
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
    
    global_log = &info->log;
    
    Game_Client    * const        g_cl = &info->client;
    
    Renderer       * const    renderer = &info->renderer;
    Audio_Info     * const  audio_info = &info->audio;
    Text_Info      * const   text_info = &info->text;
    Synth          * const       synth = &info->synth;
    
    { // Game state
        Game_Map_Storage *storage = &g_cl->g.storage;
        
        s32 grid_width = f_ceil_s(TEST_MAP_WIDTH / MAP_TILE_DIM);
        s32 grid_height = f_ceil_s(TEST_MAP_HEIGHT / MAP_TILE_DIM);
        s32 tile_count = grid_width * grid_height;
        
        storage->dim = V2S(grid_width, grid_height);
        
        {
            ssize entity_block_count = IDIV_ROUND_UP(MAX_SERIALIZED_ENTITY_SIZE * tile_count * MAX_LIVE_ENTITY_COUNT, 9 * ENTITY_BLOCK_DATA_SIZE);
            Entity_Block *entity_blocks = push_array(game_block, Entity_Block, entity_block_count);
            
            Entity_Block *previous = &entity_blocks[0];
            previous->header.next = 0;
            previous->header.count = 0;
            FORI(1, entity_block_count) {
                Entity_Block *block = &entity_blocks[i];
                
                previous->header.next = block;
                block->header.next = 0;
                block->header.count = 0;
                
                previous = block;
            }
        }
        
        storage->tiles = push_array(game_block, Game_Tile, tile_count);
        
        FORI(0, tile_count) {
            Game_Tile *tile = &storage->tiles[i];
            
            tile->sdf = push_array(game_block, f32, PADDED_TILE_SDF_RESOLUTION * PADDED_TILE_SDF_RESOLUTION);
        }
        
        load_dumb_level(&g_cl->g);
    }
    
    { // Loading the config file.
        bool need_to_load_default_config = true;
        
        string config_file = os_platform.read_entire_file(STRING("main.config"));
        if(config_file.length) {
            // Load the file's settings.
            if(config_file.length == sizeof(User_Config)) {
                User_Config *config = (User_Config *)config_file.data;
                
                if(config->version == USER_CONFIG_VERSION) {
                    logprint(global_log, STRING("Correct version config found. Loading.\n"));
                    program_state->should_be_fullscreen = config->fullscreen;
                    program_state->window_size = V2S(config->window_dim);
                    program_state->input_settings = config->input;
                    audio_info->current_volume    = config->volume;
                    
                    need_to_load_default_config = false;
                } else {
                    string s = tprint("Outdated config version %d (expected %d). Ignoring.\n", config->version, USER_CONFIG_VERSION);
                    logprint(global_log, s);
                }
            } else {
                logprint(global_log, "Config file was of incorrect length %u. Ignoring.\n", config_file.length);
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
                BIND_BUTTON(program_state->input_settings.bindings,     run,     LSHIFT);
                BIND_BUTTON(program_state->input_settings.bindings,  crouch,      LCTRL);
                BIND_BUTTON(program_state->input_settings.bindings,     use,          E);
                
                BIND_BUTTON(program_state->input_settings.bindings,  editor,         F1);
                BIND_BUTTON(program_state->input_settings.bindings, profiler,        F2);
                
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
    
    // audio_init(audio_info, game_block, program_state->audio_output_rate, program_state->audio_output_channels, core_count); @Cleanup
    
    init_synth(synth, game_block, CAST(u8, core_count), CAST(u8, program_state->audio_output_channels), program_state->audio_output_rate);
    
#if GENESIS_DEV
    { // Profiler:
        Profiler profiler = {};
        
        // profile_init(&profiler, game_block, core_count);
        
        info->profiler = profiler;
        
        global_profiler = &info->profiler;
    }
#endif
    
    { // GPU-sensitive systems.
        // This is starting a frame that we then are expected to end manually; we might want to make this more explicit?
        render_init(renderer, game_block, 1);
        
        mesh_init(game_block, &info->mesh_editor);
        
        Render_Command_Queue *command_queue = get_current_command_queue(renderer);
        
        { // Load meshes.
            u16 player_mesh_handle  = load_mesh(renderer, command_queue, READ_ENTIRE_ASSET(&temporary_memory, &g_cl->assets.datapack, player,  mesh));
            u16 partner_mesh_handle = load_mesh(renderer, command_queue, READ_ENTIRE_ASSET(&temporary_memory, &g_cl->assets.datapack, partner, mesh));
            u16 ball_mesh_handle    = load_mesh(renderer, command_queue, READ_ENTIRE_ASSET(&temporary_memory, &g_cl->assets.datapack, ball,    mesh));
            
            put_mesh(renderer, Mesh_Uid::player,  player_mesh_handle);
            put_mesh(renderer, Mesh_Uid::partner, partner_mesh_handle);
            put_mesh(renderer, Mesh_Uid::ball,    ball_mesh_handle);
        }
        
        { // Default edit mesh.
            Output_Mesh mesh = {};
            // @Hardcoded
#define MAX_EDITABLE_MESH_VERTEX_COUNT 256
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), MAX_EDITABLE_MESH_VERTEX_COUNT, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            
            
            mesh.handle = handle;
            mesh.vertex_mapped = vertex_mapped;
            mesh.index_mapped = index_mapped;
            mesh.default_offset = V2();
            mesh.default_scale = V2(1.0f);
            mesh.default_rot = V2(1.0f, 0.0f);
            
            add_mesh(&info->mesh_editor, &mesh, MAX_EDITABLE_MESH_VERTEX_COUNT, V4(0.3f, 0.07f, 0.0f, 1.0f));
        }
        
        maybe_flush_draw_commands(command_queue);
        
        { // @Temporary
            Loaded_BMP blank = load_bmp(STRING("textures/blank.bmp"));
            Loaded_BMP untextured = load_bmp(STRING("textures/untextured.bmp"));
            Loaded_BMP hitbox = load_bmp(STRING("textures/default_hitbox.bmp"));
            
            os_platform.upload_texture_to_gpu_rgba8(command_queue->command_list_handle, Texture_Id::BLANK, CAST(u8 *, blank.data), V2S(blank.width, blank.height));
            os_platform.upload_texture_to_gpu_rgba8(command_queue->command_list_handle, Texture_Id::UNTEXTURED, CAST(u8 *, untextured.data), V2S(untextured.width, untextured.height));
            os_platform.upload_texture_to_gpu_rgba8(command_queue->command_list_handle, Texture_Id::FONT_ATLAS, CAST(u8 *, hitbox.data), V2S(hitbox.width, hitbox.height));
        }
        
        os_platform.submit_commands(&command_queue->command_list_handle, 1, true);
        render_end_frame(renderer);
    }
    
#if 0
    { // Text info:
        init(&text_info->glyph_hash, game_block);
        rect_pack_init(text_info->glyph_atlas_lines.lefts, text_info->glyph_atlas_lines.ys,
                       &text_info->glyph_atlas_lines.count, GLYPH_ATLAS_SIZE);
        u32 *zeroes = push_array(game_block, u32, GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE);
        text_info->glyph_atlas = gl_add_texture_linear_rgba(GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, zeroes);
    }
#endif
    
#if USE_DATAPACK
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
    
    info->sim_time = program_state->time;
}

void Implicit_Context::clone_game_state(Game *source, Game *clone) {
    TIME_BLOCK;
    
    mem_copy(source, clone, sizeof(Game));
    
    // We may need to do more if we have dynamically allocated RW data, which is not the case right now;
    // map_sdf is the only dynamic allocation we have, and it's R data.
}

static void update_menu(Menu *menu, Input *in) {
    advance_input(in);
}

GAME_RUN_FRAME(Implicit_Context::g_run_frame) { 
    TIME_BLOCK;
    temporary_memory.used = 0;
    
    Game_Block_Info * const info        = (Game_Block_Info *)game_block->mem;
    Game_Client     * const g_cl        = &info->client;
    Renderer        * const renderer = &info->renderer;
    Audio_Info      * const  audio_info = &info->audio;
    Text_Info       * const   text_info = &info->text;
    Menu            * const        menu = &info->menu;
    
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
    
    static bool in_mesh_editor = false;
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
            close_menu(g_cl);
            // If we don't do this, we send the same input to both the game and the menu,
            // which results in double presses which can lead to the menu reopening on the same
            // frame.
            advance_input(&game_input);
        } else {
            open_menu(g_cl);
        }
    }
    
    { TIME_BLOCK; // Game update:
        if(g_cl->in_menu) {
            update_menu(menu, &menu_input);
            
            program_state->input = menu_input;
            this_frame_sim_time = sim_dt;
        } else if(in_mesh_editor) {
            this_frame_sim_time = sim_dt;
        } else {
            // If we're too much behind, just drop frames.
            if((sim_dt + PHYSICS_DT) >= (16.0 * PHYSICS_DT)) {
                this_frame_sim_time = sim_dt - PHYSICS_DT;
            }
            
            u32 frames = 0;
            while((this_frame_sim_time + PHYSICS_DT) <= sim_dt) {
                SCOPE_MEMORY(&temporary_memory);
                
                // Actual game simulation.
                g_full_update(&g_cl->g, &game_input, PHYSICS_DT, audio_info, &g_cl->assets.datapack);
                
                this_frame_sim_time += PHYSICS_DT;
                flush_log_to_standard_output(global_log);
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
    
    { // Rendering:
        render_begin_frame_and_clear(renderer, V4(0.03f, 0.07f, 0.015f, 1.0f));
        Render_Command_Queue *command_queue = get_current_command_queue(renderer);
        
        // @Cleanup: We might want to remove this, or make it change the vsync interval, or something.
        if(program_state->should_render || 1) {
            if(in_mesh_editor) {
                mesh_update(&info->mesh_editor, renderer, &menu_input, command_queue);
                
                program_state->input = menu_input;
            } else {
                
                draw_game_single_threaded(renderer, command_queue, &g_cl->visual_state, &g_cl->assets);
                
                render_set_transform_right_handed_unit_scale(command_queue->command_list_handle);
                
                // Drawing UI:
                if(g_cl->in_menu) {
                    f32 render_target_height = CAST(f32, program_state->draw_rect.top - program_state->draw_rect.bottom);
                    // @XX
                    // draw_menu(menu, g_cl, renderer, text_info, render_target_height);
                }
                
                if((global_log->heads_up_buffer.length > 0) && (global_log->heads_up_alpha > 0.0f)) {
                    // @XX
                    // text_add_string(text_info, renderer, global_log->heads_up_buffer, V2(0.5f, 0.8f), 0.15f, FONT_MONO, true);
                }
                
                if(text_info->strings.count) {
                    text_update(text_info, renderer);
                }
            }
        }
        
        maybe_flush_draw_commands(command_queue);
        
        os_platform.submit_commands(&command_queue->command_list_handle, 1, true);
        render_end_frame(renderer);
    }
    
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