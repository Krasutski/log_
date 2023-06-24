#ifndef __LOG_H__
#define __LOG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <log_conf.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(LOG_MAX_MESSAGE_LENGTH)
#    define LOG_MAX_MESSAGE_LENGTH (128U)
#endif  // LOG_MAX_MESSAGE_LENGTH

#if !defined(LOG_TIMESTAMP_64BIT)
#    define LOG_TIMESTAMP_64BIT (0U)
#endif  // LOG_ENDLINE

#if !defined(LOG_TIMESTAMP_ENABLED)
#    define LOG_TIMESTAMP_ENABLED (1U)
#endif  // LOG_TIMESTAMP_ENABLED

#if !defined(LOG_THREADSAFE_ENABLED)
#    define LOG_THREADSAFE_ENABLED (0U)
#endif

#if !defined(LOG_ENABLED_COLOR)
#    define LOG_ENABLED_COLOR (0U)
#endif  // LOG_ENABLED_COLOR

#if !defined(LOG_ISR_QUEUE)
#    define LOG_ISR_QUEUE (0U)
#endif  // LOG_ISR_QUEUE

/* ===== TYPEDEFS =========================================================== */

typedef enum log_mask_e {
    LOG_MASK_OFF = 0x00,
    LOG_MASK_INFO = 0x01,
    LOG_MASK_WARNING = 0x02,
    LOG_MASK_ERROR = 0x04,
    LOG_MASK_DEBUG = 0x08,
    LOG_MASK_USER1 = 0x10,
    LOG_MASK_USER2 = 0x20,
    LOG_MASK_USER3 = 0x40,
    LOG_MASK_USER4 = 0x80,
    LOG_MASK_ALL = 0xFF,
} log_mask_t;

typedef enum {
    LOGGER_RESULT_OK,
    LOGGER_RESULT_ERROR,
} log_result_t;

#if LOG_TIMESTAMP_64BIT == (0U)
typedef uint32_t log_timestamp_t;
#else
typedef uint64_t log_timestamp_t;
#endif

/* ===== LOG MACROS ========================================================= */

#if LOG_ENABLED == 1U
#    define LOG(...)       log_it(__VA_ARGS__)
#    define LOG_ARRAY(...) log_array(__VA_ARGS__)
#else /* LOG_ENABLED == 1 */
#    define LOG(...)                                            \
        do {                                                    \
            /* emtpy macro to avoid static analyzer warnings */ \
        } while (0)
#    define LOG_ARRAY(...)                                      \
        do {                                                    \
            /* emtpy macro to avoid static analyzer warnings */ \
        } while (0)
#endif /* LOG_ENABLED == 1 */

#if LOG_ENABLED_COLOR == 1
#    define LOG_COLOR_RED     "91"
#    define LOG_COLOR_GREEN   "92"
#    define LOG_COLOR_YELLOW  "93"
#    define LOG_COLOR_BLUE    "94"
#    define LOG_COLOR_PURPLE  "95"
#    define LOG_COLOR_CYAN    "96"
#    define LOG_COLOR_WHITE   "97"
#    define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#    define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#    define LOG_RESET_COLOR() "\033[0m"
#else
#    define LOG_COLOR(COLOR) ""
#    define LOG_BOLD(COLOR)  ""
#endif /* LOG_ENABLED_COLOR == 1 */

#define LOG_DEBUG(...)   LOG(LOG_MASK_DEBUG, LOG_COLOR(LOG_COLOR_WHITE) __VA_ARGS__)
#define LOG_INFO(...)    LOG(LOG_MASK_INFO, LOG_COLOR(LOG_COLOR_GREEN) __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_MASK_WARNING, LOG_COLOR(LOG_COLOR_YELLOW) __VA_ARGS__)
#define LOG_ERROR(...)   LOG(LOG_MASK_ERROR, LOG_COLOR(LOG_COLOR_RED) __VA_ARGS__)

#define LOG_DEBUG_ARRAY(...)       LOG_ARRAY(LOG_MASK_DEBUG, LOG_COLOR(LOG_COLOR_WHITE) __VA_ARGS__)
#define LOG_DEBUG_ARRAY_RED(...)   LOG_ARRAY(LOG_MASK_DEBUG, LOG_COLOR(LOG_COLOR_RED) __VA_ARGS__)
#define LOG_DEBUG_ARRAY_GREEN(...) LOG_ARRAY(LOG_MASK_DEBUG, LOG_COLOR(LOG_COLOR_GREEN) __VA_ARGS__)
#define LOG_DEBUG_ARRAY_BLUE(...)  LOG_ARRAY(LOG_MASK_DEBUG, LOG_COLOR(LOG_COLOR_BLUE) __VA_ARGS__)

#if LOG_ENABLED == 1U
typedef struct {
    void (*write)(const uint8_t *data, size_t size);
#    if LOG_THREADSAFE_ENABLED == 1U
    void (*lock)(void);
    void (*unlock)(void);
#    endif  // LOG_THREADSAFE_ENABLED == 1U
#    if LOG_TIMESTAMP_ENABLED == 1U
    log_timestamp_t (*get_ts)(void);
#    endif  // LOG_TIMESTAMP_ENABLED == 1U
#    if LOG_ISR_QUEUE == 1U
    bool (*is_isr)(void);
#    endif  // LOG_ISR_QUEUE == 1U
} log_io_t;

log_result_t log_init(const log_mask_t level, log_io_t const *io);

#    if defined(__GNUC__)
/* Enable format checking by GCC compiler */
#        define __PRINTF_FORMAT __attribute__((format(printf, 2, 3)))
#    else
#        define __PRINTF_FORMAT
#    endif

/* Do not use it in your code, better use defines like LOG_INFO or LOG_DEBUG_ARRAY */
void log_it(const log_mask_t level, const char *format, ...) __PRINTF_FORMAT;
void log_array(const log_mask_t level, const char *message, const void *array, size_t size);

#    if LOG_ISR_QUEUE == 1U
void log_flush_isr_queue(void);
#    endif  // LOG_ISR_QUEUE == 1U

#endif  // LOG_ENABLED==1U

#ifdef __cplusplus
}
#endif

#endif /*__LOG_H__*/
