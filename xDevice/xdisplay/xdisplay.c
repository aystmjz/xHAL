#include "xdisplay.h"
#include "xhal_assert.h"
#include "xhal_bit.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include <stdio.h>

XLOG_TAG("xDISPLAY");

#define XDISP_BUFF_SIZE (512) /* 缓冲区大小 */

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xdisp_mutex_attr = {
    .name      = "xdisp_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static xhal_err_t _map_to_mem(xdisp_t *disp, u16 x, u16 y, u16 *mx, u16 *my);
static xhal_err_t _draw_pixel(xdisp_t *disp, u16 x, u16 y, u32 color);
static inline uint32_t _pow(u16 m, u16 n);

xhal_err_t xdisp_init(xdisp_t *disp, u8 *buf, u32 buf_size, u16 mem_width,
                      u16 mem_height, u16 panel_width, u16 panel_height,
                      xdisp_pixfmt_t pixfmt, u32 color)
{
    xassert_not_null(disp);
    xassert_not_null(buf);

    xhal_err_t ret = XHAL_OK;

    disp->pixfmt = pixfmt;

    switch (pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
        disp->stride = XHAL_CEIL(mem_width, 8) / 8;
        if (buf_size < (disp->stride * mem_height))
        {
            return XHAL_ERR_NOT_ENOUGH;
        }
        if (panel_width % 8)
        {
            return XHAL_ERR_INVALID;
        }
        break;

    case XDISP_PIXFMT_MONO_VBIT:
        disp->stride = mem_width;
        if (buf_size < (disp->stride * XHAL_CEIL(mem_height, 8) / 8))
        {
            return XHAL_ERR_NOT_ENOUGH;
        }
        if (panel_height % 8)
        {
            return XHAL_ERR_INVALID;
        }
        break;

    case XDISP_PIXFMT_GRAY8:
        disp->stride = mem_width;
        if (buf_size < (disp->stride * mem_height))
        {
            return XHAL_ERR_NOT_ENOUGH;
        }
        break;

    case XDISP_PIXFMT_RGB565:
        disp->stride = mem_width * 2;
        if (buf_size < (disp->stride * mem_height))
        {
            return XHAL_ERR_NOT_ENOUGH;
        }
        break;

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }
    disp->buffer     = buf;
    disp->buf_size   = buf_size;
    disp->mem_width  = mem_width;
    disp->mem_height = mem_height;

    if (panel_width < mem_width || panel_height < mem_height)
    {
        return XHAL_ERR_INVALID;
    }
    disp->panel_width  = panel_width;
    disp->panel_height = panel_height;

    XHAL_TRY(ret, xdisp_clear(disp, color));
    XHAL_TRY(ret, xdisp_set_color(disp, color));
    XHAL_TRY(ret, xdisp_set_rect(disp, 0, 0, 0, 0));

    disp->offset_x = 0;
    disp->offset_y = 0;
    disp->rotation = XDISP_ROT_0;
    disp->in_draw  = false;

#ifdef XHAL_OS_SUPPORTING
    disp->mutex = osMutexNew(&xdisp_mutex_attr);
    xassert_not_null(disp->mutex);
#endif

    return ret;
}

xhal_err_t xdisp_get_view_size(xdisp_t *disp, u16 *w, u16 *h)
{
    xassert_not_null(disp);
    xassert_not_null(w);
    xassert_not_null(h);

    switch (disp->rotation)
    {
    case XDISP_ROT_0:
    case XDISP_ROT_180:
        *w = disp->panel_width;
        *h = disp->panel_height;
        break;

    case XDISP_ROT_90:
    case XDISP_ROT_270:
        *w = disp->panel_height;
        *h = disp->panel_width;
        break;

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    return XHAL_OK;
}

xhal_err_t xdisp_get_refresh_info(xdisp_t *disp, xdisp_refresh_mode_t mode,
                                  xdisp_refresh_info_t *info)
{
    xassert_not_null(disp);
    xassert_not_null(info);

    if (disp->in_draw)
    {
        return XHAL_ERR_BUSY;
    }

    switch (mode)
    {
    case XDISP_REFRESH_DIRTY:
        if (!disp->is_dirty)
        {
            return XHAL_ERR_EMPTY;
        }
        info->rect.x = XHAL_SUB_SATURATE_MIN(disp->dirty.x, disp->offset_x, 0);
        info->rect.y = XHAL_SUB_SATURATE_MIN(disp->dirty.y, disp->offset_y, 0);
        info->rect.w =
            XHAL_MIN(disp->dirty.x + disp->dirty.w, disp->mem_width) -
            info->rect.x;
        info->rect.h =
            XHAL_MIN(disp->dirty.y + disp->dirty.h, disp->mem_height) -
            info->rect.y;

        break;

    case XDISP_REFRESH_FULL:
        info->rect.x = 0;
        info->rect.y = 0;
        info->rect.w =
            XHAL_MIN(disp->panel_width, disp->mem_width - disp->offset_x);
        info->rect.h =
            XHAL_MIN(disp->panel_height, disp->mem_height - disp->offset_y);
        break;

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    uint32_t buf_offset = 0;
    uint16_t y          = info->rect.y + disp->offset_y;
    uint16_t x          = info->rect.x + disp->offset_x;

    switch (disp->pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
        buf_offset = y * disp->stride + x / 8;
        break;

    case XDISP_PIXFMT_MONO_VBIT:
        buf_offset = y / 8 * disp->stride + x;
        break;

    case XDISP_PIXFMT_GRAY8:
        buf_offset = y * disp->stride + x;
        break;

    case XDISP_PIXFMT_RGB565:
        buf_offset = y * disp->stride + x * 2;
        break;

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    info->stride     = disp->stride;
    info->ptr        = disp->buffer + buf_offset;
    info->max_offset = disp->buf_size - buf_offset - 1;
    disp->is_dirty   = false;

    return XHAL_OK;
}

xhal_err_t xdisp_set_offset(xdisp_t *disp, u16 offset_x, u16 offset_y)
{
    xassert_not_null(disp);

    if (offset_x >= disp->mem_width || offset_y >= disp->mem_height)
    {
        return XHAL_ERR_INVALID;
    }

    switch (disp->pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
        if (offset_x % 8)
        {
            return XHAL_ERR_INVALID;
        }
        break;

    case XDISP_PIXFMT_MONO_VBIT:
        if (offset_y % 8)
        {
            return XHAL_ERR_INVALID;
        }
        break;
    }

    disp->offset_x = offset_x;
    disp->offset_y = offset_y;

    /* 标记整个缓冲区为脏区域 */
    disp->is_dirty = true;
    disp->dirty.x  = 0;
    disp->dirty.y  = 0;
    disp->dirty.w  = disp->mem_width;
    disp->dirty.h  = disp->mem_height;

    return XHAL_OK;
}

xhal_err_t xdisp_set_rotation(xdisp_t *disp, xdisp_rotation_t rotation)
{
    xassert_not_null(disp);

    disp->rotation = rotation;

    /* 标记整个缓冲区为脏区域 */
    disp->is_dirty = true;
    disp->dirty.x  = 0;
    disp->dirty.y  = 0;
    disp->dirty.w  = disp->mem_width;
    disp->dirty.h  = disp->mem_height;

    return XHAL_OK;
}

xhal_err_t xdisp_set_rect(xdisp_t *disp, u16 x, u16 y, u16 w, u16 h)
{
    xassert_not_null(disp);

    xhal_err_t ret;
    uint16_t view_width = 0, view_height = 0;

    XHAL_TRY(ret, xdisp_get_view_size(disp, &view_width, &view_height));

    if (x >= view_width || y >= view_height)
    {
        return XHAL_ERR_INVALID;
    }

    if (w == 0 || (x + w) > view_width)
    {
        w = view_width - x;
    }

    if (h == 0 || (y + h) > view_height)
    {
        h = view_height - y;
    }

    disp->rect.x = x;
    disp->rect.y = y;
    disp->rect.w = w;
    disp->rect.h = h;

    return XHAL_OK;
}

xhal_err_t xdisp_set_color(xdisp_t *disp, u32 color)
{
    disp->color = color;

    return XHAL_OK;
}

xhal_err_t _draw_pixel(xdisp_t *disp, u16 x, u16 y, u32 color)
{
    xhal_err_t ret      = XHAL_OK;
    uint16_t view_width = 0, view_height = 0;

    XHAL_TRY(ret, xdisp_get_view_size(disp, &view_width, &view_height));

    if (x >= view_width || y >= view_height)
    {
        return XHAL_OK;
    }

    uint16_t mx = 0, my = 0;
    XHAL_TRY(ret, _map_to_mem(disp, x, y, &mx, &my));

    if (mx >= disp->mem_width || my >= disp->mem_height)
    {
        return XHAL_OK;
    }

    uint8_t *buf = disp->buffer;

    switch (disp->pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
    {
        /* 每字节 8 个横向像素 */
        uint32_t byte_index = my * disp->stride + (mx / 8);
        uint8_t bit_index   = 7 - (mx % 8);

        BIT_SET(buf[byte_index], bit_index, XHAL_BOOL(color));
        break;
    }

    case XDISP_PIXFMT_MONO_VBIT:
    {
        /* 每字节 8 个纵向像素风格 */
        uint32_t byte_index = (my / 8) * disp->stride + mx;
        uint8_t bit_index   = my % 8;

        BIT_SET(buf[byte_index], bit_index, XHAL_BOOL(color));
        break;
    }

    case XDISP_PIXFMT_GRAY8:
    {
        uint32_t index = my * disp->stride + mx;
        buf[index]     = (uint8_t)color;
        break;
    }

    case XDISP_PIXFMT_RGB565:
    {
        uint32_t index = my * disp->stride + mx * 2;
        buf[index + 0] = BITS_GET(color, 8, 0);
        buf[index + 1] = BITS_GET(color, 8, 8);
        break;
    }

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    if (!disp->is_dirty)
    {
        disp->dirty.x  = mx;
        disp->dirty.y  = my;
        disp->dirty.w  = 1;
        disp->dirty.h  = 1;
        disp->is_dirty = true;
        return XHAL_OK;
    }

    uint16_t left   = disp->dirty.x;
    uint16_t top    = disp->dirty.y;
    uint16_t right  = disp->dirty.x + disp->dirty.w - 1;
    uint16_t bottom = disp->dirty.y + disp->dirty.h - 1;

    left   = XHAL_MIN(left, mx);
    top    = XHAL_MIN(top, my);
    right  = XHAL_MAX(right, mx);
    bottom = XHAL_MAX(bottom, my);

    disp->dirty.x = left;
    disp->dirty.y = top;
    disp->dirty.w = right - left + 1;
    disp->dirty.h = bottom - top + 1;

    return XHAL_OK;
}

xhal_err_t xdisp_draw_pixel(xdisp_t *disp, u16 x, u16 y)
{
    xassert_not_null(disp);
    xassert_not_null(disp->buffer);

    return _draw_pixel(disp, x, y, disp->color);
}

xhal_err_t xdisp_get_pixel(xdisp_t *disp, u16 x, u16 y, u32 *color)
{
    xassert_not_null(disp);
    xassert_not_null(disp->buffer);
    xassert_not_null(color);

    xhal_err_t ret      = XHAL_OK;
    uint16_t view_width = 0, view_height = 0;

    XHAL_TRY(ret, xdisp_get_view_size(disp, &view_width, &view_height));

    *color = BLACK;

    if (x >= view_width || y >= view_height)
    {
        return XHAL_OK;
    }

    uint16_t mx = 0, my = 0;
    XHAL_TRY(ret, _map_to_mem(disp, x, y, &mx, &my));

    if (mx >= disp->mem_width || my >= disp->mem_height)
    {
        return XHAL_OK;
    }

    uint8_t *buf = disp->buffer;

    switch (disp->pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
    {
        uint32_t byte_index = my * disp->stride + (mx / 8);
        uint8_t bit_index   = 7 - (mx % 8);

        *color = BIT_GET(buf[byte_index], bit_index);
        break;
    }

    case XDISP_PIXFMT_MONO_VBIT:
    {
        uint32_t byte_index = (my / 8) * disp->stride + mx;
        uint8_t bit_index   = my % 8;

        *color = BIT_GET(buf[byte_index], bit_index);
        break;
    }

    case XDISP_PIXFMT_GRAY8:
    {
        uint32_t index = my * disp->stride + mx;
        *color         = buf[index];
        break;
    }

    case XDISP_PIXFMT_RGB565:
    {
        uint32_t index = my * disp->stride + mx * 2;
        BITS_SET(*color, 8, 0, buf[index + 0]);
        BITS_SET(*color, 8, 8, buf[index + 1]);
    }

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    return XHAL_OK;
}

xhal_err_t xdisp_clear(xdisp_t *disp, u32 color)
{
    xassert_not_null(disp);
    xassert_not_null(disp->buffer);

    switch (disp->pixfmt)
    {
    case XDISP_PIXFMT_MONO_HBIT:
    case XDISP_PIXFMT_MONO_VBIT:
    {
        uint8_t fill = color ? 0xFF : 0x00;
        xmemset(disp->buffer, fill, disp->buf_size);
        break;
    }

    case XDISP_PIXFMT_GRAY8:
    {
        xmemset(disp->buffer, (uint8_t)color, disp->buf_size);
        break;
    }

    case XDISP_PIXFMT_RGB565:
    {
        uint16_t c = (uint16_t)color;

        for (uint16_t y = 0; y < disp->mem_height; y++)
        {
            uint8_t *row = disp->buffer + y * disp->stride;
            for (uint16_t x = 0; x < disp->mem_width; x++)
            {
                row[x * 2 + 0] = BITS_GET(c, 8, 8);
                row[x * 2 + 1] = BITS_GET(c, 8, 0);
            }
        }
        break;
    }

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    /* 标记整个缓冲区为脏区域 */
    disp->is_dirty = true;
    disp->dirty.x  = 0;
    disp->dirty.y  = 0;
    disp->dirty.w  = disp->mem_width;
    disp->dirty.h  = disp->mem_height;

    return XHAL_OK;
}

xhal_err_t xdisp_draw_line(xdisp_t *disp, u16 x0, u16 y0, u16 x1, u16 y1)

{
    xassert_not_null(disp);

    xhal_err_t ret = XHAL_OK;

    int32_t dx     = XHAL_ABS(x1 - x0);
    int32_t dy     = -XHAL_ABS(y1 - y0);
    int32_t step_x = (x0 < x1) ? 1 : -1;
    int32_t step_y = (y0 < y1) ? 1 : -1;

    int32_t err    = dx + dy;
    int32_t curt_x = x0;
    int32_t curt_y = y0;

    while (1)
    {
        XHAL_TRY(ret, xdisp_draw_pixel(disp, curt_x, curt_y));

        if (curt_x == x1 && curt_y == y1)
        {
            break;
        }

        int32_t e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            curt_x += step_x;
        }

        if (e2 <= dx)
        {
            err += dx;
            curt_y += step_y;
        }
    }

    return ret;
}

xhal_err_t xdisp_draw_rectangle(xdisp_t *disp, u16 x, u16 y, u16 w, u16 h,
                                bool full)
{
    xassert_not_null(disp);

    xhal_err_t ret = XHAL_OK;

    uint16_t x0 = x;
    uint16_t y0 = y;
    uint16_t x1 = x + w - 1;
    uint16_t y1 = y + h - 1;

    if (full)
    {
        for (uint16_t sy = y0; sy < y1; sy++)
        {
            XHAL_TRY(ret, xdisp_draw_line(disp, x0, sy, x1, sy));
        }
    }
    else
    {
        XHAL_TRY(ret, xdisp_draw_line(disp, x0, y0, x1, y0));
        XHAL_TRY(ret, xdisp_draw_line(disp, x0, y1, x1, y1));
        XHAL_TRY(ret, xdisp_draw_line(disp, x0, y0, x0, y1));
        XHAL_TRY(ret, xdisp_draw_line(disp, x1, y0, x1, y1));
    }

    return ret;
}

xhal_err_t xdisp_draw_circle(xdisp_t *disp, u16 x0, u16 y0, u16 rad, bool full)
{
    xassert_not_null(disp);

    xhal_err_t ret = XHAL_OK;

    if (rad == 0)
    {
        return XHAL_ERR_INVALID;
    }

    int32_t x = 0;
    int32_t y = rad;
    int32_t d = 1 - rad;

    while (x <= y)
    {
        if (full)
        {
            for (int32_t i = x0 - x; i <= x0 + x; i++)
            {
                XHAL_TRY(ret, xdisp_draw_pixel(disp, i, y0 + y));
                XHAL_TRY(ret, xdisp_draw_pixel(disp, i, y0 - y));
            }
            for (int32_t i = x0 - y; i <= x0 + y; i++)
            {
                XHAL_TRY(ret, xdisp_draw_pixel(disp, i, y0 + x));
                XHAL_TRY(ret, xdisp_draw_pixel(disp, i, y0 - x));
            }
        }
        else
        {
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 + x, y0 + y));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 - x, y0 + y));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 + x, y0 - y));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 - x, y0 - y));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 + y, y0 + x));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 - y, y0 + x));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 + y, y0 - x));
            XHAL_TRY(ret, xdisp_draw_pixel(disp, x0 - y, y0 - x));
        }

        x++;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
    }

    return ret;
}

