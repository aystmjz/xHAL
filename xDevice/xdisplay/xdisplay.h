#ifndef __xdisp_H
#define __xdisp_H

#include "xhal_def.h"
#include "xhal_os.h"

#define WHITE 0XFFFF
#define BLACK 0X0000

typedef enum
{
    XDISP_PIXFMT_MONO_HBIT = 0, /* 单色 1bpp，横向 bit-packed */
    XDISP_PIXFMT_MONO_VBIT,     /* 单色 1bpp，纵向 bit-packed */
    XDISP_PIXFMT_GRAY8,         /* 灰度 8bpp */
    XDISP_PIXFMT_RGB565,        /* RGB565，16bpp */
} xdisp_pixfmt_t;

typedef enum
{
    XDISP_ROT_0 = 0, /* 不旋转 */
    XDISP_ROT_90,    /* 顺时针 90° */
    XDISP_ROT_180,   /* 顺时针 180° */
    XDISP_ROT_270,   /* 顺时针 270° */
} xdisp_rotation_t;

typedef struct xdisp_rect
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} xdisp_rect_t;

typedef struct
{
    int16_t h;
    int16_t v;
} xdisp_spacing_t;

typedef struct xdisp
{
    uint8_t *buffer;
    uint32_t buf_size;
    xdisp_pixfmt_t pixfmt;

    uint32_t stride;
    uint16_t mem_width;
    uint16_t mem_height;

    uint16_t panel_width;
    uint16_t panel_height;
    uint16_t offset_x;
    uint16_t offset_y;
    xdisp_rotation_t rotation;

    uint32_t color;
    xdisp_rect_t rect;

    uint16_t in_draw;
    uint16_t is_dirty;
    xdisp_rect_t dirty;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xdisp_t;

typedef xhal_err_t (*xdisp_get_pixel_t)(void *ctx, u32 *color, u32 bg, u16 x,
                                        u16 y);
typedef xhal_err_t (*xdisp_get_metrics_t)(void *ctx, u16 *w, u16 *h);

typedef struct xdisp_src
{
    void *ctx;
    xdisp_get_pixel_t get_pixel;
    xdisp_get_metrics_t get_metrics;
} xdisp_src_t;

typedef struct xdisp_font
{
    void *data;
    xdisp_spacing_t gap;
    uint32_t codepoint;
    const xdisp_src_t *src;
} xdisp_font_t;

typedef enum
{
    XDISP_REFRESH_DIRTY = 0, /* 仅刷新脏区域 */
    XDISP_REFRESH_FULL,      /* 强制整屏刷新 */
} xdisp_refresh_mode_t;

typedef struct xdisp_refresh_info
{
    uint8_t *ptr;
    uint32_t max_offset;
    uint16_t stride;
    xdisp_rect_t rect;
} xdisp_refresh_info_t;

xhal_err_t xdisp_init(xdisp_t *disp, u8 *buf, u32 buf_size, u16 mem_width,
                      u16 mem_height, u16 panel_width, u16 panel_height,
                      xdisp_pixfmt_t pixfmt, u32 color);

xhal_err_t xdisp_get_view_size(xdisp_t *disp, u16 *w, u16 *h);
xhal_err_t xdisp_get_refresh_info(xdisp_t *disp, xdisp_refresh_mode_t mode,
                                  xdisp_refresh_info_t *info);

xhal_err_t xdisp_set_offset(xdisp_t *disp, u16 offset_x, u16 offset_y);
xhal_err_t xdisp_set_rotation(xdisp_t *disp, xdisp_rotation_t rotation);
xhal_err_t xdisp_set_rect(xdisp_t *disp, u16 x, u16 y, u16 w, u16 h);
xhal_err_t xdisp_set_color(xdisp_t *disp, u32 color);

xhal_err_t xdisp_clear(xdisp_t *disp, u32 color);
xhal_err_t xdisp_draw_pixel(xdisp_t *disp, u16 x, u16 y);
xhal_err_t xdisp_get_pixel(xdisp_t *disp, u16 x, u16 y, u32 *color);

xhal_err_t xdisp_draw_line(xdisp_t *disp, u16 x0, u16 y0, u16 x1, u16 y1);
xhal_err_t xdisp_draw_circle(xdisp_t *disp, u16 x0, u16 y0, u16 rad, bool full);
xhal_err_t xdisp_draw_rectangle(xdisp_t *disp, u16 x, u16 y, u16 w, u16 h,
                                bool full);
xhal_err_t xdisp_draw_chart(xdisp_t *disp, uint8_t *data, u16 num, u32 min,
                            u32 max);

xhal_err_t xdisp_draw_image(xdisp_t *disp, uint16_t x, uint16_t y,
                            const xdisp_src_t *src);
xhal_err_t xdisp_draw_char(xdisp_t *disp, u16 x, u16 y, u32 codepoint,
                           xdisp_font_t *font);
xhal_err_t xdisp_draw_num(xdisp_t *disp, u16 x, u16 y, u32 num,
                          xdisp_font_t *font);

xhal_err_t xdisp_draw_string(xdisp_t *disp, xhal_size_t *read,
                             xdisp_font_t *font, const char *str);
xhal_err_t xdisp_printf(xdisp_t *disp, xdisp_font_t *font, const char *fmt,
                        ...);

xhal_err_t xdisp_utf8_to_codepoint(const char *utf8, u32 *cp, u8 *read);

xhal_err_t xdisp_begin(xdisp_t *disp);
xhal_err_t xdisp_end(xdisp_t *disp);

#endif