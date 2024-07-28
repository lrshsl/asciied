#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "header.h"

/* Where to save log and buffer dump */
#define LOG_FILE_NAME "log/logfile"
#define BUFFER_DUMP_FILE "log/buffer_dump"

/* Do tests */
#define TESTS

/* startfold Color definitions */
#define DEFAULT 0
#define BLACK 232

#define GRAY0 236 // Dark gray
#define GRAY1 240
#define GRAY2 244
#define GRAY3 248
#define GRAY4 252
#define GRAY5 255 // White

#define YELLOW0 226 // Yellow
#define YELLOW1 220
#define YELLOW2 214
#define YELLOW3 208
#define YELLOW4 202
#define YELLOW5 196 // Orange

#define RED0 166 // Red
#define RED1 124
#define RED2 88
#define RED3 52
#define RED4 127
#define RED5 92 // Dark red

#define BLUE0 18 // Dark blue
#define BLUE1 20
#define BLUE2 27
#define BLUE3 39
#define BLUE4 37
#define BLUE5 51 // Cyan

#define GREEN0 46 // Light green
#define GREEN1 40
#define GREEN2 34
#define GREEN3 28
#define GREEN4 118
#define GREEN5 154 // Green yellow
/* endfold */

#define COLORS_LEN 32
static const u8 COLORS_ARRAY[COLORS_LEN] = {
  DEFAULT, BLACK,
  GRAY0, GRAY1, GRAY2, GRAY3, GRAY4, GRAY5,
  YELLOW0, YELLOW1, YELLOW2, YELLOW3, YELLOW4, YELLOW5,
  RED0, RED1, RED2, RED3, RED4, RED5,
  BLUE0, BLUE1, BLUE2, BLUE3, BLUE4, BLUE5,
  GREEN0, GREEN1, GREEN2, GREEN3, GREEN4, GREEN5
};

#endif

// vim: foldmethod=marker foldmarker=startfold,endfold