xhal_err_t xdisp_draw_chart(xdisp_t *disp, uint8_t *data, u16 num, u32 min,
                            u32 max)
{
    xassert_not_null(disp);
    xassert_not_null(data);

    xhal_err_t ret = XHAL_OK;

    if (num < 2)
    {
        return XHAL_ERR_INVALID;
    }

    uint16_t x = disp->rect.x;
    uint16_t y = disp->rect.y;
    uint16_t w = disp->rect.w;
    uint16_t h = disp->rect.h;

    if (max <= min)
    {
        max = min + 1;
    }

    float x_step  = w / (float)(num - 1);
    float y_scale = h / (float)(max - min);

    for (uint16_t i = 0; i < num - 1; i++)
    {
        uint16_t x0 = x + (u16)(i * x_step);
        uint16_t y0 = y + h - (u16)((data[i] - min) * y_scale);

        uint16_t x1 = x + (u16)((i + 1) * x_step);
        uint16_t y1 = y + h - (u16)((data[i + 1] - min) * y_scale);

        XHAL_TRY(ret, xdisp_draw_line(disp, x0, y0, x1, y1));
    }

    return ret;
}

xhal_err_t xdisp_draw_image(xdisp_t *disp, uint16_t x, uint16_t y,
                            const xdisp_src_t *src)
{
    xassert_not_null(disp);
    xassert_not_null(src);
    xassert_not_null(src->get_metrics);
    xassert_not_null(src->get_pixel);

    xhal_err_t ret = XHAL_OK;
    uint16_t w = 0, h = 0;
    uint16_t view_width = 0, view_height = 0;

    XHAL_TRY(ret, src->get_metrics(src->ctx, &w, &h));
    XHAL_TRY(ret, xdisp_get_view_size(disp, &view_width, &view_height));

    uint16_t x1 = XHAL_MIN(x + w, view_width);
    uint16_t y1 = XHAL_MIN(y + h, view_height);

    uint32_t color = 0, bg = 0;
    for (uint16_t y0 = y; y0 < y1; y0++)
    {
        for (uint16_t x0 = x; x0 < x1; x0++)
        {
            XHAL_TRY(ret, xdisp_get_pixel(disp, x0, y0, &bg));
            XHAL_TRY(ret, src->get_pixel(src->ctx, &color, bg, x0 - x, y0 - y));
            XHAL_TRY(ret, _draw_pixel(disp, x0, y0, color));
        }
    }

    return ret;
}

