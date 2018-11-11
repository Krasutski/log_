#include "log_.h"

#if LOG_ENABLED==1

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* ===== LOCAL VARIABLES ==================================================== */

static char _log_buffer[LOG_MAX_MESSAGE_LENGTH] = {0};
static log_level_t _logger_level = LOG_LEVEL_OFF;
static log_io_t const *_log_io = NULL;

/* ===== LOCAL FUNCTIONS PROTOTYPES ========================================= */

static void _log_to(uint8_t* data, size_t size);
#if LOG_TIMESTAMP_ENABLED == 1
static inline void _print_ts(void);
#endif //LOG_TIMESTAMP_ENABLED == 1
/* ===== GLOBAL FUNCTIONS =================================================== */

log_result_t log_init(const log_level_t level, log_io_t const *io) {

    if( (io == NULL) || (io->write == NULL) ) {
        return LOGGER_RESULT_ERROR;
    }

#if LOG_TIMESTAMP_ENABLED == 1U
    if( (io->get_ts == NULL) ) {
        return LOGGER_RESULT_ERROR;
    }
#endif //LOG_TIMESTAMP_ENABLED == 1U

#if LOG_THREADSAFE_ENABLED == 1U
    if( (io->lock == NULL) || (io->unlock== NULL) ) {
        return LOGGER_RESULT_ERROR;
    }
#endif //LOG_THREADSAFE_ENABLED == 1U

    _logger_level = level;
    _log_io = io;

    return LOGGER_RESULT_OK;
}

void log_it(const log_level_t level, const char* format, ...) {

    if ((level & _logger_level) == 0) {
        return;
    }

#if LOG_THREADSAFE_ENABLED == 1U
    /* to protect _log_buffer */
    _log_io->lock();
#endif //LOG_THREADSAFE_ENABLED == 1U

#if LOG_TIMESTAMP_ENABLED == 1
    _print_ts();
#endif //LOG_TIMESTAMP_ENABLED == 1

    va_list args;
    va_start(args, format);

    int strlen = vsnprintf(_log_buffer, LOG_MAX_MESSAGE_LENGTH, format, args);

    _log_to((uint8_t*)_log_buffer, strlen);

    va_end(args);

#if LOG_THREADSAFE_ENABLED == 1U
    _log_io->unlock();
#endif //LOG_THREADSAFE_ENABLED == 1U
}

void log_array(const log_level_t level, char *message, const uint8_t* array, size_t size) {

    if (level < _logger_level) {
        return;
    }

#if LOG_THREADSAFE_ENABLED == 1U
    _log_io->lock();
#endif //LOG_THREADSAFE_ENABLED == 1U

#if LOG_TIMESTAMP_ENABLED == 1
    _print_ts();
#endif //LOG_TIMESTAMP_ENABLED == 1

    int strlen = snprintf(_log_buffer, sizeof(_log_buffer), "%s[%u]:", message, size);
    _log_to((uint8_t*)_log_buffer, strlen);

    for (uint32_t i = 0; i < size; i++) {
        int strlen = snprintf(_log_buffer, sizeof(_log_buffer), " 0x%02X", array[i]);
        _log_to((uint8_t*)_log_buffer, strlen);
    }

    _log_to((uint8_t*)LOG_ENDLINE, sizeof(LOG_ENDLINE) - 1);

#if LOG_THREADSAFE_ENABLED == 1U
    _log_io->unlock();
#endif //sLOG_THREADSAFE_ENABLED == 1U
}

/* ===== LOCAL FUNCTIONS ==================================================== */

static inline void _log_to(uint8_t* data, size_t size) {

    _log_io->write(data, size);
}

#if LOG_TIMESTAMP_ENABLED == 1
static inline void _print_ts(void) {

#if LOG_ENABLED_COLOR == 1
    static const char TS_TEMPLATE[] = "\033[0;97m[%04llu.%03lu] ";
#else
    static const char TS_TEMPLATE[] = "[%04llu.%03lu] ";
#endif //LOG_ENABLED_COLOR == 1

    uint64_t ts = _log_io->get_ts();

    int strlen = snprintf(_log_buffer, sizeof(_log_buffer),
        TS_TEMPLATE, ts / 1000ULL, ts % 1000UL);
    _log_to((uint8_t*)_log_buffer, strlen);
}
#endif //LOG_TIMESTAMP_ENABLED == 1

#endif //#if LOG_ENABLED==1
