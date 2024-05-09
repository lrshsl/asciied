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

#define usize size_t
#define isize intptr_t

/*** Function-like macros and inline functions {{{ ***/
#define KEY_ESC 27
#define CTRL(x) ((x) & 0x1f)

/* Try an operation and panic (safely) if operation fails */
#define try(stmt)                                                              \
  if ((stmt) == ERR) {                                                         \
    log(log_err, "Try failed in %s on line %d\n", __FILE__, __LINE__);          \
    finish(didnt_try_hard_enough);                                             \
  };

local inline int clamp(int x, int min, int max) {
  if (x < min)
    return min;
  if (x > max)
    return max;
  return x;
}

#define loop for (;;)

/* foreach macro
 *
 * Not just for convenience, but also to minimize surface area for errors
 * (straight-forward for loops can't have typos anymore).
 *
 * for (int x = 0; x < 10; ++x) {}
 *
 * becomes:
 *
 * foreach(x, 0, 10) {}
 *
 */
#define foreach(x, min, max) for (int(x) = (min); ((x) < (max)); (++(x)))

/* }}} */

/*** Types {{{ ***/
enum CE_Attrs {
  CE_NONE = 0,
  CE_REVERSE = 1,
  CE_BOLD = 2,
  CE_ITALIC = 4,
};

/* CEntry {{{
 * Character entry for `buffer` that is associated with a color pair and a set
 * of attributes.
 *
 * Takes up 2 bytes:
 *    - One byte for the char (one bit theoritically unused)
 *    - 5 bits for id of the color pair (allows for 32 different color pairs per
 * image)
 *    - 3 bits for the attributes (bold, italic, reverse or combinations of
 * them)
 */
struct CEntry {
  char ch;
  u8 color_id : 5, attrs : 3;
};

/* CEntry Helpers
 *
 * Extract color id and attributes from a byte
 *
 * byte : 00010001
 * split: 00010 001
 *          |    |
 *          |    attrs (1 -> CE_REVERSE)
 *          |
 *          color id (2 -> color pair nr. 3 (one indexed))
 */
#define ce_byte2color_id(x) ((x) & (u8)0x1f)
#define ce_byte2attrs(x) ((x) >> 5)

/* Convert attributes to curses attributes */
local attr_t ce_attrs2curs_attr_t(u8 attr) {
  attr_t result = 0;
  if (attr & CE_REVERSE)
    result |= A_REVERSE;
  if (attr & CE_BOLD)
    result |= A_BOLD;
  if (attr & CE_ITALIC)
    result |= A_ITALIC;
  return result;
} /* }}} */

local fn test_ce_attrs_helpers() {
  assert(ce_byte2attrs(0b00110001) == 0b001);
  assert(ce_byte2color_id(0b00110001) == 0b10001);
  assert(ce_attrs2curs_attr_t(0) == 0);
  assert(
      ce_attrs2curs_attr_t((A_REVERSE | A_ITALIC) == (A_REVERSE | A_ITALIC)));
  assert(ce_attrs2curs_attr_t(7) == (A_ITALIC | A_BOLD | A_REVERSE));
}

/* Error codes. Will be extended further and order might change */
enum LogLevel {
  log_none,
  log_err,
  log_warn,
  log_info,
  log_debug,
  log_all,
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
  alloc_fail, /* Couln't allocate enough memory */
  didnt_try_hard_enough, /* Used in `try` macro */
  illegal_state, /* Assertion failed */
} Result;

/* Coordinate pair */
struct Cords {
  int x, y;
};
/* }}} */

/*** Globals and accessors {{{ ***/
local char draw_ch = 'X';
local u8 cur_attrs = CE_NONE;
local u8 cur_pair = 0;
local MEVENT mevent;
local char cmdline_buf[1024];

/* Log level */
local enum LogLevel loglvl = log_all;

/* Drag event static variables */
local struct Cords drag_start = {-1, -1};
local struct Cords drag_end;

/* Internally, -1 for x means that we are not dragging.
 * Helper functions below should be used though */
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
local fn log(enum LogLevel lvl, char *fmt, ...);
local fn finish(int sig);

local Result get_cmd_input();
local fn react_to_mouse();
local fn draw_buffer(struct CEntry buffer[LINES][COLS]);
local fn dump_buffer(struct CEntry buffer[LINES][COLS]);
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attr);
local fn write_to_file(struct CEntry buffer[LINES][COLS], char *filename);
local Result load_from_file(struct CEntry buffer[LINES][COLS], int y, int x, char *filename);
/* }}} */

local fn swallow_interrupt(int sig) {
  log(log_debug, "Caught signal %d\n", sig);
}

