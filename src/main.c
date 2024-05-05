#include <assert.h>
#include <ncurses.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define local static
#define fn void

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

/*** Function-like macros and inline functions {{{ ***/

#define KEY_ESC 27
#define CTRL(x) ((x) & 0x1f)

/* Try an operation and panic (safely) if operation fails */
#define try(stmt)                                                              \
  if ((stmt) == ERR) {                                                         \
    fprintf(stderr, "Error 69 in %s on line %d", __FILE__, __LINE__);          \
    finish(69);                                                                \
  };

local inline int clamp(int x, int min, int max) {
  if (x < min)
    return min;
  if (x > max)
    return max;
  return x;
}

/* }}} */

/*** Types {{{ ***/
enum CE_Attrs {
  CE_NONE,
  CE_REVERSE,
  CE_BOLD,
  CE_ITALIC,
};

struct CEntry {
  char ch;
  u8 color_id : 5, attrs : 3;
};
#define ce_byte2color_id(x) ((x) & (u8)0x1f)
#define ce_byte2attrs(x) ((x) >> 5)
local attr_t ce_attrs2curs_attr_t(u8 attr) {
  attr_t result = 0;
  if (attr & CE_REVERSE)
    result |= A_REVERSE;
  if (attr & CE_BOLD)
    result |= A_BOLD;
  if (attr & CE_ITALIC)
    result |= A_ITALIC;
  return result;
}

enum err {
  ok,
  any_err,
  alloc_fail,
  illegal_state = 69,
};

struct Cords {
  int x, y;
};
/* }}} */

/*** Globals and accessors {{{ ***/
local char draw_ch = 'X';
local u8 cur_attrs = CE_NONE;
local u8 cur_pair = 0;
local MEVENT mevent;

/* Drag event static variables */
local struct Cords drag_start = {-1, -1};
local struct Cords drag_end;

local inline void start_dragging(int y, int x) {
  drag_start.y = y;
  drag_start.x = x;
}
local inline void END_DRAGGING(int y, int x) {
  drag_start.x = -1;
  drag_end.y = y;
  drag_end.x = x;
}
#define IS_DRAGGING drag_start.x != -1
/* }}} */

/*** Prototypes (forward declarations) {{{ ***/
local fn finish(int sig);
local fn react_to_mouse();
local fn draw_buffer(struct CEntry buffer[COLS][LINES]);
local fn write_char(struct CEntry buffer[COLS][LINES], int y, int x, char ch,
                    u8 color_id, u8 flags);
local fn write_to_file(struct CEntry buffer[COLS][LINES], char *file);
local fn load_from_file(struct CEntry buffer[COLS][LINES], char *file);
/* }}} */

/*** Main ***/
int main(void) {
  assert(sizeof(struct CEntry) == 2);

  /*** Setup {{{ ***/

  (void)signal(SIGINT, finish); /* arrange interrupts to terminate */

  (void)initscr();      /* initialize the curses library */
  keypad(stdscr, TRUE); /* enable keyboard mapping (important for mouse) */
  mouseinterval(50);    /* press+release under 200ms -> click */
  (void)nonl();         /* tell curses not to do NL->CR/NL on output */
  (void)cbreak();       /* take input chars one at a time, no wait for \n */
  (void)noecho();       /* turn off echoing */

  // Capture mouse events
  if (!mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED |
                     BUTTON1_DOUBLE_CLICKED | REPORT_MOUSE_POSITION,
                 NULL)) {
    fprintf(stderr, "No mouse events can be captured (try a different terminal "
                    "or set TERM=xterm)\n");
  } /* }}} */

  /*** Colors {{{ ***/
  if (has_colors()) {
    start_color();

    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_CYAN, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);

    init_pair(8, COLOR_RED, COLOR_WHITE);
    init_pair(9, COLOR_GREEN, COLOR_WHITE);
    init_pair(10, COLOR_YELLOW, COLOR_WHITE);
    init_pair(11, COLOR_BLUE, COLOR_WHITE);
    init_pair(12, COLOR_CYAN, COLOR_WHITE);
    init_pair(13, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(14, COLOR_BLACK, COLOR_WHITE);
  } /*** }}} ***/

  /*** Main loop {{{ ***/

  int x, y;
  struct CEntry buffer[COLS][LINES];
  /* Initialize buffer with spaces */
  memset(buffer, 32,
         sizeof(buffer)); /* Haha, this only works bc space is 32 which
                             luckily doesn't set the first 3 bits */

  for (;;) {
    getyx(stdscr, y, x);
    try(move(y, x));
    try(refresh());

    /* Update */
    int ch = getch();

    /* If char is in printable range */
    if (ch >= 32 && ch < 127) {

      /* Change what char to print */
      draw_ch = ch;

    } else {

      /* Some kind of special key */
      switch (ch) {

      /* Quit */
      case KEY_ESC:
      case CTRL('q'):
        goto quit;

      /* Toggle attributes */
      case CTRL('i'):
        cur_attrs ^= CE_REVERSE;
        break;
      case CTRL('b'):
        cur_attrs ^= CE_BOLD;
        break;

      /* New painting */
      case CTRL('n'):
        clear();
        memset(buffer, 32, sizeof(buffer));
        break;

      /* Reload */
      case CTRL('r'):
        draw_buffer(buffer);
        break;

      /* Save and load file */
      case CTRL('s'):
        write_to_file(buffer, "buffer.centry");
        break;
      case CTRL('o'):
        load_from_file(buffer, "buffer.centry");
        draw_buffer(buffer);
        break;

      /* Move with arrows */
      case KEY_LEFT:
        if (x > 0)
          try(move(y, x - 1));
        break;
      case KEY_RIGHT:
        try(move(y, x + 1));
        break;
      case KEY_UP:
        if (y > 0)
          try(move(y - 1, x));
        break;
      case KEY_DOWN:
        try(move(y + 1, x));
        break;

      /* Mouse event */
      case KEY_MOUSE:
        react_to_mouse();
        break;
      }

      /* Else: ignore */
    }

    /* Draw if dragging --> no mouse event */
    if (IS_DRAGGING) {
      write_char(buffer, mevent.y, mevent.x, draw_ch, cur_pair, cur_attrs);
    }
  } /* }}} */

