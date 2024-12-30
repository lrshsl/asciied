#ifndef CE_LOG_H
#define CE_LOG_H

#include "header.h"

/**
 * Log levels.
 * Will be extended further and order might change
 */
enum LogLevel {
	LOG_NONE,

	LOG_ERR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_TRACE,

	LOG_ALL,
};

extern enum LogLevel loglvl;

/**
 * Log a message with a given log level.
 *
 * @param lvl Log level at which the message should be logged
 * @param fmt Format string (same as for printf)
 * @param ... Arguments for the format string
 *
 * @todo Use preprocessor, add file and line number
 */
fn log_add(enum LogLevel lvl, char *fmt, ...);

#endif
