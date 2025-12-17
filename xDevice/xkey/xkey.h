#ifndef __XKEY_H__
#define __XKEY_H__

#include "xhal_def.h"
#include "xhal_list.h"
#include "xhal_os.h"
#include "xhal_ringbuf.h"
#include "xhal_time.h"

typedef enum
{
    XKEY_STATE_RELEASED = 0, /* 抬起状态 */
    XKEY_STATE_PRESSED  = 1, /* 按下状态 */
} xkey_state_t;

typedef enum xkey_action
{
    XKEY_ACTION_RELEASE = 0, /* 抬起 */
    XKEY_ACTION_PRESS   = 1, /* 按下 */
} xkey_action_t;

typedef enum
{
    XKEY_TYPE_NONE = 0,
    XKEY_TYPE_CLICK,        /* 单击 */
    XKEY_TYPE_DOUBLE_CLICK, /* 双击 */
    XKEY_TYPE_LONG_PRESS,   /* 长按 */
    XKEY_TYPE_MULTI_CLICK,  /* 多次点击（>=3 次） */
} xkey_type_t;

typedef struct xkey xkey_t;

typedef void (*xkey_action_cb_t)(xkey_action_t action);
typedef xhal_err_t (*xkey_state_cb_t)(xkey_t *key, xkey_state_t *state);

typedef struct xkey
{
    const char *name;
    uint32_t id;
    xhal_list_t list;

    xkey_action_cb_t action_cb;
    xkey_state_cb_t state_cb;

    uint32_t start_tick;
    uint32_t end_tick;
    uint32_t event_bits;
    uint8_t event_active;
    uint8_t event_index;

    uint8_t last_state;
    uint8_t curr_state;
} xkey_t;

typedef struct
{
    xkey_t *key;
    xkey_type_t type;
    xhal_tick_t tick;
    uint32_t duration;
    uint32_t raw_bits;
} xkey_event_t;

typedef struct
{
    uint16_t event_timeout_tick;
    uint8_t scan_interval_tick;
    uint8_t long_press_count;
} xkey_config_t;

typedef struct
{
    xhal_list_t key_list;
    xrbuf_t evt_rb;
    xhal_tick_t last_scan_tick;
    xkey_config_t config;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xkey_manager_t;

xhal_err_t xkey_manager_init(xkey_manager_t *mgr, const xkey_config_t *config,
                             void *event_buf, uint32_t buf_size);
xhal_err_t xkey_manager_deinit(xkey_manager_t *mgr);
xhal_err_t xkey_register(xkey_manager_t *mgr, xkey_t *key, const char *name,
                         uint32_t id, xkey_state_cb_t state_cb,
                         xkey_action_cb_t action_cb);
xhal_err_t xkey_unregister(xkey_manager_t *mgr, xkey_t *key);

xhal_err_t xkey_set_action_cb(xkey_t *key, xkey_action_cb_t cb);
xhal_err_t xkey_get_state(xkey_t *key, xkey_state_t *state);
xhal_err_t xkey_get_event(xkey_manager_t *mgr, xkey_event_t *evt);
xhal_err_t xkey_clear_event(xkey_manager_t *mgr);

xkey_t *xkey_find_by_name(xkey_manager_t *mgr, const char *name);
xkey_t *xkey_find_by_id(xkey_manager_t *mgr, uint32_t id);
uint8_t xkey_event_get_press_count(const xkey_event_t *evt);
uint8_t xkey_bits_visual(const xkey_event_t *evt, char *buf, xhal_size_t size);
const char *xkey_type_to_str(xkey_type_t type);

xhal_err_t xkey_poll(xkey_manager_t *mgr, xhal_tick_t now_tick);

#endif /* __XKEY_H__ */