xhal_err_t xdisp_draw_char(xdisp_t *disp, u16 x, u16 y, u32 codepoint,
                           xdisp_font_t *font)
{
    xassert_not_null(disp);
    xassert_not_null(font);

    xhal_err_t ret = XHAL_OK;

    font->codepoint = codepoint;

    XHAL_TRY(ret, xdisp_draw_image(disp, x, y, font->src));

    return ret;
}

xhal_err_t xdisp_draw_num(xdisp_t *disp, u16 x, u16 y, u32 num,
                          xdisp_font_t *font)
{
    xassert_not_null(disp);
    xassert_not_null(font);

    xhal_err_t ret = XHAL_OK;

    if (num == 0)
    {
        return xdisp_draw_char(disp, x, y, '0', font);
    }

    /* 计算位数 */
    uint32_t tmp   = num;
    uint8_t digits = 0;
    while (tmp)
    {
        tmp /= 10;
        digits++;
    }

    /* 从最高位开始绘制 */
    for (uint8_t i = digits; i > 0; i--)
    {
        uint32_t div       = _pow(10, i - 1);
        uint32_t digit     = (num / div) % 10;
        uint32_t codepoint = (u32)('0' + digit);
        uint16_t w = 0, h = 0;

        XHAL_TRY(ret, xdisp_draw_char(disp, x, y, codepoint, font));
        XHAL_TRY(ret, font->src->get_metrics(font, &w, &h));

        x += w;
    }

    return ret;
}

