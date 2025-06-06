#pragma once

/*
    Set and line for all logs, you can remove it if no need
*/
#define LOG_ENDLINE "\r\n"

/*
    Use timestamp
*/
#define LOG_TIMESTAMP_ENABLED (1U)

/*
    Timestamp format:
        0 - Only uptime
        1 - Time and uptime
        2 - Date, Tine and uptime
*/
#define LOG_TIMESTAMP_FORMAT (0U)

/*
    Thread safe mode, enable it if use RTOS
*/
#define LOG_THREADSAFE_ENABLED (0U)

/*
    Allow use timestamp as 64bit variable
*/
#define LOG_TIMESTAMP_64BIT (0U)

/*
    Use queue in interrupts, message will printed during with
    next log or by calling log_flush_isr_queue()
*/
#define LOG_ISR_QUEUE (0U)

/*
   Internal buffer size
*/
#define LOG_MAX_MESSAGE_LENGTH (128U)

/*
   Internal buffer size
*/
#define LOG_MAX_MESSAGE_LENGTH (128U)

/*
   Use colors
*/
#define LOG_ENABLED_COLOR (1U)
