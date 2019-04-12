
#define LEVEL_FORMAT_VERSION 1

// @Hardcoded
#define MAX_PLAYER_COUNT 4
#define MAX_PARTNERS_PER_PLAYER 4
#define MAX_PARTNER_COUNT (MAX_PLAYER_COUNT * MAX_PARTNERS_PER_PLAYER)
#define MAX_LIVE_ENTITY_COUNT 256

#define PLAYER_HALFWIDTH          0.25f
#define PLAYER_HALFHEIGHT         0.50f
#define PARTNER_HALFWIDTH         0.30f
#define PARTNER_HALFHEIGHT        0.30f
#define TREE_HALFWIDTH            0.20f
#define TREE_HALFHEIGHT           0.20f
#define TURRET_HALFWIDTH          0.10f
#define TURRET_HALFHEIGHT         0.10f
#define BULLET_RADIUS             0.05f
#define BULLET_EXPLOSION_RADIUS   0.50f
#define DOG_RADIUS                0.15f
#define WALL_TURRET_HALFWIDTH     0.10f
#define WALL_TURRET_HALFHEIGHT    0.10f
#define TARGET_RADIUS 0.15f

#define PROJECTILE_SPAWN_DIST_EPSILON 0.1f

#define GAME_ASPECT_RATIO_X 16.0f
#define GAME_ASPECT_RATIO_Y 9.0f
#define GAME_ASPECT_RATIO (GAME_ASPECT_RATIO_X / GAME_ASPECT_RATIO_Y)
#define INV_GAME_ASPECT_RATIO (GAME_ASPECT_RATIO_Y / GAME_ASPECT_RATIO_X)
#define PHYSICS_HZ 128
#define PHYSICS_DT (1.0 / PHYSICS_HZ)

#define TEST_MAP_WIDTH 50.0f
#define TEST_MAP_HEIGHT 50.0f

enum Entity_Type {
    ENTITY_TYPE_PLAYER = 0,
    ENTITY_TYPE_PARTNER,
    ENTITY_TYPE_TURRET,
    ENTITY_TYPE_BULLET,
    ENTITY_TYPE_DOG,
    ENTITY_TYPE_WALL_TURRET,
    ENTITY_TYPE_TREE,
    ENTITY_TYPE_TARGET,
    
    ENTITY_TYPE_COUNT,
};

struct Game {
    // Metagame:
    u32 current_level_index; // ;Serialised
    u32 desired_level_index;
    
    v2 xhair_offset; // ;Immediate
    v2 camera_p; // ;Immediate
    f32 camera_zoom; // ;Immediate
    
    s32 current_player;
    struct {
        s32   count; // ;Serialised
        v2    camera_ps        [     MAX_PLAYER_COUNT]; // ;Immediate
        v2    dps              [     MAX_PLAYER_COUNT];
        v2    ps               [     MAX_PLAYER_COUNT]; // ;Serialised
    } players;
    
    // Collapse this into a player field?
    struct {
        s32 count; // ;Serialised
        v2 offsets[MAX_PARTNER_COUNT]; // ;Serialised
        v2 ps[MAX_PARTNER_COUNT]; // ;Immediate
    } partners;
    
    struct {
        s32 count; // ;Serialised
        v2  ps                 [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
        f32 cooldowns          [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
    } turrets;
    
    struct {
        s32  count;
        v2   ps                [MAX_LIVE_ENTITY_COUNT];
        v2   directions        [MAX_LIVE_ENTITY_COUNT];
        f32  speeds            [MAX_LIVE_ENTITY_COUNT];
        bool explodes_on_impact[MAX_LIVE_ENTITY_COUNT];
    } bullets;
    
    struct {
        s32 count; // ;Serialised
        v2  ps                    [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
        v2  dps                   [MAX_LIVE_ENTITY_COUNT];
    } dogs;
    
    struct {
        s32 count; // ;Serialised
        v2  ps            [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
    } wall_turrets;
    
    struct {
        s32 count;
        v2  ps                   [MAX_LIVE_ENTITY_COUNT];
    } trees;
    
    struct {
        s32 count; // ;Serialised
        v2 ps                    [MAX_LIVE_ENTITY_COUNT]; // ;Serialised
    } targets;
    
    bool lost;
    
    struct {
        s32 count; // ;Immediate
        u8  types                [MAX_LIVE_ENTITY_COUNT];
        s32 indices              [MAX_LIVE_ENTITY_COUNT];
    } removals;
    
    struct {
        s32 count;
        u8  types        [MAX_LIVE_ENTITY_COUNT];
    } logged_removals;
    
    // Map:
    v2 map_halfdim; // ;Serialised
    
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

struct Game_Clock {
    f64 cur_time;
    f64 sim_time;
};

struct Game_Block_Info {
    Game_Client client;
    Rendering_Info render_info;
    Audio_Info audio;
    Text_Info text;
    Game_Clock clock;
    Log log;
    Menu menu;
    
#if GENESIS_DEV
    Profiler profiler;
#endif
};

// @Serialisation
#define USER_CONFIG_VERSION 1

#pragma pack(push, 1)
struct User_Config {
    s32 version;
    bool fullscreen; // @Compression
    v2s16 window_dim; // Only relevant if windowed.
    Input_Settings input;
    Audio_Volumes volume;
};
struct Level_Header {
    s32 version; // @Compression: Move the version to a level set.
    s32 index; // @Compression: These will be implicit within a level set.
    
    // Level data @Incomplete
    v2 map_halfdim;
    
    // u32 music_uid; TODO
    
    // maybe terrain-specific graphics?
    
    s32 player_count;
    s32 partner_count;
    s32 turret_count;
    s32 dog_count;
    s32 wall_turret_count;
    s32 target_count;
};
#pragma pack(pop)

struct Serialised_Level_Data {
    Level_Header *header;
    
    v2 *player_ps;
    v2 *partner_offsets;
    v2 *turret_ps;
    f32 *turret_cooldowns;
    v2 *dog_ps;
    v2 *wall_turret_ps;
    v2 *target_ps;
};
