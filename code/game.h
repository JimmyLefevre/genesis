
#define LEVEL_FORMAT_VERSION 1

// @Hardcoded
#define MAX_PLAYER_COUNT 4
#define MAX_LIVE_ENTITY_COUNT 256

#define PLAYER_RADIUS             0.20f
#define TREE_RADIUS               0.25f
#define TURRET_RADIUS             0.10f
#define BULLET_RADIUS             0.05f
#define ITEM_RADIUS               0.30f

#define PLAYER_HITCAP_RADIUS      0.15f
#define PLAYER_HITCAP_RANGE       1.00f

#define PLAYER_HITCAP_A_OFFSET (PLAYER_HITCAP_RADIUS + PLAYER_RADIUS + PROJECTILE_SPAWN_DIST_EPSILON)
#define PLAYER_HITCAP_B_OFFSET (PLAYER_HITCAP_RANGE - PLAYER_HITCAP_RADIUS)

#define PROJECTILE_SPAWN_DIST_EPSILON 0.1f

#define GAME_ASPECT_RATIO_X 16.0f
#define GAME_ASPECT_RATIO_Y 9.0f
#define GAME_ASPECT_RATIO (GAME_ASPECT_RATIO_X / GAME_ASPECT_RATIO_Y)
#define INV_GAME_ASPECT_RATIO (GAME_ASPECT_RATIO_Y / GAME_ASPECT_RATIO_X)
#define PHYSICS_HZ 128
#define PHYSICS_DT (1.0 / PHYSICS_HZ)

#define TEST_MAP_WIDTH 250.0f
#define TEST_MAP_HEIGHT 250.0f

#define MAP_TILE_DIM 24.0f
#define MAP_TILE_HALFDIM ((MAP_TILE_DIM) * 0.5f)
#define MAX_COLLISION_VOLUMES_PER_TILE 255

#define TILE_SDF_RESOLUTION_BITS 7
#define TILE_SDF_RESOLUTION (1 << TILE_SDF_RESOLUTION_BITS)
#define TILE_SDF_X_MASK ((TILE_SDF_RESOLUTION) - 1)
#define TILE_SDF_MAX_DISTANCE 5.0f
#define PADDED_TILE_SDF_RESOLUTION ((TILE_SDF_RESOLUTION) + 1)

#define WORKING_SET_GRID_WIDTH 3
#define WORKING_SET_GRID_HEIGHT 3
#define WORKING_SET_DIM (MAP_TILE_DIM * WORKING_SET_GRID_WIDTH)
#define WORKING_SET_HALFDIM (MAP_TILE_HALFDIM * WORKING_SET_GRID_WIDTH)

#define GAME_TO_SDF_FACTOR ((TILE_SDF_RESOLUTION) / MAP_TILE_DIM)
#define SDF_TO_GAME_FACTOR (1.0f / (GAME_TO_SDF_FACTOR))

#define MAX_SERIALIZED_ENTITY_SIZE (sizeof(Serialized_Item))

ENUM(Entity_Type) {
    PLAYER = 0,
    TURRET,
    BULLET,
    TREE,
    ITEM,
    
    COUNT,
};

ENUM(Weapon_Type) {
    NONE = 0,
    
    SLASH,
    THRUST,
    
    COUNT,
};

struct Collision_Batch {
    v2 *ps;
    v2 *dps;
    u16 *collision_handles;
    
    f32 radius;
    s32 *count;
    
    u8 type;
};

struct Entity_Reference {
    u8 type;
    s32 index;
};
static inline Entity_Reference make_entity_reference(u8 type, s32 index) {
    Entity_Reference result = {};
    
    result.type = type;
    result.index = index;
    
    return result;
}

struct Weapon {
    u8 type;
    
    union {
        struct {
            union {
                struct {
                    v2 rot0;
                    v2 rot1;
                } slash;
                
                struct {
                    v2 translate0;
                    v2 translate1;
                } thrust;
            };
        } melee;
        
        struct {
        } ranged;
    };
};

struct Player_Animation_State {
    f32   time_since_last_attack;
    s32   attack_index;
    
    f32   t;
    f32   dt;
    
    bool  animating;
    bool  cancellable;
};

ENUM(Item_Type) {
    NONE = 0,
    
    WEAPON,
    
    COUNT,
};
struct Item_Contents {
    u8 type;
    
    union {
        struct {
            u8 type;
        } weapon;
    };
};
static inline Item_Contents make_weapon_item(u8 type, u8 weapon_type) {
    Item_Contents result;
    
    result.type = type;
    result.weapon.type = weapon_type;
    
    return result;
}

struct Precise_Position {
    v2s tile;
    v2 local;
};

// Map stuff
struct Map_Header { // :SaveGames
    s32 version;
    u32 seed;
    
    v2s dim_in_tiles;
    
    Precise_Position player_spawn_p;
    
    // ssize tile_offsets[dim_in_tiles.x * dim_in_tiles.y];
};

struct Serialized_Entity_Header {
    u8 type;
};
struct Serialized_Turret {
    Serialized_Entity_Header header;
    
    v2 p;
};
struct Serialized_Tree {
    Serialized_Entity_Header header;
    
    v2 p;
};
struct Serialized_Item {
    Serialized_Entity_Header header;
    
    v2 p;
    Item_Contents contents;
};

struct Entity_Block;
struct Entity_Block_Header {
    Entity_Block *next;
    ssize allocated;
    s32 count;
};

#define ENTITY_BLOCK_DATA_SIZE 256
#pragma pack(push, 1)
struct Entity_Block {
    Entity_Block_Header header;
    
