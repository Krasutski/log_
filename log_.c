#include "log_.h"

#if LOG_ENABLED == 1

#    include <inttypes.h>
#    include <stdarg.h>
#    include <stddef.h>
#    include <stdint.h>
#    include <stdio.h>

/* -------------------------------------------------------------------------- */

static struct {
    char buff[LOG_MAX_MESSAGE_LENGTH];
    log_mask_t mask;
    log_io_t const *io;
#    if LOG_ISR_QUEUE == 1U
    uint8_t queue[LOG_MAX_MESSAGE_LENGTH * 2U];
    size_t queue_index;
#    endif  // LOG_ISR_QUEUE == 1U
} _ctx = {
    .mask = LOG_MASK_OFF,
    .io = NULL,
#    if LOG_ISR_QUEUE == 1U
    .queue_index = 0,
#    endif  // LOG_ISR_QUEUE == 1U
};

/* -------------------------------------------------------------------------- */

static const uint8_t snprintf_error[] = "\r\nsnprintf - internal error\r\n";

/* -------------------------------------------------------------------------- */

static void _log_to(uint8_t const *data, size_t size);
#    if LOG_TIMESTAMP_ENABLED == 1
static inline void _print_ts(void);
#    endif  // LOG_TIMESTAMP_ENABLED == 1

/* -------------------------------------------------------------------------- */

log_result_t log_init(const log_mask_t level_mask, log_io_t const *io) {

    if ((io == NULL) || (io->write == NULL)) {
        return LOGGER_RESULT_ERROR;
    }

#    if LOG_TIMESTAMP_ENABLED == 1U
    if (io->get_ts == NULL) {
        return LOGGER_RESULT_ERROR;
    }
#    endif  // LOG_TIMESTAMP_ENABLED == 1U

#    if LOG_THREADSAFE_ENABLED == 1U
    if ((io->lock == NULL) || (io->unlock == NULL)) {
        return LOGGER_RESULT_ERROR;
    }
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_ISR_QUEUE == 1U
    if ((io->is_isr == NULL)) {
        return LOGGER_RESULT_ERROR;
    }
#    endif  // LOG_ISR_QUEUE == 1U

    _ctx.mask = level_mask;
    _ctx.io = io;
#    if LOG_ISR_QUEUE == 1U
    _ctx.queue_index = 0;
#    endif  // LOG_ISR_QUEUE == 1U

    return LOGGER_RESULT_OK;
}

/* -------------------------------------------------------------------------- */

void log_it(const log_mask_t level_mask, const char *format, ...) {

    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    /* to protect _ctx.buff */
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
    _print_ts();
#    endif  // LOG_TIMESTAMP_ENABLED == 1

    va_list args;
    va_start(args, format);

    bool is_truncated = false;
    int strlen = vsnprintf(_ctx.buff, LOG_MAX_MESSAGE_LENGTH, format, args);
    if (strlen >= (int)LOG_MAX_MESSAGE_LENGTH) {
        strlen = LOG_MAX_MESSAGE_LENGTH;
        is_truncated = true;
    }

    if (strlen >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
    } else {
        _log_to(snprintf_error, sizeof(snprintf_error) - 1);
    }

    va_end(args);
#    if defined(LOG_ENDLINE)
    _log_to((uint8_t *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
#    endif

    if (is_truncated) {
        uint8_t const msg[] =
            LOG_COLOR(LOG_COLOR_RED) "Message was truncated" LOG_ENDLINE "Increase LOG_MAX_MESSAGE_LENGTH" LOG_ENDLINE;
        _log_to(msg, sizeof(msg) - 1);
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->unlock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U
}

/* -------------------------------------------------------------------------- */

void log_array(const log_mask_t level_mask, const char *message, const void *data, size_t size) {

    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

    uint8_t const *array = data;

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
    _print_ts();
#    endif  // LOG_TIMESTAMP_ENABLED == 1

    int strlen = snprintf(_ctx.buff, sizeof(_ctx.buff), "%s[%u]:", message, size);
    if (strlen >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
    } else {
        _log_to(snprintf_error, sizeof(snprintf_error) - 1);
    }

    for (uint32_t i = 0; i < size; i++) {
        strlen = snprintf(_ctx.buff, sizeof(_ctx.buff), " %02X", array[i]);
        if (strlen >= 0) {
            _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
        } else {
            _log_to(snprintf_error, sizeof(snprintf_error) - 1);
        }
    }

    _log_to((uint8_t *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->unlock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U
}

/* -------------------------------------------------------------------------- */

#    if LOG_ISR_QUEUE == 1U

void log_flush_isr_queue(void) {
    if (_ctx.queue_index > 0) {
        _ctx.io->write(_ctx.queue, _ctx.queue_index);
        _ctx.queue_index = 0;
    }
}

#    endif  // LOG_ISR_QUEUE == 1U

/* -------------------------------------------------------------------------- */

static inline void _log_to(uint8_t const *data, size_t size) {

#    if LOG_ISR_QUEUE == 1U

    if (_ctx.io->is_isr()) {
        size_t queue_count = _ctx.queue_index;
        for (size_t i = 0; i < size; i++) {
            size_t index = queue_count + i;
            if (index >= sizeof(_ctx.queue)) {
                return;
            }
            _ctx.queue[index] = data[i];
            _ctx.queue_index++;
        }

        return;
    }

    log_flush_isr_queue();
#    endif  // LOG_ISR_QUEUE == 1U

    _ctx.io->write(data, size);
}

/* -------------------------------------------------------------------------- */

#    if LOG_TIMESTAMP_ENABLED == 1
static inline void _print_ts(void) {

#        if LOG_ENABLED_COLOR == 1
#            define _COLOR "\033[0;97m"
#        else
#            define _COLOR ""
#        endif  // LOG_ENABLED_COLOR == 1
#        if LOG_TIMESTAMP_64BIT == 0
#            define _FORMAT  "[%04" PRIi32 ".%03" PRIu32 "] "
#            define _DIVIDER (1000U)
#        else
#            define _FORMAT  "[%04" PRIi64 ".%03" PRIu32 "] "
#            define _DIVIDER (1000ULL)
#        endif  // LOG_TIMESTAMP_64BIT == 1
    static const char TS_TEMPLATE[] = _COLOR _FORMAT;

    log_timestamp_t ts = _ctx.io->get_ts();

    int strlen = snprintf(_ctx.buff, sizeof(_ctx.buff), TS_TEMPLATE, (ts / _DIVIDER), (uint32_t)(ts % 1000UL));
    if (strlen >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
    } else {
        _log_to(snprintf_error, sizeof(snprintf_error) - 1);
    }
}
#    endif  // LOG_TIMESTAMP_ENABLED == 1

#endif  // #if LOG_ENABLED==1
