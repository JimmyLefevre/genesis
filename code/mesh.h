
#define MAX_EDIT_MESH_COUNT 256
#define MAX_LAYERS_PER_MESH 8
#define MAX_VERTS_PER_LAYER 64
#define MAX_INDICES_PER_LAYER (((MAX_VERTS_PER_LAYER) - 2) * 3)

struct Render_Vertex;

struct Output_Mesh {
    u16 handle;
    
    Render_Vertex *vertex_mapped; 
    u16 *index_mapped; 
    
    s32 vert_count;
    s32 index_count;
    
    v2 default_offset;
    v2 default_scale;
    v2 default_rot;
};

struct Edit_Mesh_Layer {
    v2 verts[MAX_VERTS_PER_LAYER];
    u16 indices[MAX_INDICES_PER_LAYER];
    
    s32 vert_count;
    
    v4 color;
};

struct Edit_Mesh {
    s32 layer_count;
    s32 current_layer;
    Edit_Mesh_Layer layers[MAX_LAYERS_PER_MESH];
    
    Output_Mesh output;
};

#define MESHES_IN_PICKER 8
ENUM(MESH_EDITOR_UI_SELECTION) {
    NONE,
    VERTEX,
    COLOR_PICKER,
    LAYER_PICKER,
    MESH_PICKER,
    
    COUNT,
};

struct Mesh_Editor {
    s32 selection;
    s32 selected_vert;
    v2 last_cursor_p; // ;Immediate
    bool highlight_all_vertices;
    
    s32 mesh_count;
    Edit_Mesh meshes[MAX_EDIT_MESH_COUNT];
    
    u16 current_mesh;
};
