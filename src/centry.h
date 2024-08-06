#ifndef CENTRY_H
#define CENTRY_H

#include "header.h"
#include "cursed.h"
#include "constants.h"
#include <assert.h>
#include <ncurses.h>

/*** Char Entry Types and helpers ***/
enum CE_Attrs {
  CE_NONE = 0,
  CE_REVERSE = 1,
  CE_BOLD = 2,
  CE_ITALIC = 4,
};

/* CEntry struct {{{
 * Character entry for `buffer` that is associated with a color pair and a set
 * of attributes.
 *
 * Takes up 2 bytes:
 *    - One byte for the char (one bit theoritically unused)
 *    - 5 bits for id of the color pair (allows for 32 different color pairs per
 * image)
 *    - 3 bits for the attributes (bold, italic, reverse or combinations of
 * them)
 * }}} */
struct CEntry {
  char ch;
  u8 color_id : 5, attrs : 3;
};

extern const struct CEntry EMPTY_CENTRY;

/* CEntry Helpers {{{
 *
 * Extract color id and attributes from a byte
 *
 * byte : 00010001
 * split: 00010 001
 *          |    |
 *          |    attrs (1 -> CE_REVERSE)
 *          |
 *          color id (2 -> color pair nr. 3 (one indexed))
 * }}} */
#define ce_read_color_id(x) ((x) & (u8)0x1f)
#define ce_read_attrs(x) ((x) >> 5)

/* Curses attrs --> CEntry attrs */
attr_t ce2curs_attrs(u8 attr);

/* Curses attrs <-- CEntry attrs */
u8 curs2ce_attrs(attr_t attr);

/* Curses color_id --> CEntry color_id */
#define ce2curs_color_id(color_id) (chtype)(color_id)
/* Curses color_id <-- CEntry color_id */
#define curs2ce_color_id(color_id) (u8)(color_id)

/* Curses chtype --> CEntry */
struct CEntry curs2ce_all(chtype ch);
/* Curses chtype <-- CEntry */
chtype ce2curs_all(struct CEntry ce);


/*** Editor ***/
enum Mode {
  mode_normal,  /**< (Normal | Draw) mode */
  mode_select,  /**< Select area in image (unstable) */
  mode_drag,    /**< Drag a selection (unimplemented) */
  mode_preview, /**< (Paste | file load) preview (unimplemented) */
};

/* Error codes. Will be extended further and order might change */
typedef enum Err {

  /* Success */
  ok,

  /* Can usually be recovered */
  no_input,

  /* Maybe recoverable */
  file_not_found, /* fopen failed */

  /* Can't be recovered */
  any_err,
  alloc_fail,            /* Couldn't allocate enough memory */
  didnt_try_hard_enough, /* When `try` fails */
  illegal_state,         /* Assertion failed */
} Result;

/* Coordinate pair */
struct Cords {
  int x, y;
};

#endif
