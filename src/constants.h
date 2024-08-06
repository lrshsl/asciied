#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "header.h"

/* Where to save log and buffer dump */
#define LOG_FILE_NAME "log/logfile"
#define BUFFER_DUMP_FILE "log/buffer_dump"

/* Do tests */
#define TESTS

#define COLORS_LEN 32
/* clang-format off */
static const u8 FG_COLOR_COLLECTION_DEFAULT[COLORS_LEN] = {
  0,                            // Terminal default
  232,                          // Black
  236, 240, 244, 248, 252,      // Gray, dark to lighter
  255,                          // White
  226, 220, 214, 208, 202,      // Yellow --> Orange
  196,                          // Red
  166, 124, 88,  52,            // Brown
  127, 92,                      // Violet, Violet blue
  18,  20,  27,  39,            // Dark blue --> Light blue
  37,  51,                      // Turquoice, Cyan
  46,  40,  34,  28,            // Light green --> Dark green
  118, 154,                     // Neon green, green yellow
};
enum DefaultCollection {
  DefaultCollection_DEFAULT,
  DefaultCollection_BLACK,
  DefaultCollection_GRAY_DARKEST, DefaultCollection_GRAY_DARKER, DefaultCollection_GRAY, DefaultCollection_GRAY_LIGHTER, DefaultCollection_GRAY_LIGHTEST,
  DefaultCollection_WHITE,
  DefaultCollection_YELLOW, DefaultCollection_YELLOW_DARK, DefaultCollection_YELLOW_DARKEST, DefaultCollection_ORANGE_LIGHT, DefaultCollection_ORANGE,
  DefaultCollection_RED,
  DefaultCollection_BROWN_LIGHTEST, DefaultCollection_BROWN_LIGHT, DefaultCollection_BROWN, DefaultCollection_BROWN_DARK,
  DefaultCollection_VIOLET, DefaultCollection_VIOLET_BLUE,
  DefaultCollection_BLUE_DARKEST, DefaultCollection_BLUE_DARK, DefaultCollection_BLUE, DefaultCollection_BLUE_LIGHT,
  DefaultCollection_BLUE_TURQUOISE, DefaultCollection_CYAN,
  DefaultCollection_GREEN_LIGHT, DefaultCollection_GREEN, DefaultCollection_GREEN_DARK, DefaultCollection_GREEN_DARKEST,
  DefaultCollection_GREEN_NEON, DefaultCollection_GREEN_YELLOW
};
/* clang-format on */

#define DEFAULT_COLOR_ID DefaultCollection_WHITE

#endif

// vim: foldmethod=marker foldmarker=startfold,endfold