/*** Main ***/
int main(void) {
  log(log_none, "\n");
  log(log_info, "Starting...\n");
  assert(sizeof(struct CEntry) == 2);

  /*** Setup {{{ ***/

  signal(SIGINT, swallow_interrupt); /* Arrange interrupt handler */

  /* NCurses setup */
  initscr();            /* Initialize the curses library */
  keypad(stdscr, TRUE); /* Enable keyboard mapping (important for mouse) */
  mouseinterval(50);    /* Press+release under 200ms -> click */
  nonl();               /* Tell curses not to do NL->CR/NL on output */
  cbreak();             /* Take input chars one at a time, no wait for \n */
  noecho();             /* Turn off echoing */

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
  struct CEntry buffer[LINES][COLS];
  struct CEntry clip_buf[LINES][COLS];
  int clip_x = 0;
  int clip_y = 0;

  /* Initialize buffer with spaces */
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      buffer[y][x].ch = ' ';
      buffer[y][x].color_id = 0;
      buffer[y][x].attrs = CE_NONE;
    }
  }

  test_ce_attrs_helpers();
  loop {
    getyx(stdscr, y, x);
    try(move(y, x));
    try(refresh());

    /* Update */
    int ch = getch();

    /* Change draw char with `space + new_char` */
    if (ch == ' ') {
      ch = getch();

      /* Only change if `ch` is in printable range */
      if (ch >= 32 && ch < 127) {

        /* Change what char to print */
        draw_ch = ch;
      }
    } else {

      switch (ch) {

      /* Quit */
      case CTRL('q'):
        goto quit;

      /* Write and delete under the cursor */
      case KEY_ENTER:
      case CTRL('m'):
      case '\n':
        write_char(buffer, y, x, draw_ch, cur_pair, cur_attrs);
        break;
      case '\b':
        write_char(buffer, y, x, ' ', 0, 0);
        break;

      /* Toggle attributes */
      case 'i':
        cur_attrs ^= CE_REVERSE;
        break;
      case CTRL('i'):
        cur_attrs ^= CE_ITALIC;
        break;
      case CTRL('b'):
        cur_attrs ^= CE_BOLD;
        break;

      /* New painting */
      case CTRL('n'):
        clear();
        memset(buffer, ' ', sizeof(buffer));
        break;

      /* Reload */
      case CTRL('r'):
        draw_buffer(buffer);
        break;

      /* Save and load file */
      case CTRL('s'):
        if (get_cmd_input() == ok)
          write_to_file(buffer, cmdline_buf);
        break;
      case CTRL('o'):
        if (get_cmd_input() == ok) {
          Result res = load_from_file(buffer, y, x, cmdline_buf);
          if (res != ok && res != no_input) {
            log(log_err, "Error loading file: %s\n", cmdline_buf);
            finish(res);
          }
          draw_buffer(buffer);
        }
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
        try(getmouse(&mevent));
        if (IS_DRAGGING) {
          write_char(buffer, mevent.y, mevent.x, draw_ch, cur_pair, cur_attrs);
        }
        react_to_mouse();
        break;
      }

      /* Else: ignore */
    }
  } /* }}} */

quit:
  endwin();
  printf("Terminal size: %dx%d\n", COLS, LINES);
  printf("Buffer size: %d\n", LINES * COLS);
  exit(0);
}

/* Cmd line {{{ */
local fn clear_cmd_line() {
  try(move(LINES - 1, 0));
  foreach (i, 0, COLS - 1) {
    try(addch(' '));
  }
}

/* get_cmd_input
 * Get input from user via cmd line. User input is written to `cmdline_buf`.
 * Returns `ok` on success, `any_err` on error or interrupt
 */
local Result get_cmd_input() {

  /* Go to cmdline position */
  int y_old, x_old;
  getyx(stdscr, y_old, x_old);
  try(mvaddch(LINES - 1, 0, '>'));
  refresh();

  /* Retrieve input */
  int i = 0;
  char ch;
  while ((ch = getch())) {
    switch (ch) {

    /* Unexpected end of input */
    case KEY_ESC:
    case CTRL('c'):
    case CTRL('q'):
    case CTRL('d'):

      /* Clear cmd line and go back to old position */
      cmdline_buf[0] = '\0';
      clear_cmd_line();
      try(move(y_old, x_old));
      refresh();

      /* Return error: `cmdline_buf` shouldn't be used */
      return any_err;

    /* Delete a character */
    case '\b':
      if (i > 0) {
        cmdline_buf[i - 1] = '\0';
        try(addstr("\b \b"));
        refresh();
        i--;
      }
      break;

    /* Confirm */
    case '\n':
    case CTRL('m'):
      cmdline_buf[i] = '\0';
      goto quit;

    /* Read more */
    default:
      cmdline_buf[i] = ch;
      try(addch(ch));
      i++;
    }
  }

quit:
  /* Clear cmd line again */
  clear_cmd_line();
  try(move(y_old, x_old));
  refresh();
  return ok;
} /* }}} */

/* log {{{ */
local fn log(enum LogLevel lvl, char *fmt, ...) {
  if (loglvl < lvl) {
    return;
  }

  FILE *logfile = fopen("log", "a");
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
} /* }}} */

/* endswith {{{ */
local bool endswith(char *str, char *suffix) {
  int i = strlen(str) - strlen(suffix);
  if (i < 0)
    return 0;
  return strcmp(str + i, suffix) == 0;
} /* }}} */

/* draw_buffer {{{
 * Draw the buffer
 */