xhal_err_t xdisp_utf8_to_codepoint(const char *utf8, u32 *cp, u8 *read)
{
    xassert_not_null(utf8);
    xassert_not_null(cp);
    xassert_not_null(read);

    *read = 0;

    if (utf8[0] == '\0')
    {
        return XHAL_ERR_INVALID;
    }

    uint32_t codepoint = 0;
    if (BITS_GET(utf8[0], 1, 7) == 0x00)
    {
        /* 1-byte UTF-8: 0xxxxxxx */
        BITS_SET(codepoint, 7, 0, BITS_GET(utf8[0], 7, 0));
        *read = 1;
    }
    else if (BITS_GET(utf8[0], 3, 5) == 0x06)
    {
        if (utf8[1] == '\0')
        {
            *read = 1;
            return XHAL_ERR_INVALID;
        }
        /* 2-byte UTF-8: 110xxxxx 10xxxxxx */
        BITS_SET(codepoint, 5, 6, BITS_GET(utf8[0], 5, 0));
        BITS_SET(codepoint, 6, 0, BITS_GET(utf8[1], 6, 0));
        *read = 2;
    }
    else if (BITS_GET(utf8[0], 4, 4) == 0x0E)
    {
        if (utf8[1] == '\0')
        {
            *read = 1;
            return XHAL_ERR_INVALID;
        }
        if (utf8[2] == '\0')
        {
            *read = 2;
            return XHAL_ERR_INVALID;
        }
        /* 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx */
        BITS_SET(codepoint, 4, 12, BITS_GET(utf8[0], 4, 0));
        BITS_SET(codepoint, 6, 6, BITS_GET(utf8[1], 6, 0));
        BITS_SET(codepoint, 6, 0, BITS_GET(utf8[2], 6, 0));
        *read = 3;
    }
    else if (BITS_GET(utf8[0], 5, 3) == 0x1E)
    {
        if (utf8[1] == '\0')
        {
            *read = 1;
            return XHAL_ERR_INVALID;
        }
        if (utf8[2] == '\0')
        {
            *read = 2;
            return XHAL_ERR_INVALID;
        }
        if (utf8[3] == '\0')
        {
            *read = 3;
            return XHAL_ERR_INVALID;
        }
        /* 4-byte UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        BITS_SET(codepoint, 3, 18, BITS_GET(utf8[0], 3, 0));
        BITS_SET(codepoint, 6, 12, BITS_GET(utf8[1], 6, 0));
        BITS_SET(codepoint, 6, 6, BITS_GET(utf8[2], 6, 0));
        BITS_SET(codepoint, 6, 0, BITS_GET(utf8[3], 6, 0));
        *read = 4;
    }
    else
    {
        return XHAL_ERR_INVALID;
    }

    *cp = codepoint;

    return XHAL_OK;
}

xhal_err_t xdisp_draw_string(xdisp_t *disp, xhal_size_t *read,
                             xdisp_font_t *font, const char *str)
{
    xassert_not_null(disp);
    xassert_not_null(str);
    xassert_not_null(font);

    xhal_err_t ret = XHAL_OK;

    uint16_t cursor_x = disp->rect.x;
    uint16_t cursor_y = disp->rect.y;
    uint16_t start_x  = disp->rect.x;
    uint16_t end_x    = disp->rect.x + disp->rect.w;
    uint16_t end_y    = disp->rect.y + disp->rect.h;

    uint16_t w = 0, h = 0;

    font->codepoint = ' ';
    XHAL_TRY(ret, font->src->get_metrics(font, &w, &h));

    const char *p = str;

    while (*p)
    {
        /* 换行与回车处理 */
        if (*p == '\r')
        {
            cursor_x = start_x;
            p++;

            if (*p == '\n')
            {
                cursor_y += h + font->gap.h;
                p++;
            }

            if (cursor_y + h > end_y)
            {
                break;
            }

            continue;
        }
        else if (*p == '\n')
        {
            cursor_x = start_x;
            cursor_y += h + font->gap.h;
            p++;

            if (cursor_y + h > end_y)
            {
                break;
            }

            continue;
        }

        /* UTF-8 解码 */
        uint8_t read = 0;
        uint32_t cp  = 0;

        if (xdisp_utf8_to_codepoint(p, &cp, &read) != XHAL_OK)
        {
            cp = 0xFFFD;
        }

        p += read;

        font->codepoint = cp;
        XHAL_TRY(ret, font->src->get_metrics(font, &w, &h));

        /* 自动换行 */
        if (cursor_x + w > end_x)
        {
            cursor_x = start_x;
            cursor_y += h + font->gap.h;

            if (cursor_y + h > end_y)
            {
                break;
            }
        }

        /* 绘制字符 */
        XHAL_TRY(ret, xdisp_draw_char(disp, cursor_x, cursor_y, cp, font));

        cursor_x += w;
    }

    if (read != NULL)
    {
        *read = (xhal_size_t)(p - str);
    }

    return ret;
}

