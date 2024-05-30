#include "log_.h"

#if LOG_ENABLED == 1

#    include <inttypes.h>
#    include <stdarg.h>
#    include <stddef.h>
#    include <stdint.h>
#    include <stdio.h>
#    if LOG_TIMESTAMP_FORMAT > 0U
#        include <time.h>
#    endif

/* -------------------------------------------------------------------------- */

#    if LOG_ENABLED_COLOR == 1
#        define _TIMESTAMP_COLOR "\033[0;97m"
#    else
#        define _TIMESTAMP_COLOR ""
#    endif  // LOG_ENABLED_COLOR == 1

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
static inline void _print_uptime(void);

#        if LOG_TIMESTAMP_FORMAT > 0U
static inline void _print_date_time(void);
#        endif  // LOG_TIMESTAMP_FORMAT > 0U
#    endif      // LOG_TIMESTAMP_ENABLED == 1

/* -------------------------------------------------------------------------- */

log_result_t log_init(const log_mask_t level_mask, log_io_t const *io) {

    if ((io == NULL) || (io->write == NULL)) {
        return LOGGER_RESULT_ERROR;
    }

#    if LOG_TIMESTAMP_ENABLED == 1U
    if (io->get_uptime_ms == NULL) {
        return LOGGER_RESULT_ERROR;
    }
#    endif  // LOG_TIMESTAMP_ENABLED == 1U

#    if LOG_TIMESTAMP_FORMAT > 0U
    if (io->get_utc_time_s == NULL) {
        return LOGGER_RESULT_ERROR;
    }
#    endif  // LOG_TIMESTAMP_FORMAT > 0U

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

static void _message(const log_mask_t level_mask, const char *format, va_list args, bool need_to_use_format) {
#    if (LOG_TIMESTAMP_ENABLED == 0) && !defined(LOG_ENDLINE)
    (void)need_to_use_format;
#    endif  // LOG_TIMESTAMP_ENABLED == 0
    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    /* to protect _ctx.buff */
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
    if (need_to_use_format == true) {
#        if LOG_TIMESTAMP_FORMAT > 0U
        _print_date_time();
#        endif  // LOG_TIMESTAMP_FORMAT > 0
        _print_uptime();
    }
#    endif  // LOG_TIMESTAMP_ENABLED == 1

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

#    if defined(LOG_ENDLINE)
    if (need_to_use_format == true) {
        _log_to((uint8_t *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
    }
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

void log_it(const log_mask_t level_mask, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _message(level_mask, format, args, true);
    va_end(args);
}

/* -------------------------------------------------------------------------- */

void log_raw(const log_mask_t level_mask, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _message(level_mask, format, args, false);
    va_end(args);
}
/* -------------------------------------------------------------------------- */

void log_array(const log_mask_t level_mask, const char *message, const void *data, size_t size) {

    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

    uint8_t const *array = (const uint8_t *)data;

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
#        if LOG_TIMESTAMP_FORMAT > 0U
    _print_date_time();
#        endif  // LOG_TIMESTAMP_FORMAT > 0
    _print_uptime();
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
        _ctx.io->write((uint8_t *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
        _ctx.io->write(_ctx.queue, _ctx.queue_index);
        _ctx.queue_index = 0;
        _ctx.io->write((uint8_t *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
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

static inline void _print_uptime(void) {
#        if LOG_TIMESTAMP_64BIT == 0
#            define _FORMAT  "[%04" PRIi32 ".%03" PRIu32 "] "
#            define _DIVIDER (1000U)
#        else
#            define _FORMAT  "[%04" PRIi64 ".%03" PRIu32 "] "
#            define _DIVIDER (1000ULL)
#        endif  // LOG_TIMESTAMP_64BIT == 1

#        if LOG_TIMESTAMP_FORMAT > 0U
    static const char TS_TEMPLATE[] = _FORMAT;
#        else
    static const char TS_TEMPLATE[] = _TIMESTAMP_COLOR _FORMAT;
#        endif  // LOG_TIMESTAMP_FORMAT > 0U

    log_timestamp_t ts = _ctx.io->get_uptime_ms();

    int strlen = snprintf(_ctx.buff, sizeof(_ctx.buff), TS_TEMPLATE, (ts / _DIVIDER), (uint32_t)(ts % 1000UL));
    if (strlen >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
    } else {
        _log_to(snprintf_error, sizeof(snprintf_error) - 1);
    }
}

/* -------------------------------------------------------------------------- */

#        if LOG_TIMESTAMP_FORMAT > 0U
static inline void _print_date_time(void) {
    time_t utc_time = _ctx.io->get_utc_time_s();
    struct tm tm_buffer;
#            if defined(_MSC_VER)
    gmtime_s(&tm_buffer, &utc_time);
#            else
    gmtime_r(&utc_time, &tm_buffer);
#            endif

#            if LOG_TIMESTAMP_FORMAT == 1U
    int strlen = snprintf(_ctx.buff,
                          sizeof(_ctx.buff),
                          _TIMESTAMP_COLOR "[%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "] ",
                          (uint32_t)tm_buffer.tm_hour,
                          (uint32_t)tm_buffer.tm_min,
                          (uint32_t)tm_buffer.tm_sec);
#            else
    int strlen = snprintf(_ctx.buff,
                          sizeof(_ctx.buff),
                          _TIMESTAMP_COLOR "[%02" PRIu32 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32
                                           ":%02" PRIu32 "] ",
                          (uint32_t)(tm_buffer.tm_year + 1900),
                          (uint32_t)(tm_buffer.tm_mon + 1),
                          (uint32_t)tm_buffer.tm_mday,
                          (uint32_t)tm_buffer.tm_hour,
                          (uint32_t)tm_buffer.tm_min,
                          (uint32_t)tm_buffer.tm_sec);
#            endif  // LOG_TIMESTAMP_FORMAT == 1U
    if (strlen >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)strlen);
    } else {
        _log_to(snprintf_error, sizeof(snprintf_error) - 1);
    }
}
#        endif  // LOG_TIMESTAMP_FORMAT > 0U
#    endif      // LOG_TIMESTAMP_ENABLED == 1

#endif  // #if LOG_ENABLED==1
