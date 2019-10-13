
#define MAX_EDIT_MESH_COUNT 256
#define MAX_LAYERS_PER_MESH 8
#define MAX_VERTS_PER_LAYER 64
#define MAX_INDICES_PER_LAYER (((MAX_VERTS_PER_LAYER) - 2) * 3)

#define DRAG_SNAP_GRANULARITY 0.1f

struct Render_Vertex;

struct Output_Mesh {
    u16 handle;
    
    Render_Vertex *vertex_mapped; 
    u16 *index_mapped; 
    
    u16 vert_count;
    u16 index_count;
    
    v2 default_offset;
    v2 default_scale;
    v2 default_rot;
};

//
//
// We have two ways of managing mesh offset/rot/scale:
// - one is to have a default_X value that gets loaded with the mesh, and that becomes
// a per-instance multiply at submit time
// - another is to bake everything into the vert offsets themselves
// For editing purposes, the first option is probably more robust to floating-point BS?
// And also probably nicer to use as a whole, even if it's marginally slower.
//
//

struct Edit_Mesh_Layer {
    v2 *verts;
    u16 *indices;
    
    u16 vert_count;
    
    v4 color;
};

struct Edit_Mesh {
    u16 layer_count;
    u16 current_layer;
    Edit_Mesh_Layer *layers;
    
    v2 center_point;
    
    Output_Mesh output;
};

struct Serialized_Edit_Mesh_Layer {
    v4 color;
    u16 vert_count;
    // v2 verts[vert_count];
    // u16 indices[(vert_count - 2) * 3];
};

struct Edit_Mesh_Undo {
    u16 mesh_index;
    u16 layer_index;
    
    Serialized_Edit_Mesh_Layer layer;
};

#define MESHES_IN_PICKER 8
ENUM(MESH_EDITOR_UI_SELECTION) {
    NONE,
    
    VERTEX,
    EDGE,
    CENTER_POINT,
    
    COLOR_PICKER,
    HUE_PICKER,
    LAYER_PICKER,
    MESH_PICKER,
    
    COUNT,
};

struct Mesh_Interaction {
    s32 selection;
    
    union {
        struct {
            s32 index;
            v2 vert_drag_start;
            v2 cursor_drag_start;
        } vert;
        
        struct {
            s32 index;
            
            v2 vert_starts[2];
            v2 cursor_start;
            v2 left_adjacent_dir;
            v2 right_adjacent_dir;
            
            v2 last_drag;
            v2 last_drag_cursor_p;
            v2 last_rotation;
            f32 last_scale;
        } edge;
        
        struct {
            v4 layer_color_before;
        } color_picker;
    };
};

struct Mesh_Hover {
    s32 selection;
    
    s32 index;
};

#define UNDO_RING_BUFFER_MAX_ALLOCATION_COUNT 1024
#define UNDO_RING_BUFFER_ALLOCATION_MASK (UNDO_RING_BUFFER_MAX_ALLOCATION_COUNT - 1)
struct Undo_Ring_Buffer {
    Memory_Block block;
    
    usize allocation_offsets[1024]; // Sorted
    u16 wrap_length;
    u16 oldest_allocation;
    u16 first_free;
    u16 start_of_undo_chain;
    
    // @Compression
    bool all_undos_popped;
    bool all_redos_popped;
    bool just_saved_snapshot;
};

struct Mesh_Editor {
    Undo_Ring_Buffer undo_buffer;
    
    Mesh_Interaction interaction;
    
    // @Compression
    bool highlight_all_vertices;
    bool snap_to_grid;
    bool snap_to_edge;
    bool expanded_mesh_picker;
    
    v2 color_picker_uv;
    
    s32 mesh_count;
    Edit_Mesh meshes[MAX_EDIT_MESH_COUNT];
    
    u16 last_mesh; // @Cleanup :NeedToGenerateMultipleOutputMeshes We won't need this in the future.
    u16 current_mesh;
};
