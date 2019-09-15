
#define VERTS_PER_CIRCLE 32

enum Render_Entry_Type {
    RENDER_ENTRY_TYPE_NONE = 0,
    RENDER_ENTRY_TYPE_CLEAR,
    RENDER_ENTRY_TYPE_LINE_STRIP,
    RENDER_ENTRY_TYPE_QUADS,
    RENDER_ENTRY_TYPE_CIRCLES,
    RENDER_ENTRY_TYPE_TRANSFORM,
    RENDER_ENTRY_TYPE_SHADER,
    RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_BEGIN,
    RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_END,
    RENDER_ENTRY_TYPE_POLYGON,
    RENDER_ENTRY_TYPE_TRIANGLES,
    RENDER_ENTRY_TYPE_TRIANGLE_FAN,
    RENDER_ENTRY_TYPE_ATLAS_QUADS,
};

struct Bitmap {
    void *data;
    s32 width;
    s32 height;
    s32 stride;
};

struct Texture {
    u32 handle;
    f32 aspect_ratio;
    v2 offset; // 0 to 1
    
    // When we have a texture atlas:
    // v2 uv0, uv1;
    // 
    // The texture atlas will probably have a global scale.
    // We do want to add scaling to the rendering code, though.
};

struct Shader {
    u32 program;
    u32 p;
    u32 uv;
    u32 color;
    u32 transform;
    u32 texture_sampler;
};

struct Render_Target {
    u32 target_handle;
    u32 texture_handle;
    v2s dim;
};

struct Render_Entry_Header {
    u8 type;
};

// @Compression: We could use a u32 here.
struct Render_Entry_Clear {
    v4 color;
};
struct Render_Entry_Line_Strip {
    u32 count;
};
struct Render_Entry_Quads {
    u32 count;
};
struct Render_Entry_Circles {
    u32 count;
};
struct Render_Entry_Transform {
    u32 index;
};
struct Render_Entry_Shader {
    u32 index;
};
struct Render_Entry_Target {
    u32 index;
};
struct Render_Entry_Polygon {
    u32 pt_count;
};
struct Render_Entry_Triangles {
    u32 count;
};
struct Render_Entry_Triangle_Fan {
    u32 count;
};
// @Compression: We could put the atlas ID elsewhere.
struct Render_Entry_Atlas_Quads {
    u32 atlas;
    u32 count;
};

struct Render_Entry {
    Render_Entry_Header header;
    union {
        Render_Entry_Clear        clear;
        Render_Entry_Line_Strip   line_strip;
        Render_Entry_Quads        quads;
        Render_Entry_Circles      circles;
        Render_Entry_Transform    transform;
        Render_Entry_Shader       shader;
        Render_Entry_Target       target;
        Render_Entry_Polygon      poly;
        Render_Entry_Triangles    tris;
        Render_Entry_Triangle_Fan tri_fan;
        Render_Entry_Atlas_Quads  atlas_quads;
    };
};

#define MAX_VERT_COUNT 524288
#define MAX_QUAD_COUNT (MAX_VERT_COUNT / 6)
struct Render_Vertex {
    v2 p;
    v2 uv;
    // @Compression: This can be a u32.
    v4 color;
};

enum Transform_Index {
    TRANSFORM_INDEX_GAME,
    TRANSFORM_INDEX_RIGHT_HANDED_UNIT_SCALE,
    TRANSFORM_INDEX_COUNT,
};

enum Shader_Index {
    SHADER_INDEX_TEXTURE,
    SHADER_INDEX_COUNT,
};

struct Texture_ID {
    union {
        struct {
            u16 texture: 12;
            u16 atlas: 4;
        };
        u16 all;
    };
};

struct Texture_Framing {
    rect2 uvs;
    v2 offset;
    v2 halfdim;
};

// @#
#define MAX_ATLAS_COUNT 4
#define MAX_TEXTURE_COUNT 512
struct Rendering_Info {
    Memory_Block render_queue;
    rect2s draw_rect;
    v2 render_target_dim;
    f32 aspect_ratio;
    
    v2 one_pixel;
    
    Render_Vertex verts[MAX_VERT_COUNT];
    u32 quadTextures[MAX_QUAD_COUNT];
    u32 vert_count;
    u32 quad_count;
    
    Render_Target render_targets[2];
    u32 render_target_count;
    
    f32 transforms[3][16];
    
    u32 idGeneralVertexBuffer;
    
    Shader shaders[2];
    u32 last_shader_queued;
    
    // This is used for retained-mode/iterative primitive construction.
    bool next_entry_should_be_new;
    
    // Texture storage:
    u8 atlas_count;
    s16 texture_count;
    u32 atlas_handles[MAX_ATLAS_COUNT];
    Texture_Framing texture_framings[MAX_TEXTURE_COUNT];
    
    Texture_ID *textures_by_uid;
    
    Texture_ID blank_texture_id;
    Texture_ID untextured_id;
    Texture_ID circle_id;
};
inline v2 get_pixel_to_unit_scale(Rendering_Info *info) {
    v2 result = v2_inverse(info->render_target_dim);
    return result;
}
