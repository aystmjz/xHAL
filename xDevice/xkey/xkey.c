#include "xkey.h"
#include "xhal_assert.h"
#include "xhal_bit.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include <string.h>

XLOG_TAG("xKEY");

#define XKEY_MAX_EVENT_BITS 32

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xkey_mutex_attr = {
    .name      = "xkey_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xkey_manager_t *mgr);
static inline void _unlock(xkey_manager_t *mgr);
static xkey_type_t _parse_bits(xkey_manager_t *mgr, xkey_t *key);

xhal_err_t xkey_manager_init(xkey_manager_t *mgr, const xkey_config_t *config,
                             void *event_buf, uint32_t event_bufsz)
{
    xassert_not_null(mgr);
    xassert_not_null(config);
    xassert_not_null(event_buf);

    if (event_bufsz < sizeof(xkey_event_t))
    {
        return XHAL_ERR_INVALID;
    }

    xlist_init(&mgr->key_list);
    xrbuf_init(&mgr->evt_rb, event_buf, event_bufsz);
    mgr->last_scan_tick = 0;
    mgr->config         = *config;

#ifdef XHAL_OS_SUPPORTING
    mgr->mutex = osMutexNew(&xkey_mutex_attr);
    xassert_not_null(mgr->mutex);
#endif

    return XHAL_OK;
}

xhal_err_t xkey_manager_deinit(xkey_manager_t *mgr)
{
    xassert_not_null(mgr);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    xlist_init(&mgr->key_list);
    xrbuf_free(&mgr->evt_rb);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osMutexDelete(mgr->mutex);
    if (ret_os != osOK)
    {
        return XHAL_ERROR;
    }
    mgr->mutex = NULL;
#endif
    return XHAL_OK;
}

xhal_err_t xkey_register(xkey_manager_t *mgr, xkey_t *key, const char *name,
                         uint32_t id, xkey_state_cb_t state_cb,
                         xkey_action_cb_t action_cb)
{
    xassert_not_null(mgr);
    xassert_not_null(key);
    xassert_not_null(state_cb);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    xmemset(key, 0, sizeof(*key));

    key->name       = name;
    key->id         = id;
    key->action_cb  = action_cb;
    key->state_cb   = state_cb;
    key->last_state = (uint8_t)XKEY_STATE_RELEASED;
    key->curr_state = (uint8_t)XKEY_STATE_RELEASED;

    _lock(mgr);
    xlist_add_tail(&key->list, &mgr->key_list);
    _unlock(mgr);

    return XHAL_OK;
}

xhal_err_t xkey_unregister(xkey_manager_t *mgr, xkey_t *key)
{
    xassert_not_null(mgr);
    xassert_not_null(key);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_ERR_NOT_FOUND;

    _lock(mgr);
    xkey_t *iter;
    xlist_for_each_entry(iter, &mgr->key_list, xkey_t, list)
    {
        if (iter == key)
        {
            xlist_del(&key->list);
            ret = XHAL_OK;
            break;
        }
    }
    _unlock(mgr);

    return ret;
}

xhal_err_t xkey_set_action_cb(xkey_t *key, xkey_action_cb_t cb)
{
    xassert_not_null(key);

    key->action_cb = cb;

    return XHAL_OK;
}

xhal_err_t xkey_poll(xkey_manager_t *mgr, xhal_tick_t now_tick)
{
    xassert_not_null(mgr);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(mgr);
    /* 未到扫描周期，直接退出 */
    if (TIME_DIFF(now_tick, mgr->last_scan_tick) <
        mgr->config.scan_interval_tick)
    {
        goto exit;
    }

    mgr->last_scan_tick = now_tick;

    xkey_t *key;
    xlist_for_each_entry(key, &mgr->key_list, xkey_t, list)
    {
        /* 记录上一次状态 */
        key->last_state = key->curr_state;

        /* 读取当前按键状态 */
        xhal_err_t ret = key->state_cb(key, &key->curr_state);
        if (ret != XHAL_OK)
        {
            continue;
        }

        /* 状态发生变化，触发 action 回调 */
        if (key->action_cb && key->curr_state != key->last_state)
        {
            key->action_cb(key->curr_state == (uint8_t)XKEY_STATE_PRESSED
                               ? XKEY_ACTION_PRESS
                               : XKEY_ACTION_RELEASE);
        }

        /* 检测一次新的事件序列开始（首次按下） */
        if (!key->event_active &&
            key->curr_state == (uint8_t)XKEY_STATE_PRESSED &&
            key->last_state == (uint8_t)XKEY_STATE_RELEASED)
        {
            key->event_active = 1;
            key->event_bits   = 1;
            key->event_index  = 0;
            key->start_tick   = now_tick;
            key->end_tick     = now_tick;
        }

        /* 当前没有处于事件采集中，跳过 */
        if (!key->event_active)
        {
            continue;
        }

        if (key->curr_state == XKEY_STATE_PRESSED)
        {
            key->end_tick = now_tick;
        }

        /* 采集按键状态位序列 */
        if (key->event_index < XKEY_MAX_EVENT_BITS)
        {
            BIT_SET(key->event_bits, key->event_index,
                    key->curr_state == XKEY_STATE_PRESSED);

            key->event_index++;
        }

        /* 事件超时，解析并上报事件 */
        if (TIME_DIFF(now_tick, key->end_tick) >=
            mgr->config.event_timeout_tick)
        {
            xkey_event_t event = {
                .key      = key,
                .type     = _parse_bits(mgr, key),
                .tick     = now_tick,
                .duration = TIME_DIFF(key->end_tick, key->start_tick),
                .raw_bits = key->event_bits,
            };

            if (xrbuf_get_free(&mgr->evt_rb) >= sizeof(event))
            {
                xrbuf_write(&mgr->evt_rb, &event, sizeof(event));
            }

            /* 重置事件状态 */
            key->event_active = 0;
            key->event_bits   = 0;
            key->event_index  = 0;
        }
    }
exit:
    _unlock(mgr);

    return XHAL_OK;
}

xhal_err_t xkey_get_state(xkey_t *key, xkey_state_t *state)
{
    xassert_not_null(key);
    xassert_not_null(state);

    xhal_err_t ret = XHAL_ERR_NO_INIT;

    if (key->state_cb)
    {
        ret = key->state_cb(key, state);
    }

    return ret;
}

xhal_err_t xkey_get_event(xkey_manager_t *mgr, xkey_event_t *evt)
{
    xassert_not_null(mgr);
    xassert_not_null(evt);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_OK;
    uint32_t read  = 0;

    _lock(mgr);
    if (!xrbuf_get_full(&mgr->evt_rb))
    {
        ret = XHAL_ERR_EMPTY;
        goto exit;
    }

    read = xrbuf_read(&mgr->evt_rb, evt, sizeof(*evt));
    if (read != sizeof(*evt))
    {
        ret = XHAL_ERROR;
        xrbuf_reset(&mgr->evt_rb);
        goto exit;
    }
exit:
    _unlock(mgr);

    return ret;
}

xhal_err_t xkey_clear_event(xkey_manager_t *mgr)
{
    xassert_not_null(mgr);

    if (!mgr->evt_rb.buff)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(mgr);
    xrbuf_reset(&mgr->evt_rb);
    _unlock(mgr);

    return XHAL_OK;
}

xkey_t *xkey_find_by_name(xkey_manager_t *mgr, const char *name)
{
    xassert_not_null(mgr);
    xassert_not_null(name);

    if (!mgr->evt_rb.buff)
    {
        return NULL;
    }

    xkey_t *ret = NULL, *key = NULL;

    _lock(mgr);
    xlist_for_each_entry(key, &mgr->key_list, xkey_t, list)
    {
        if (key->name && (strcmp(key->name, name) == 0))
        {
            ret = key;
            break;
        }
    }
    _unlock(mgr);

    return ret;
}

xkey_t *xkey_find_by_id(xkey_manager_t *mgr, uint32_t id)
{
    xassert_not_null(mgr);

    if (!mgr->evt_rb.buff)
    {
        return NULL;
    }

    xkey_t *ret = NULL, *key = NULL;

    _lock(mgr);
    xlist_for_each_entry(key, &mgr->key_list, xkey_t, list)
    {
        if (key->id == id)
        {
            ret = key;
            break;
        }
    }
    _unlock(mgr);

    return ret;
}

uint8_t xkey_event_get_press_count(const xkey_event_t *evt)
{
    xassert_not_null(evt);

    uint8_t high_count  = 0;
    uint8_t press_count = 0;

    for (uint8_t i = 0; i < XKEY_MAX_EVENT_BITS; i++)
    {
        bool bit = BIT_GET(evt->raw_bits, i);

        if (bit)
        {
            /* 处于按下段 */
            high_count++;
        }
        else
        {
            if (high_count > 0)
            {
                /* 一个完整的按下段结束 */
                press_count++;
                high_count = 0;
            }
        }
    }

    /* 最后一段如果以 PRESSED 结束，也算一次 */
    if (high_count > 0)
    {
        press_count++;
    }

    return press_count;
}

uint8_t xkey_bits_visual(const xkey_event_t *evt, char *buf, xhal_size_t size)
{
    xassert_not_null(evt);
    xassert_not_null(buf);

    if (size < 2)
    {
        if (size > 0)
        {
            buf[0] = '\0';
        }
        return 0;
    }

    uint8_t max_bits =
        ((size - 1) < XKEY_MAX_EVENT_BITS) ? (size - 1) : XKEY_MAX_EVENT_BITS;

    for (uint8_t i = 0; i < max_bits; i++)
    {
        buf[max_bits - i - 1] = BIT_GET(evt->raw_bits, i) ? '1' : '0';
    }

    buf[max_bits] = '\0';

    return max_bits;
}

const char *xkey_type_to_str(xkey_type_t type)
{
    switch (type)
    {
    case XKEY_TYPE_CLICK:
        return "CLICK";
    case XKEY_TYPE_DOUBLE_CLICK:
        return "DOUBLE";
    case XKEY_TYPE_LONG_PRESS:
        return "LONG";
    case XKEY_TYPE_MULTI_CLICK:
        return "MULTI";
    default:
        return "NONE";
    }
}

static inline void _lock(xkey_manager_t *mgr)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexAcquire(mgr->mutex, osWaitForever);
    xassert(ret == osOK);
#endif
}

