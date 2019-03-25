
#if OS_WINDOWS
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif

typedef void GLvoid;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

#define GL_ZERO                           0
#define GL_FALSE                          0
#define GL_ONE                            1
#define GL_TRUE                           1
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_EXTENSIONS                     0x1F03
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_TEXTURE_2D                     0x0DE1
#define GL_BLEND                          0x0BE2
#define GL_DOUBLEBUFFER                   0x0C32
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_RGBA8                          0x8058
#define GL_R32F                           0x822E
#define GL_RGBA                           0x1908
#define GL_DONT_CARE                      0x1100
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_RED                            0x1903
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_FRAMEBUFFER_SRGB               0x8DB9
#define GL_SRGB8_ALPHA8                   0x8C43
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_VALIDATE_STATUS                0x8B83
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STREAM_DRAW                    0x88E0
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_COPY                    0x88E2
#define GL_STATIC_DRAW                    0x88E4
#define GL_STATIC_READ                    0x88E5
#define GL_STATIC_COPY                    0x88E6
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_DYNAMIC_COPY                   0x88EA
#define GL_R8                             0x8229
#define GL_R8UI                           0x8232
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE8                       0x84C8
#define GL_TEXTURE9                       0x84C9
#define GL_TEXTURE10                      0x84CA
#define GL_TEXTURE11                      0x84CB
#define GL_TEXTURE12                      0x84CC
#define GL_TEXTURE13                      0x84CD
#define GL_TEXTURE14                      0x84CE
#define GL_TEXTURE15                      0x84CF
#define GL_TEXTURE16                      0x84D0
#define GL_TEXTURE17                      0x84D1
#define GL_TEXTURE18                      0x84D2
#define GL_TEXTURE19                      0x84D3
#define GL_TEXTURE20                      0x84D4
#define GL_TEXTURE21                      0x84D5
#define GL_TEXTURE22                      0x84D6
#define GL_TEXTURE23                      0x84D7
#define GL_TEXTURE24                      0x84D8
#define GL_TEXTURE25                      0x84D9
#define GL_TEXTURE26                      0x84DA
#define GL_TEXTURE27                      0x84DB
#define GL_TEXTURE28                      0x84DC
#define GL_TEXTURE29                      0x84DD
#define GL_TEXTURE30                      0x84DE
#define GL_TEXTURE31                      0x84DF
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0

