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

typedef struct {
    char buff[LOG_MAX_MESSAGE_LENGTH];
    log_mask_t mask;
    log_io_t const *io;
#    if LOG_ISR_QUEUE == 1U
    uint8_t queue[LOG_ISR_MESSAGE_LENGTH];
    size_t queue_index;
#    endif  // LOG_ISR_QUEUE == 1U
} log_context_t;

static log_context_t _ctx = {
    .buff = { 0 },
    .mask = LOG_MASK_ALL,
    .io = NULL,
#    if LOG_ISR_QUEUE == 1U
    .queue = { 0 },
    .queue_index = 0,
#    endif  // LOG_ISR_QUEUE == 1U
};

/* -------------------------------------------------------------------------- */

static const uint8_t SNPRINTF_ERROR[] = "\r\nsnprintf - internal error\r\n";

/* -------------------------------------------------------------------------- */

static inline void _log_to(uint8_t const *data, size_t size);
static void _log_format(log_mask_t level_mask, const char *format, va_list args, bool add_formating);

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
    if (io->is_isr == NULL) {
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

static void _log_format(log_mask_t level_mask, const char *format, va_list args, bool add_formating) {
#    if (LOG_TIMESTAMP_ENABLED == 0) && !defined(LOG_ENDLINE)
    (void)add_formating;
#    endif  // LOG_TIMESTAMP_ENABLED == 0
    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
    if (add_formating == true) {
#        if LOG_TIMESTAMP_FORMAT > 0U
        _print_date_time();
#        endif  // LOG_TIMESTAMP_FORMAT > 0
        _print_uptime();
    }
#    endif  // LOG_TIMESTAMP_ENABLED == 1

    bool is_truncated = false;
    int len = vsnprintf(_ctx.buff, LOG_MAX_MESSAGE_LENGTH, format, args);
    if (len >= (int)LOG_MAX_MESSAGE_LENGTH) {
        len = LOG_MAX_MESSAGE_LENGTH;
        is_truncated = true;
    }
    if (len >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)len);
    } else {
        _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
    }