quit:
  endwin();
  printf("Terminal size: %dx%d\n", COLS, LINES);
  exit(0);
}

/* draw_buffer {{{
 * Draw the buffer
 */
local fn draw_buffer(struct CEntry buffer[COLS][LINES]) {
  for (int y = 0; y < LINES; ++y) {
    for (int x = 0; x < COLS; ++x) {
      struct CEntry *e = &buffer[y][x];

      /* Flags */
      int attrs = ce_attrs2curs_attr_t(e->attrs);
      short color_id = e->color_id;
      chgat(1, attrs, color_id, NULL);

      /* Print the char */
      mvaddch(y, x, e->ch);
    }
  }
} /* }}} */

/* write_to_file {{{
 * Write the buffer to the file
 */
local fn write_to_file(struct CEntry buffer[COLS][LINES], char *file) {
  FILE *fp = fopen(file, "wb");
  if (fp == NULL) {
    return;
  }
  for (int y = 0; y < LINES; ++y) {
    for (int x = 0; x < COLS; ++x) {
      struct CEntry *e = &buffer[y][x];
      u8 flags = e->color_id | e->attrs;
      fputc(e->ch, fp);
      fputc(flags, fp);
    }
  }
  fclose(fp);
} /* }}} */

/* load_from_file {{{
 * Load the buffer from the file
 */
local fn load_from_file(struct CEntry buffer[COLS][LINES], char *file) {
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    return;
  }

  /* Check the filesize */
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  rewind(fp);
  assert(size == LINES * COLS * 2);

  for (int y = 0; y < LINES; y++) {
    for (int x = 0; x < COLS; x++) {

      /* Read two bytes and change the corresponding entry in `buffer` */
      struct CEntry *e = &buffer[y][x];

      /* Char */
      e->ch = (u8)fgetc(fp);

      /* Flags */
      u8 flags = (u8)fgetc(fp);
      e->color_id = ce_byte2color_id(flags);
      e->attrs = ce_byte2attrs(flags);
    }
  }

  fclose(fp);
} /* }}} */

/* write_char {{{
 * Write a char to the screen and make the corresponding entry into the buffer
 */
local fn write_char(struct CEntry buffer[COLS][LINES], int y, int x, char ch,
                    u8 color_id, u8 attrs) {
  y = clamp(y, 0, LINES - 1);
  x = clamp(x, 0, COLS - 1);

  /* Write to buffer */
  buffer[y][x].ch = ch;
  buffer[y][x].color_id = color_id;
  buffer[y][x].attrs = attrs;

  /* Write to screen */
  attrs = ce_attrs2curs_attr_t(attrs);
  chgat(1, attrs, color_id, NULL);
  move(y, x);
  addch(ch);
  move(y, x); // Don't move on
} /* }}} */

/* react_to_mouse {{{
 * Calls getmouse() and handles the event
 */
local fn react_to_mouse() {
  try(getmouse(&mevent));

  switch (mevent.bstate) {
  case BUTTON1_DOUBLE_CLICKED:
  case BUTTON1_CLICKED:
  case BUTTON1_PRESSED:
    drag_start.x = mevent.x;
    drag_start.y = mevent.y;
    break;
  case BUTTON1_RELEASED:
    drag_start.x = -1;
    drag_start.y = -1;
    break;
  case REPORT_MOUSE_POSITION:
    /* Catch but do nothing -> sets mevent.x and mevent.y */
    break;
  default:
    printf("state: %d\n", mevent.bstate);
    finish(illegal_state);
  }
} /* }}} */

/* finish {{{
 * Clean up and exit safely with a given exit code
 */
local fn finish(int sig) { exit(sig); } /* }}} */

// vim: foldmethod=marker
