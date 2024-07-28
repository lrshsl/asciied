#include "log.h"
#include "constants.h"
#include <stdarg.h>
#include <stdio.h>

fn log_add(enum LogLevel lvl, char *fmt, ...) {
  if (loglvl < lvl) {
    return;
  }

  FILE *logfile = fopen(LOG_FILE_NAME, "a");
  va_list args;

  /* Print prefix */
  char *prefix = "";
  switch (lvl) {
  case log_none:
    break;
  case log_err:
    prefix = "ERR  : ";
    break;
  case log_warn:
    prefix = "WARN : ";
    break;
  case log_info:
    prefix = "INFO : ";
    break;
  case log_debug:
    prefix = "DEBUG: ";
    break;
  case log_trace:
    prefix = "TRACE: ";
    break;
  case log_all:
    prefix = "LOG  : ";
    break;
  }

  fprintf(logfile, "%s", prefix);

  /* Print message */
  va_start(args, fmt);
  vfprintf(logfile, fmt, args);
  va_end(args);
  fclose(logfile);
}