    u8 data[ENTITY_BLOCK_DATA_SIZE];
};
#pragma pack(pop)

struct Game_Tile {
    f32 *sdf;
    Entity_Block entities;
};

struct Game_Map_Storage {
    Entity_Block *free_entity_blocks;
    v2s dim;
    Game_Tile *tiles;
};
static inline Game_Tile *get_stored_tile(Game_Map_Storage *storage, v2s tile_p) {
    ASSERT(tile_p.y >= 0 && tile_p.y < storage->dim.y);
    ASSERT(tile_p.x >= 0 && tile_p.x < storage->dim.x);
    
    Game_Tile *result = &storage->tiles[tile_p.y * storage->dim.x + tile_p.x];
    
    return result;
}

struct Collision_Plane {
    f32 x;
    u16 handle;
    bool left; // We have 15 bits available for flags.
};

#define MAX_COLLISION_VOLUME_COUNT 256
struct Game_Collision {
    s32 removal_count;
    u16 removals[MAX_COLLISION_VOLUME_COUNT];
    
    s32 plane_count;
    Collision_Plane planes[MAX_COLLISION_VOLUME_COUNT * 2];
    
    Entity_Reference references[MAX_COLLISION_VOLUME_COUNT];
    u32 handle_minus_one_to_indices[MAX_COLLISION_VOLUME_COUNT];
    
    s32 overlap_pair_count;
    u16 overlap_pairs[MAX_COLLISION_VOLUME_COUNT * 4][2];
    
    Entity_Reference results[MAX_COLLISION_VOLUME_COUNT * 4][2];
};

struct Game {
    // Metagame:
    u32 current_level_index; // ;Serialised
    u32 desired_level_index;
    
    v2 xhair_offset; // ;Immediate
    v2 camera_p;     // ;Immediate
    f32 camera_zoom; // ;Immediate
    
    v2s working_set_tile_p;
    
    s32 current_player;
    
    struct {
        s32   count;                                    // ;Serialised
        
        v2    camera_ps        [     MAX_PLAYER_COUNT]; // ;Immediate
        
        v2    ps               [     MAX_PLAYER_COUNT]; // ;Serialised
        v2    dps              [     MAX_PLAYER_COUNT]; // ;Immediate
        v2    rots             [     MAX_PLAYER_COUNT];
        
        Player_Animation_State animation_states[MAX_PLAYER_COUNT];
        Weapon weapons         [     MAX_PLAYER_COUNT];
        
        cap2 weapon_hitcaps    [     MAX_PLAYER_COUNT];
        
        s32 item_selections    [     MAX_PLAYER_COUNT]; // ;Immediate
        
        u16 collision_handles  [     MAX_PLAYER_COUNT];
        u16 weapon_collision_handles[MAX_PLAYER_COUNT];
    } PLAYER;
    
    struct {
        s32 count;                                      // ;Serialised
        v2  ps                 [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
        f32 cooldowns          [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
        
        u16 collision_handles  [MAX_LIVE_ENTITY_COUNT];
    } TURRET;
    
    struct {
        s32  count;
        v2   ps                [MAX_LIVE_ENTITY_COUNT];
        v2   directions        [MAX_LIVE_ENTITY_COUNT];
        f32  speeds            [MAX_LIVE_ENTITY_COUNT];
        bool hit_level_geometry[MAX_LIVE_ENTITY_COUNT];
        
        v2   dps               [MAX_LIVE_ENTITY_COUNT]; // ;Immediate
        
        u16 collision_handles  [MAX_LIVE_ENTITY_COUNT];
    } BULLET;
    
    struct {
        s32 count;
        v2  ps                 [MAX_LIVE_ENTITY_COUNT];
        
        u16 collision_handles  [MAX_LIVE_ENTITY_COUNT];
    } TREE;
    
    struct {
        s32 count;
        v2 ps                  [MAX_LIVE_ENTITY_COUNT];
        Item_Contents contents [MAX_LIVE_ENTITY_COUNT];
        
        u16 collision_handles  [MAX_LIVE_ENTITY_COUNT];
    } ITEM;
    
    bool lost;
    
    Game_Collision collision;
    
    s32 entity_removal_count; // ;Immediate
    Entity_Reference entity_removals[MAX_LIVE_ENTITY_COUNT];
    
    // Map:
    v2 map_dim; // ;Serialised
    
    f32 *sdfs_by_tile[WORKING_SET_GRID_WIDTH * WORKING_SET_GRID_HEIGHT];
    
    Game_Map_Storage storage;
    
#if GENESIS_DEV
    // Developer:
    bool godmode;
#endif
};

struct Game_Client {
    Game g;
    Game visual_state;
    
    f32 time_scale;
    
    bool in_menu;
    
    Asset_Storage assets;
};

struct Menu {
};

struct Render_Vertex;
struct Game_Block_Info {
    Game_Client client;
    Renderer renderer;
    
    Menu menu;
    Synth synth;
    
    f64 sim_time;
    
    // @Cleanup: These two systems are obsolete.
    Audio_Info audio;
    Text_Info text;
    
    Log log;
    Mesh_Editor mesh_editor;
    
#if GENESIS_DEV
    Profiler profiler;
#endif
};

#define USER_CONFIG_VERSION 5

#pragma pack(push, 1)
struct User_Config { // ;Serialized
    s32 version;
    bool fullscreen; // @Compression
    v2s16 window_dim; // Only relevant if windowed.
    Input_Settings input;
    Audio_Volumes volume;
};
#pragma pack(pop)