#    if defined(LOG_ENDLINE)
    if (add_formating == true) {
        _log_to((uint8_t const *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
    }
#    endif  // LOG_ENDLINE
    if (is_truncated == true) {
        static const uint8_t TRUNC_MESSAGE[] =
            LOG_COLOR(LOG_COLOR_RED) "Message was truncated" LOG_ENDLINE "Increase LOG_MAX_MESSAGE_LENGTH" LOG_ENDLINE;
        _log_to(TRUNC_MESSAGE, sizeof(TRUNC_MESSAGE) - 1);
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->unlock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U
}

/* -------------------------------------------------------------------------- */

void log_it(log_mask_t level_mask, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log_format(level_mask, format, args, true);
    va_end(args);
}

/* -------------------------------------------------------------------------- */

void log_raw(const log_mask_t level_mask, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log_format(level_mask, format, args, false);
    va_end(args);
}

/* -------------------------------------------------------------------------- */

void log_array(log_mask_t level_mask, const char *message, const void *data, size_t size) {
    if ((level_mask & _ctx.mask) == 0) {
        return;
    }
    uint8_t const *array = (uint8_t const *)data;
#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
#        if LOG_TIMESTAMP_FORMAT > 0U
    _print_date_time();
#        endif  // LOG_TIMESTAMP_FORMAT > 0
    _print_uptime();
#    endif  // LOG_TIMESTAMP_ENABLED == 1
    int len = snprintf(_ctx.buff, sizeof(_ctx.buff), "%s[%zu]:", message, size);
    if (len >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)len);
    } else {
        _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
    }

    for (size_t i = 0; i < size; i++) {
        len = snprintf(_ctx.buff, sizeof(_ctx.buff), " %02X", array[i]);
        if (len >= 0) {
            _log_to((uint8_t *)_ctx.buff, (size_t)len);
        } else {
            _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
        }
    }
    _log_to((uint8_t const *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->unlock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U
}

/* -------------------------------------------------------------------------- */

void log_array_float(log_mask_t level_mask, const char *message, const float *array, size_t size) {
    if ((level_mask & _ctx.mask) == 0) {
        return;
    }

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->lock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U

#    if LOG_TIMESTAMP_ENABLED == 1
#        if LOG_TIMESTAMP_FORMAT > 0U
    _print_date_time();
#        endif  // LOG_TIMESTAMP_FORMAT > 0
    _print_uptime();
#    endif  // LOG_TIMESTAMP_ENABLED == 1
    int len = snprintf(_ctx.buff, sizeof(_ctx.buff), "%s[%zu]:", message, size);
    if (len >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)len);
    } else {
        _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
    }

    for (size_t i = 0; i < size; i++) {
        len = snprintf(_ctx.buff, sizeof(_ctx.buff), " %.2f", array[i]);
        if (len >= 0) {
            _log_to((uint8_t *)_ctx.buff, (size_t)len);
        } else {
            _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
        }
    }
    _log_to((uint8_t const *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);

#    if LOG_THREADSAFE_ENABLED == 1U
    _ctx.io->unlock();
#    endif  // LOG_THREADSAFE_ENABLED == 1U
}

/* -------------------------------------------------------------------------- */

#    if LOG_ISR_QUEUE == 1U

void log_flush_isr_queue(void) {
    if (_ctx.queue_index > 0) {
        _ctx.io->write((uint8_t const *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
        _ctx.io->write(_ctx.queue, _ctx.queue_index);
        _ctx.queue_index = 0;
        _ctx.io->write((uint8_t const *)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);
    }
}

#    endif  // LOG_ISR_QUEUE == 1U

/* -------------------------------------------------------------------------- */

static inline void _log_to(uint8_t const *data, size_t size) {
#    if LOG_ISR_QUEUE == 1U
    if (_ctx.io->is_isr()) {
        for (size_t i = 0; i < size; i++) {
            if (_ctx.queue_index >= sizeof(_ctx.queue)) {
                return;
            }
            _ctx.queue[_ctx.queue_index] = data[i];
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
#            define _FORMAT "[%04" PRIi32 ".%03" PRIu32 "] "
#        else
#            define _FORMAT "[%04" PRIi64 ".%03" PRIu32 "] "
#        endif  // LOG_TIMESTAMP_64BIT == 1

    static const log_timestamp_t _DIVIDER = 1000UL;
    log_timestamp_t ts = _ctx.io->get_uptime_ms();

    log_timestamp_t sec = (ts / _DIVIDER);
    uint32_t msec = (uint32_t)(ts % _DIVIDER);
    int len = snprintf(_ctx.buff, sizeof(_ctx.buff), _TIMESTAMP_COLOR _FORMAT, sec, msec);
    if (len >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)len);
    } else {
        _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
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
    int len = snprintf(_ctx.buff,
                       sizeof(_ctx.buff),
                       _TIMESTAMP_COLOR "[%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "] ",
                       (uint32_t)tm_buffer.tm_hour,
                       (uint32_t)tm_buffer.tm_min,
                       (uint32_t)tm_buffer.tm_sec);
#            else
    int len = snprintf(_ctx.buff,
                       sizeof(_ctx.buff),
                       _TIMESTAMP_COLOR "[%04" PRIu32 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32
                                        ":%02" PRIu32 "] ",
                       (uint32_t)(tm_buffer.tm_year + 1900),
                       (uint32_t)(tm_buffer.tm_mon + 1),
                       (uint32_t)tm_buffer.tm_mday,
                       (uint32_t)tm_buffer.tm_hour,
                       (uint32_t)tm_buffer.tm_min,
                       (uint32_t)tm_buffer.tm_sec);
#            endif  // LOG_TIMESTAMP_FORMAT == 1U
    if (len >= 0) {
        _log_to((uint8_t *)_ctx.buff, (size_t)len);
    } else {
        _log_to(SNPRINTF_ERROR, sizeof(SNPRINTF_ERROR) - 1);
    }
}
#        endif  // LOG_TIMESTAMP_FORMAT > 0U
#    endif      // LOG_TIMESTAMP_ENABLED == 1

#endif  // #if LOG_ENABLED==1