typedef void (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

typedef void glEnableVertexAttribArray_t(GLuint index);
typedef void glDisableVertexAttribArray_t(GLuint index);
typedef GLint glGetAttribLocation_t(GLuint program,const GLchar *name);
typedef GLint glVertexAttribPointer_t(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void glShaderSource_t(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void glCompileShader_t(GLuint shader);
typedef GLuint glCreateShader_t(GLenum shaderType);
typedef GLuint glCreateProgram_t();
typedef void glAttachShader_t(GLuint program, GLuint shader);
typedef void glDeleteShader_t(GLuint shader);
typedef void glLinkProgram_t(GLuint program);
typedef void glValidateProgram_t(GLuint program);
typedef void glGetProgramiv_t(GLuint program, GLenum pname, GLint *params);
typedef void glGetShaderInfoLog_t(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void glGetProgramInfoLog_t(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void glUseProgram_t(GLuint program);
typedef void glUniformMatrix2fv_t(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void glUniformMatrix4fv_t(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef GLuint glGetUniformLocation_t(GLuint program, const GLchar *name);
typedef void glUniform1i_t(GLint location, GLint v0);
typedef void glDebugMessageCallback_t(GLDEBUGPROC callback, void *userParam);
typedef void glDebugMessageControl_t(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void glGenVertexArrays_t(GLsizei n, GLuint *arrays);
typedef void glBindVertexArray_t(GLuint array);
typedef void glGenBuffers_t(GLsizei n, GLuint *buffers);
typedef void glBindBuffer_t(GLenum target, GLuint buffer);
typedef void glBufferData_t(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef void glActiveTexture_t(GLenum texture);
typedef void glGenFramebuffers_t(GLsizei n, GLuint *ids);
typedef void glBindFramebuffer_t(GLenum target, GLuint framebuffer);
typedef void glFramebufferTexture2D_t(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

typedef void glBindTexture_t(GLenum target, GLuint texture);
typedef void glBlendFunc_t(GLenum sfactor, GLenum dfactor);
typedef void glClear_t(GLbitfield mask);
typedef void glClearColor_t(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void glDrawArrays_t(GLenum mode, GLint first, GLsizei count);
typedef void glEnable_t(GLenum cap);
typedef void glGenTextures_t(GLsizei n, GLuint *textures);
typedef void glTexImage2D_t(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void glTexParameteri_t(GLenum target, GLenum pname, GLint param);
typedef void glViewport_t(GLint x, GLint y, GLsizei width, GLsizei height);
typedef const GLubyte *glGetString_t(GLenum name);
typedef void glBegin_t(GLenum mode);
typedef void glEnd_t();
typedef void glColor4f_t(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void glVertex2f_t(GLfloat x, GLfloat y);
typedef void glDisable_t(GLenum cap);
typedef void glTexSubImage2D_t(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);

GLOBAL_FUNCTION_POINTER(glEnableVertexAttribArray);
GLOBAL_FUNCTION_POINTER(glDisableVertexAttribArray);
GLOBAL_FUNCTION_POINTER(glGetAttribLocation);
GLOBAL_FUNCTION_POINTER(glVertexAttribPointer);
GLOBAL_FUNCTION_POINTER(glShaderSource);
GLOBAL_FUNCTION_POINTER(glCompileShader);
GLOBAL_FUNCTION_POINTER(glCreateShader);
GLOBAL_FUNCTION_POINTER(glCreateProgram);
GLOBAL_FUNCTION_POINTER(glDeleteShader);
GLOBAL_FUNCTION_POINTER(glAttachShader);
GLOBAL_FUNCTION_POINTER(glLinkProgram);
GLOBAL_FUNCTION_POINTER(glValidateProgram);
GLOBAL_FUNCTION_POINTER(glGetProgramiv);
GLOBAL_FUNCTION_POINTER(glGetShaderInfoLog);
GLOBAL_FUNCTION_POINTER(glGetProgramInfoLog);
GLOBAL_FUNCTION_POINTER(glUseProgram);
GLOBAL_FUNCTION_POINTER(glUniformMatrix2fv);
GLOBAL_FUNCTION_POINTER(glUniformMatrix4fv);
GLOBAL_FUNCTION_POINTER(glGetUniformLocation);
GLOBAL_FUNCTION_POINTER(glUniform1i);
GLOBAL_FUNCTION_POINTER(glDebugMessageCallback);
GLOBAL_FUNCTION_POINTER(glDebugMessageControl);
GLOBAL_FUNCTION_POINTER(glGenVertexArrays);
GLOBAL_FUNCTION_POINTER(glBindVertexArray);
GLOBAL_FUNCTION_POINTER(glGenBuffers);
GLOBAL_FUNCTION_POINTER(glBindBuffer);
GLOBAL_FUNCTION_POINTER(glBufferData);
GLOBAL_FUNCTION_POINTER(glActiveTexture);
GLOBAL_FUNCTION_POINTER(glBindTexture);
GLOBAL_FUNCTION_POINTER(glBlendFunc);
GLOBAL_FUNCTION_POINTER(glClear);
GLOBAL_FUNCTION_POINTER(glClearColor);
GLOBAL_FUNCTION_POINTER(glDrawArrays);
GLOBAL_FUNCTION_POINTER(glEnable);
GLOBAL_FUNCTION_POINTER(glGenTextures);
GLOBAL_FUNCTION_POINTER(glTexImage2D);
GLOBAL_FUNCTION_POINTER(glTexParameteri);
GLOBAL_FUNCTION_POINTER(glViewport);
GLOBAL_FUNCTION_POINTER(glGetString);
GLOBAL_FUNCTION_POINTER(glBegin);
GLOBAL_FUNCTION_POINTER(glEnd);
GLOBAL_FUNCTION_POINTER(glColor4f);
GLOBAL_FUNCTION_POINTER(glVertex2f);
GLOBAL_FUNCTION_POINTER(glDisable);
GLOBAL_FUNCTION_POINTER(glGenFramebuffers);
GLOBAL_FUNCTION_POINTER(glBindFramebuffer);
GLOBAL_FUNCTION_POINTER(glFramebufferTexture2D);
GLOBAL_FUNCTION_POINTER(glTexSubImage2D);

void APIENTRY GlDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
    UNHANDLED;
}

struct gl_extensions {
    bool vsync;
    bool srgb;
};

static GLuint GLCompileShader(const GLuint vertexShader, const GLchar **vertexShaderCode, const u32 vertexShaderPartCount,
                              const GLuint fragmentShader, const GLchar **fragmentShaderCode, const u32 fragmentShaderPartCount) {
    glShaderSource(vertexShader, vertexShaderPartCount, vertexShaderCode, 0);
    glShaderSource(fragmentShader, fragmentShaderPartCount, fragmentShaderCode, 0);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glValidateProgram(program);
    GLint validLink = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &validLink);
    if(!validLink) {
        GLchar vertLog[256] = {0};
        GLchar fragLog[256] = {0};
        GLchar progLog[256] = {0};
        GLsizei vertErrLen = 0;
        GLsizei fragErrLen = 0;
        GLsizei progErrLen = 0;
        glGetShaderInfoLog(vertexShader, 256, &vertErrLen, vertLog);
        glGetShaderInfoLog(fragmentShader, 256, &fragErrLen, fragLog);
        glGetProgramInfoLog(program, 256, &progErrLen, progLog);
        UNHANDLED;
    }
    return program;
}

static GLuint GLShader(const GLchar *vertexCode, const GLchar *fragmentCode) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar * const headerCode = "#version 130\n";
    const GLchar * vertexShaderCode[] = {
        headerCode,
        vertexCode,
    };
    const GLchar * fragmentShaderCode[] = {
        headerCode,
        fragmentCode,
    };
    GLuint program = GLCompileShader(vertexShader, vertexShaderCode, ARRAY_LENGTH(vertexShaderCode),
                                     fragmentShader, fragmentShaderCode, ARRAY_LENGTH(fragmentShaderCode));
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

static GLuint GLTextureShader() {
    const GLchar *vertexCode =
        "uniform mat4 transform;"
        "in vec2 p;"
        "in vec2 vertUV;"
        "in vec4 vertColor;"
        "noperspective out vec2 fragUV;"
        "noperspective out vec4 fragColor;"
        "void main() {"
        "    gl_Position = transform * vec4(p, 0.0f, 1.0f);"
        "    fragUV = vertUV;"
        "    fragColor = vertColor;"
        "}";
    const GLchar *fragmentCode = 
        "uniform sampler2D txSampler;"
        "out vec4 outColor;"
        "noperspective in vec2 fragUV;"
        "noperspective in vec4 fragColor;"
        "void main() {"
        "    vec4 texSample = texture(txSampler, fragUV);"
        "    vec4 color;"
        "    color.rgb = fragColor.rgb * fragColor.a;"
        "    color.a = fragColor.a;"
        "    outColor = texSample * color;"
        "}";
    return GLShader(vertexCode, fragmentCode);
}

#define GL_LOAD_FUNCTION(name) name = (name##_t *)funcs[i++]
static void gl_load_needed_functions(void **funcs) {
    u32 i = 0;
    // Base functions:
    GL_LOAD_FUNCTION(glBindTexture);
    GL_LOAD_FUNCTION(glBlendFunc);
    GL_LOAD_FUNCTION(glClear);
    GL_LOAD_FUNCTION(glClearColor);
    GL_LOAD_FUNCTION(glDrawArrays);
    GL_LOAD_FUNCTION(glEnable);
    GL_LOAD_FUNCTION(glGenTextures);
    GL_LOAD_FUNCTION(glTexImage2D);
    GL_LOAD_FUNCTION(glTexParameteri);
    GL_LOAD_FUNCTION(glViewport);
    GL_LOAD_FUNCTION(glGetString);
    GL_LOAD_FUNCTION(glBegin);
    GL_LOAD_FUNCTION(glEnd);
    GL_LOAD_FUNCTION(glColor4f);
    GL_LOAD_FUNCTION(glVertex2f);
    GL_LOAD_FUNCTION(glDisable);
    GL_LOAD_FUNCTION(glTexSubImage2D);
    
    // Extended functions:
    GL_LOAD_FUNCTION(glEnableVertexAttribArray);
    GL_LOAD_FUNCTION(glDisableVertexAttribArray);
    GL_LOAD_FUNCTION(glGetAttribLocation);
    GL_LOAD_FUNCTION(glVertexAttribPointer);
    GL_LOAD_FUNCTION(glShaderSource);
    GL_LOAD_FUNCTION(glCompileShader);
    GL_LOAD_FUNCTION(glCreateShader);
    GL_LOAD_FUNCTION(glCreateProgram);
    GL_LOAD_FUNCTION(glAttachShader);
    GL_LOAD_FUNCTION(glLinkProgram);
    GL_LOAD_FUNCTION(glValidateProgram);
    GL_LOAD_FUNCTION(glGetProgramiv);
    GL_LOAD_FUNCTION(glGetShaderInfoLog);
    GL_LOAD_FUNCTION(glGetProgramInfoLog);
    GL_LOAD_FUNCTION(glUseProgram);
    GL_LOAD_FUNCTION(glUniformMatrix2fv);
    GL_LOAD_FUNCTION(glUniformMatrix4fv);
    GL_LOAD_FUNCTION(glGetUniformLocation);
    GL_LOAD_FUNCTION(glUniform1i);
    GL_LOAD_FUNCTION(glDebugMessageCallback);
    GL_LOAD_FUNCTION(glDebugMessageControl);
    GL_LOAD_FUNCTION(glGenVertexArrays);
    GL_LOAD_FUNCTION(glBindVertexArray);
    GL_LOAD_FUNCTION(glGenBuffers);
    GL_LOAD_FUNCTION(glBindBuffer);
    GL_LOAD_FUNCTION(glBufferData);
    GL_LOAD_FUNCTION(glActiveTexture);
    GL_LOAD_FUNCTION(glGenFramebuffers);
    GL_LOAD_FUNCTION(glBindFramebuffer);
    GL_LOAD_FUNCTION(glFramebufferTexture2D);
    GL_LOAD_FUNCTION(glDeleteShader);
}
#undef GL_LOAD_FUNCTION

#if 0
static gl_extensions available_gl_extensions() {
    
    const u8 *vendor = glGetString(GL_VENDOR);
    const u8 *renderer = glGetString(GL_RENDERER);
    const u8 *version = glGetString(GL_VERSION);
    const u8 *slversion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    const u8 *extensions = glGetString(GL_EXTENSIONS);
    gl_extensions result = {0};
    
#if 0
    // Optional extension parsing:
    u8 *extnames = (u8 *)extensions;
    while(*extnames) {
        string name = tokenize(extnames);
        
        if(str_is_at_start_of(name, "GL_ARB_texture_env_add")) {
            int x = 3;
        }
        extnames = eat_word(extnames);
    }
#endif
    
    return result;
}
#endif

static void gl_set_render_region(const rect2s drawRect) {
    s32 w = drawRect.right - drawRect.left;
    s32 h = drawRect.top - drawRect.bottom;
    glViewport(drawRect.left, drawRect.bottom, w, h);
}

static void gl_setup() {
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GlDebugCallback, 0);
    
    GLuint idVertArray = 0;
    glGenVertexArrays(1, &idVertArray);
    glBindVertexArray(idVertArray);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_BLEND);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

static void draw_point(v2 p,
                       v4 color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_POINTS);
    glVertex2f(p.x, p.y);
    glEnd();
}

static void draw_line(v2 a, v2 b, v4 color) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd();
}

static u32 gl_add_texture(const s32 w, const s32 h, void * const data, s32 internal_format, s32 format, s32 type, s32 sampling) {
    u32 texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampling);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}
inline u32 gl_add_texture_float(const s32 w, const s32 h, f32 * const data) {
    return gl_add_texture(w, h, data, GL_R32F, GL_RED, GL_FLOAT, GL_LINEAR);
}
inline u32 gl_add_texture_linear_rgba(const s32 w, const s32 h, u32 * const data) {
    return gl_add_texture(w, h, data, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
}

static u32 glLoadBitmap(const u8 * const data, const s32 w, const s32 h) {
    u32 bmp;
    glGenTextures(1, &bmp);
    glBindTexture(GL_TEXTURE_2D, bmp);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return bmp;
}

static void gl_create_render_target(const s32 w, const s32 h, u32 *fb_out, u32 *tex_out) {
    u32 fb;
    u32 tex;
    
    tex = glLoadBitmap(0, w, h);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    
    *fb_out = fb;
    *tex_out = tex;
}

// @Cleanup: Maybe use an enum translation table?
// We should wait for a second renderer to see.
static void update_texture(u32 texture, s32 x, s32 y, s32 width, s32 height, void *data, s32 format, s32 type) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format, type, data);
}
inline void update_texture_rgba(u32 texture, s32 x, s32 y, s32 width, s32 height, void *data) {
    update_texture(texture, x, y, width, height, data, GL_RGBA, GL_UNSIGNED_BYTE);
}
inline void update_texture_float(u32 texture, s32 x, s32 y, s32 width, s32 height, void *data) {
    update_texture(texture, x, y, width, height, data, GL_RED, GL_FLOAT);
}

static void draw_clear(v4 color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

