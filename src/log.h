#ifndef LOG_H
#define LOG_H

#include "header.h"

/**
 * Log levels.
 * Will be extended further and order might change
 */
enum LogLevel {
  log_none,

  log_err,
  log_warn,
  log_info,
  log_debug,
  log_trace,

  log_all,
};

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

static enum LogLevel loglvl = log_all;

#endif