xhal_err_t xdisp_printf(xdisp_t *disp, xdisp_font_t *font, const char *fmt, ...)
{
    char str_buff[XDISP_BUFF_SIZE];

    va_list args;
    va_start(args, fmt);
    int32_t count = vsnprintf(str_buff, sizeof(str_buff), fmt, args);
    va_end(args);

    if (count < 0)
    {
        return XHAL_ERR_INVALID;
    }

    if (count >= sizeof(str_buff))
    {
        return XHAL_ERR_NO_MEMORY;
    }

    return xdisp_draw_string(disp, NULL, font, str_buff);
}

xhal_err_t xdisp_begin(xdisp_t *disp)
{
    xassert_not_null(disp);

    xhal_err_t ret = XHAL_OK;

    if (disp->in_draw)
    {
        return XHAL_ERR_BUSY;
    }

    disp->in_draw = true;

#ifdef XHAL_OS_SUPPORTING
    if (disp->mutex)
    {
        osStatus_t ret_os = osMutexAcquire(disp->mutex, osWaitForever);
        if (ret_os != osOK)
        {
            return XHAL_ERROR;
        }
    }
#endif

    return ret;
}

xhal_err_t xdisp_end(xdisp_t *disp)
{
    xassert_not_null(disp);

    xhal_err_t ret = XHAL_OK;

    if (!disp->in_draw)
    {
        return XHAL_ERR_BUSY;
    }

    disp->in_draw = false;

#ifdef XHAL_OS_SUPPORTING
    if (disp->mutex)
    {
        osStatus_t ret_os = osMutexRelease(disp->mutex);
        if (ret_os != osOK)
        {
            return XHAL_ERROR;
        }
    }
#endif

    return ret;
}

xhal_err_t _map_to_mem(xdisp_t *disp, u16 x, u16 y, u16 *mx, u16 *my)
{
    uint16_t tx, ty;

    switch (disp->rotation)
    {
    case XDISP_ROT_0:
        tx = x;
        ty = y;
        break;

    case XDISP_ROT_90:
        tx = disp->panel_width - 1 - y;
        ty = x;
        break;

    case XDISP_ROT_180:
        tx = disp->panel_width - 1 - x;
        ty = disp->panel_height - 1 - y;
        break;

    case XDISP_ROT_270:
        tx = y;
        ty = disp->panel_height - 1 - x;
        break;

    default:
        return XHAL_ERR_NOT_SUPPORT;
    }

    tx += disp->offset_x;
    ty += disp->offset_y;

    *mx = tx;
    *my = ty;

    return XHAL_OK;
}

uint32_t inline _pow(u16 m, u16 n)
{
    uint32_t result = 1;
    while (n--)
    {
        result *= m;
    }
    return result;
}
