#ifndef CE_CONFIG_H
#define CE_CONFIG_H

#include "colors.h"

#include <ncurses.h>

/* Where to save log and buffer dump */
#define LOG_FILE_NAME "log/logfile"
#define BUFFER_DUMP_FILE "log/buffer_dump"

/* Do tests */
#define TESTS

#define DRAW_AREA_MIN_X (0)
#define DRAW_AREA_MAX_X (COLS - 2)
#define DRAW_AREA_MIN_Y (1)
#define DRAW_AREA_MAX_Y (LINES - 3)
#define DRAW_AREA_WIDTH (COLS - 1)
#define DRAW_AREA_HEIGHT (LINES - 2)

// Length: excluding NULL terminator
#define COLOR_INDICATOR_LEN 3
#define COLOR_INDICATOR_RIGHT_OFFSET 0
#define COLOR_INDICATOR_STRING SPACES_100

// Length: excluding NULL terminator
#define MODE_INDICATOR_LEN 10

#define NOTIFY_AREA_Y (LINES - 2)
#define NOTIFY_AREA_X (0)
#define NOTIFY_AREA_WIDTH (COLS / 2 - MODE_INDICATOR_LEN)

#define CURSOR_INVISIBLE 0
#define CURSOR_VISIBLE 1

#define SAVE_DIR "./saves"
#define SAVE_DIR_LEN 7

#define FILE_EXTENSION ".centry"
#define FILE_EXTENSION_LEN 7

// clang-format off
#define UI_BG_ATTRS			(COLOR_PAIR(DefaultCollection_GRAY) | A_REVERSE)
#define UI_MODE_INDICATOR_ATTRS (COLOR_PAIR(DefaultCollection_WHITE))
#define CMD_LINE_ATTRS		(COLOR_PAIR(DefaultCollection_WHITE))
#define NOTIFY_ATTRS			(COLOR_PAIR(DefaultCollection_GRAY) | A_REVERSE)
// clang-format off

#endif