local fn draw_buffer(struct CEntry buffer[LINES][COLS]) {
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      struct CEntry *e = &buffer[y][x];

      /* Convert attrs */
      attr_t attrs = ce_attrs2curs_attr_t(e->attrs);

      /* Write to screen */
      chgat(1, attrs, e->color_id, NULL);
      move(y, x);
      addch(e->ch);
      move(y, x); // Why tf is this necessary?
    }
  }
  refresh();
} /* }}} */

/* dump_buffer {{{
 * Write the buffer to stdout (debug function)
 */
local fn dump_buffer(struct CEntry buffer[LINES][COLS]) {
  printf("<--- Buffer dump --->\n");
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      printf("%c", buffer[y][x].ch);
    }
    printf("\n");
  }
  printf("<--- End dump --->\n");
} /* }}} */

/* write_to_file {{{
 * Write the buffer to the file
 */
local fn write_to_file(struct CEntry buffer[LINES][COLS], char *filename) {

  /* Open the file */
  char file[128];
  if (strlen(filename) > 128) {
    return;
  }
  if (endswith(filename, ".centry")) {
    sprintf(file, "saves/%s", filename);
  } else {
    sprintf(file, "saves/%s.centry", filename);
  }
  FILE *fp = fopen(file, "wb");
  if (fp == NULL) {
    return;
  }

  /* Write the header */
  fwrite("CE", sizeof(char), 2, fp);
  fwrite(&LINES, sizeof(int), 1, fp);
  fwrite(&COLS, sizeof(int), 1, fp);

  fwrite(buffer, sizeof(struct CEntry), LINES * COLS, fp);

/*   /1* Write the buffer *1/ */
/*   foreach (y, 0, LINES) { */
/*     foreach (x, 0, COLS) { */
/*       struct CEntry *e = &buffer[y][x]; */
/*       fputc(e->ch, fp); */
/*       u8 attrs = e->color_id | (e->attrs << 5); */
/*       fputc(attrs, fp); */
/*     } */
/*   } */

  fclose(fp);
} /* }}} */

/* load_from_file {{{
 * Load the buffer from the file
 */
local Result load_from_file(struct CEntry buffer[LINES][COLS], int y, int x, char *filename) {

  /* Open the file */
  char file[128];

  /* Check length of filename */
  if (strlen(filename) > 128) {
    return alloc_fail;
  }

  /* Add extension if necessary */
  if (endswith(filename, ".centry")) {
    sprintf(file, "saves/%s", filename);
  } else {
    sprintf(file, "saves/%s.centry", filename);
  }

  if (loglvl >= log_info) {
    log(log_info, "Loading file %s\n", file);
  }

  /* Open the file */
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    log(log_warn, "Couldn't open file: %s\n", file);
    return file_not_found;
  }

  /* Check the header */
  /* First two bytes should be 'CE' */
  char header[2];
  fread(header, 1, 2, fp);
  if (header[0] != 'C' || header[1] != 'E') {
    log(log_warn, "Format not recognized: File header should begin with 'CE'\n");
    return no_input;
  }

  /* The next two ints should be lines and columns */
  int insert_lines, insert_cols;
  fread(&insert_lines, sizeof(int), 1, fp);
  fread(&insert_cols, sizeof(int), 1, fp);

  /* Check if the file is too big */
  if (insert_lines > LINES || insert_cols > COLS) {
    log(log_warn, "File too big: %s\n", file);
    fclose(fp);
    return alloc_fail;
  }

  /* Read to clip buffer */
  foreach (y, 0, insert_lines) {
    foreach (x, 0, insert_cols) {

      /* Read two bytes and change the corresponding entry in `clip_buf` */
      struct CEntry *e = &buffer[y][x];

      /* Char */
      e->ch = (u8)fgetc(fp);

      /* Flags */
      u8 flags = (u8)fgetc(fp);
      e->color_id = ce_byte2color_id(flags);
      e->attrs = ce_byte2attrs(flags);
    }
  }

  /* insert_partial_buffer(buffer, clip_buf, y, x, insert_lines, insert_cols); */

  fclose(fp);
  return ok;
} /* }}} */

/* write_char {{{
 * Write a char to the screen and make the corresponding entry into the buffer
 */
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attrs) {
  y = clamp(y, 0, LINES - 2);
  x = clamp(x, 0, COLS - 1);

  /* Write to buffer */
  buffer[y][x].ch = ch;
  buffer[y][x].color_id = color_id;
  buffer[y][x].attrs = ce_attrs;

  /* Write to screen */
  attr_t attrs = ce_attrs2curs_attr_t(ce_attrs);
  chgat(1, attrs, color_id, NULL);
  move(y, x);
  addch(ch);
  move(y, x); // Don't move on
} /* }}} */

/* react_to_mouse {{{
 * React to mouse events
 */
local fn react_to_mouse() {
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
    log(log_err, "Illegal mouse state: %d\n", mevent.bstate);
    finish(illegal_state);
  }
} /* }}} */

/* finish {{{
 * Clean up and exit safely with a given exit code
 */
local fn finish(int sig) {
  endwin();
  log(log_err, "Exiting with signal %d\n", sig);
  exit(sig);
} /* }}} */

// vim: foldmethod=marker