static inline void _unlock(xkey_manager_t *mgr)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexRelease(mgr->mutex);
    xassert(ret == osOK);
#endif
}

static xkey_type_t _parse_bits(xkey_manager_t *mgr, xkey_t *key)
{
    uint8_t high_count        = 0;
    uint8_t press_count       = 0;
    uint8_t first_press_count = 0;

    for (uint8_t i = 0; i < XKEY_MAX_EVENT_BITS; i++)
    {
        bool bit = BIT_GET(key->event_bits, i);

        if (bit)
        {
            high_count++;
        }
        else
        {
            /* 记录第一段按下长度 */
            if (!first_press_count && high_count > 0)
            {
                first_press_count = high_count;
            }

            if (high_count > 0)
            {
                press_count++;
            }
            high_count = 0;
        }
    }

    if (high_count > 0)
    {
        /* 最后一段也算一次按下 */
        press_count++;

        /* 如果还没记录第一段，这就是第一段 */
        if (!first_press_count)
        {
            first_press_count = high_count;
        }
    }

    if (press_count == 1)
    {
        if (first_press_count >= mgr->config.long_press_count)
        {
            return XKEY_TYPE_LONG_PRESS;
        }
        else
        {
            return XKEY_TYPE_CLICK;
        }
    }
    else if (press_count == 2)
    {
        return XKEY_TYPE_DOUBLE_CLICK;
    }
    else if (press_count > 2)
    {
        return XKEY_TYPE_MULTI_CLICK;
    }

    return XKEY_TYPE_NONE;
}