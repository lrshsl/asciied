#include "include/log.h"
#include "include/config.h"

#include <stdarg.h>
#include <stdio.h>

enum LogLevel loglvl = LOG_ALL;

fn log_add(enum LogLevel lvl, char *fmt, ...) {
	if ( loglvl < lvl ) {
		return;
	}

	FILE *logfile = fopen(LOG_FILE_NAME, "a");
	if ( logfile == NULL ) {
		fprintf(stderr, "Error: Could not open logfile: %s\n", LOG_FILE_NAME);
		return;
	}
	va_list args;

	/* Print prefix */
	char *prefix = "";
	switch ( lvl ) {
	case LOG_NONE:
		break;
	case LOG_ERR:
		prefix = "ERR  : ";
		break;
	case LOG_WARN:
		prefix = "WARN : ";
		break;
	case LOG_INFO:
		prefix = "INFO : ";
		break;
	case LOG_DEBUG:
		prefix = "DEBUG: ";
		break;
	case LOG_TRACE:
		prefix = "DEBUG: ";
		break;
	case LOG_ALL:
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
