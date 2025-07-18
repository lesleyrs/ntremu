#ifndef GPU_H
#define GPU_H

// #include <pthread.h>

#include "ppu.h"
#include "types.h"

#define MAX_VTX 6144
#define MAX_POLY 2048

#define MAX_POLY_N 10

enum {
    MTX_MODE = 0x10,
    MTX_PUSH,
    MTX_POP,
    MTX_STORE,
    MTX_RESTORE = 0x14,
    MTX_IDENTITY,
    MTX_LOAD_44,
    MTX_LOAD_43,
    MTX_MULT_44 = 0x18,
    MTX_MULT_43,
    MTX_MULT_33,
    MTX_SCALE,
    MTX_TRANS = 0x1c,

    COLOR = 0x20,
    NORMAL,
    TEXCOORD,
    VTX_16,
    VTX_10 = 0x24,
    VTX_XY,
    VTX_XZ,
    VTX_YZ,
    VTX_DIFF = 0x28,
    POLYGON_ATTR,
    TEXIMAGE_PARAM,
    PLTT_BASE,

    DIF_AMB = 0x30,
    SPE_EMI,
    LIGHT_VECTOR,
    LIGHT_COLOR,
    SHININESS = 0x34,

    BEGIN_VTXS = 0x40,
    END_VTXS,

    SWAP_BUFFERS = 0x50,

    VIEWPORT = 0x60,

    BOX_TEST = 0x70,
    POS_TEST,
    VEC_TEST
};

enum { MM_PROJ, MM_POS, MM_POSVEC, MM_TEX };
enum { POLY_TRIS, POLY_QUADS, POLY_TRI_STRIP, POLY_QUAD_STRIP };

enum {
    TEX_NONE,
    TEX_A3I5,
    TEX_2BPP,
    TEX_4BPP,
    TEX_8BPP,
    TEX_COMPRESS,
    TEX_A5I3,
    TEX_DIRECT
};

enum { TEXTF_NONE, TEXTF_TEXCOORD, TEXTF_NORMAL, TEXTF_VTX };

enum { POLYMODE_MOD, POLYMODE_DECAL, POLYMODE_TOON, POLYMODE_SHADOW };

typedef union {
    u32 w;
    struct {
        u32 light_enable : 4;
        u32 mode : 2;
        u32 back : 1;
        u32 front : 1;
        u32 unused : 3;
        u32 depth_transparent : 1;
        u32 farplane : 1;
        u32 onedot : 1;
        u32 depth_test : 1;
        u32 fog : 1;
        u32 alpha : 5;
        u32 unused2 : 3;
        u32 id : 6;
        u32 unused3 : 2;
    };
} PolygonAttr;

typedef union {
    u32 w;
    struct {
        u32 dif_r : 5;
        u32 dif_g : 5;
        u32 dif_b : 5;
        u32 vtx_color : 1;
        u32 amb_r : 5;
        u32 amb_g : 5;
        u32 amb_b : 5;
        u32 unused : 1;
    };
} Material0;

typedef union {
    u32 w;
    struct {
        u32 spe_r : 5;
        u32 spe_g : 5;
        u32 spe_b : 5;
        u32 shininess : 1;
        u32 emi_r : 5;
        u32 emi_g : 5;
        u32 emi_b : 5;
        u32 unused : 1;
    };
} Material1;

typedef union {
    u32 w;
    struct {
        u32 offset : 16;
        u32 s_rep : 1;
        u32 t_rep : 1;
        u32 s_flip : 1;
        u32 t_flip : 1;
        u32 s_size : 3;
        u32 t_size : 3;
        u32 format : 3;
        u32 color0 : 1;
        u32 transform : 2;
    };
} TexParam;

typedef struct {
    float p[4];
} vec4;

typedef struct {
    float p[4][4];
} mat4;

typedef struct {
    vec4 v;
    vec4 vt;
    float r, g, b;
    int sx, sy;
} vertex;

typedef struct {
    vertex* p[MAX_POLY_N];
    int n;
    PolygonAttr attr;
    TexParam texparam;
    u32 pltt_base;
} poly;

struct interp_attrs {
    int x;
    float z, w;
    float s, t;
    float r, g, b;
};

typedef struct _NDS NDS;

typedef struct {
    NDS* master;

    u32 framebuffers[2][NDS_SCREEN_H][NDS_SCREEN_W];
    u32 (*screen)[NDS_SCREEN_W];
    u32 (*screen_back)[NDS_SCREEN_W];

    float depth_buf[NDS_SCREEN_H][NDS_SCREEN_W];
    u8 polyid_buf[NDS_SCREEN_H][NDS_SCREEN_W];
    union {
        u8 b;
        struct {
            u8 blend : 1;
            u8 stencil : 1;
            u8 edge : 1;
            u8 fog : 1;
            u8 pad : 4;
        };
    } attr_buf[NDS_SCREEN_H][NDS_SCREEN_W];

    u8* texram[4];
    u16* texpal[6];

    vertex vertexrambufs[2][MAX_VTX];
    poly polygonrambufs[2][MAX_POLY];

    vertex* vertexram;
    poly* polygonram;
    u16 n_verts;
    u16 n_polys;

    vertex* vertexram_rendering;
    poly* polygonram_rendering;
    u16 n_polys_rendering;

    bool blocked;
    bool drawing;
    bool pending_swapbuffers;

    FIFO(u8, 256) cmd_fifo;
    FIFO(u32, 256) param_fifo;
    u8 params_pending;

    mat4 projmtx;
    mat4 projmtx_stk[1];
    u8 projstk_size;

    mat4 posmtx;
    mat4 posmtx_stk[32];

    mat4 vecmtx;
    mat4 vecmtx_stk[32];

    u8 mtxstk_size;

    mat4 texmtx;
    mat4 texmtx_stack[1];

    int mtx_mode;
    mat4 clipmtx;

    bool mtx_dirty;

    int poly_mode;
    PolygonAttr cur_attr;
    PolygonAttr next_attr;

    vertex cur_vtx;
    vec4 cur_texcoord;

    int cur_vtx_ct;
    bool tri_orient;

    vertex cur_poly_vtxs[4];
    vertex* cur_poly_strip[4];

    vec4 lightvec[4];
    vec4 halfvec[4];
    u16 lightcol[4];
    Material0 cur_mtl0;
    Material1 cur_mtl1;
    u8 shininess[128];

    TexParam cur_texparam;
    u32 cur_pltt_base;

    bool w_buffer;
    bool autosort;

    int view_x;
    int view_y;
    int view_w;
    int view_h;

} GPU;

// extern pthread_t gpu_thread;
// extern pthread_mutex_t gpu_mutex;
// extern pthread_cond_t gpu_cond;

void init_gpu_thread(GPU* gpu);
void destroy_gpu_thread();

void gpu_init_ptrs(GPU* gpu);

void gxfifo_write(GPU* gpu, u32 command);
void gxcmd_execute(GPU* gpu);
void gxcmd_execute_all(GPU* gpu);
void swap_buffers(GPU* gpu);

void update_mtxs(GPU* gpu);

void gpu_render(GPU* gpu);

#endif
