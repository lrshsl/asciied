#include <assert.h>
#include <ncurses.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* TODO:
 *  - Treat screen as a buffer (inch)
 *  - Split project into different files
 *    - main, cursed and llib.h header
 *  - Add tests
 *
 * FEATURES:
 *  - Straight line drawing
 *  - Selection
 *    - Clipboard
 *  - Colors
 *
 *  - Command line parsing
 *  - Blink support
 */

/* Where to save log and buffer dump */
#define LOG_FILE_NAME "log/logfile"
#define BUFFER_DUMP_FILE "log/buffer_dump"

/* Do tests */
#define TESTS

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
    log_add(log_err, "Try failed in %s on line %d\n", __FILE__, __LINE__);     \
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
  u8 color_id : 5,
     attrs    : 3;
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

enum Mode {
  mode_normal,
  mode_select,
  mode_drag,
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
/* }}} */

/*** Globals and accessors {{{ ***/
local enum Mode mode = mode_normal;

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
local bool is_dragging = false;

/* }}} */

/*** Prototypes (forward declarations) {{{ ***/
local fn log_add(enum LogLevel lvl, char *fmt, ...);
local fn finish(int sig);

local Result get_cmd_input();
local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]);
local fn draw_buffer(struct CEntry buffer[LINES][COLS]);
local fn dump_buffer(struct CEntry buffer[LINES][COLS], FILE *file);
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attr);
local fn write_to_file(struct CEntry buffer[LINES][COLS], char *filename);
local Result load_from_file(struct CEntry buffer[LINES][COLS], int y, int x,
                            char *filename);
/* }}} */

local fn swallow_interrupt(int sig) {
  log_add(log_debug, "Caught signal %d\n", sig);
}

int COLOR_PAIRS_COUNT = 31;

/*** Main ***/
int main(void) {
  /*** Setup {{{ ***/
  log_add(log_none, "\n");
  log_add(log_info, "Starting...\n");

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

    /* 0 -> NORMAL */
    init_pair(1, COLOR_CYAN, 0);
    init_pair(2, COLOR_BLUE, 0);
    init_pair(3, COLOR_GREEN, 0);
    init_pair(4, COLOR_YELLOW, 0);
    init_pair(5, COLOR_RED, 0);
    init_pair(6, COLOR_MAGENTA, 0);
    init_pair(7, COLOR_RED, 0);

    init_pair(8, COLOR_RED, 0);
    init_pair(9, COLOR_RED, 0);
    init_pair(10, COLOR_GREEN, 0);
    init_pair(11, COLOR_YELLOW, 0);
    init_pair(12, COLOR_BLUE, 0);
    init_pair(13, COLOR_CYAN, 0);
    init_pair(14, COLOR_MAGENTA, 0);
    init_pair(15, COLOR_WHITE, 0);

    init_pair(16, COLOR_RED, 0);
    init_pair(17, COLOR_RED, 0);
    init_pair(18, COLOR_GREEN, 0);
    init_pair(19, COLOR_YELLOW, 0);
    init_pair(20, COLOR_BLUE, 0);
    init_pair(21, COLOR_CYAN, 0);
    init_pair(22, COLOR_MAGENTA, 0);
    init_pair(23, COLOR_WHITE, 0);

    init_pair(24, COLOR_RED, 0);
    init_pair(25, COLOR_RED, 0);
    init_pair(26, COLOR_GREEN, 0);
    init_pair(27, COLOR_YELLOW, 0);
    init_pair(28, COLOR_BLUE, 0);
    init_pair(29, COLOR_CYAN, 0);
    init_pair(30, COLOR_MAGENTA, 0);
    init_pair(31, COLOR_WHITE, 0);

  } /*** }}} ***/

  /*** Initialization {{{ ***/
  int x, y;
  struct CEntry buffer[LINES][COLS];
  struct CEntry clip_buf[LINES][COLS];
  int clip_x = 0;
  int clip_y = 0;
  int palette_element_width_chars = COLS / COLOR_PAIRS_COUNT;

  /* Initialize buffer with spaces */
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      buffer[y][x].ch = ' ';
      buffer[y][x].color_id = 0;
      buffer[y][x].attrs = CE_NONE;
    }
  }

  move(0, 0);
  foreach (color_id, 0, COLOR_PAIRS_COUNT) {
    attrset(COLOR_PAIR(color_id));
    foreach (ltr, 0, palette_element_width_chars) {
      addch('X');
      struct CEntry *e = &buffer[0][color_id * palette_element_width_chars + ltr];
      e->ch = 'X';
      e->color_id = color_id;
      e->attrs = CE_NONE;
    }
  }
  /* }}} */

  /* Tests */
#ifdef TESTS
  test_ce_attrs_helpers();
  assert(sizeof(struct CEntry) == 2);
#endif

  /*** Main loop {{{ ***/
  loop {
    getyx(stdscr, y, x);
    try(move(y, x));
    try(refresh());

    /* Update */
    int ch = getch();

    /* Change draw char with `space + new_char` */
    switch (ch) {
    case ' ':
      ch = getch();

      /* Only change if `ch` is in printable range */
      if (ch >= 32 && ch < 127) {

        /* Change what char to print */
        draw_ch = ch;
      }
      break;

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
          log_add(log_err, "Error loading file: %s\n", cmdline_buf);
          finish(res);
        }
        draw_buffer(buffer);
      }
      break;

    /* Copy and paste */
    case 's':
      /* Select mode */
      if (mode != mode_select)
        mode = mode_select;
      else
        mode = mode_normal;
      break;
    case 'p':
      /* TODO */
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
      react_to_mouse(buffer, clip_buf);
      break;
    }

    /* Else: ignore */
  } /* }}} */

  /*** Quit {{{ ***/
quit:
  endwin();
  printf("Terminal size: %dx%d\n", COLS, LINES);
  printf("Buffer size: %d\n", LINES * COLS);

  /* Dump buffer to file */
#ifdef BUFFER_DUMP_FILE
  FILE *fp = fopen(BUFFER_DUMP_FILE, "w");
  dump_buffer(buffer, fp);
  fclose(fp);
  printf("Buffer dumped to log/buffer_dump\n");
#endif

  exit(0);
  /* }}} */
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
local fn log_add(enum LogLevel lvl, char *fmt, ...) {
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
    move(y, 0);
    foreach (x, 0, COLS) {
      struct CEntry *e = &buffer[y][x];

      /* Convert attrs */
      attr_t attrs = ce_attrs2curs_attr_t(e->attrs);

      /* Write to screen */
      attrset(attrs | COLOR_PAIR(e->color_id));
      addch(e->ch);
    }
  }
  refresh();
} /* }}} */

/* dump_buffer {{{
 * Write the buffer to stdout (debug function)
 */
local fn dump_buffer(struct CEntry buffer[LINES][COLS], FILE *file) {
  fprintf(file, "<--- Buffer dump --->\nCE%d,%d\n", LINES, COLS);
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      fprintf(file, "%c", buffer[y][x].ch);
    }
    fprintf(file, "\n");
  }
  fprintf(file, "<--- End dump --->\n");
  fprintf(file, "<--- Attrs and color --->\nCE%d,%d\n", LINES, COLS);
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      fprintf(file, "|%d%d", buffer[y][x].attrs, buffer[y][x].color_id);
    }
    fprintf(file, "\n");
  }
  fprintf(file, "<--- End dump --->\n");
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
local Result load_from_file(struct CEntry buffer[LINES][COLS], int mouse_y,
                            int mouse_x, char *filename) {

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
    log_add(log_info, "Loading file %s\n", file);
  }

  /* Open the file */
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    log_add(log_warn, "Couldn't open file: %s\n", file);
    return file_not_found;
  }

  /* Check the header */
  /* First two bytes should be 'CE' */
  char header[2];
  fread(header, 1, 2, fp);
  if (header[0] != 'C' || header[1] != 'E') {
    log_add(log_warn,
            "Format not recognized: File header should begin with 'CE'\n");
    return no_input;
  }

  /* The next two ints should be lines and columns */
  int insert_lines, insert_cols;
  fread(&insert_lines, sizeof(int), 1, fp);
  fread(&insert_cols, sizeof(int), 1, fp);

  /* Check if the file is too big */
  if (insert_lines > LINES || insert_cols > COLS) {
    log_add(log_warn, "File too big: %s\n", file);
    fclose(fp);
    return alloc_fail;
  }

  /* Read to clip buffer */
  foreach (y, mouse_y, mouse_y + insert_lines) {
    foreach (x, mouse_x, mouse_x + insert_cols) {

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

  /* insert_partial_buffer(buffer, clip_buf, y, x, insert_lines, insert_cols);
   */

  fclose(fp);
  return ok;
} /* }}} */

/* write_char {{{
 * Write a char to the screen and make the corresponding entry into the buffer
 */
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attrs) {
  y = clamp(y, 1, LINES - 2);
  x = clamp(x, 0, COLS - 1);

  /* Write to buffer */
  buffer[y][x].ch = ch;
  buffer[y][x].color_id = color_id;
  buffer[y][x].attrs = ce_attrs;

  /* Write to screen */
  attr_t attrs = ce_attrs2curs_attr_t(ce_attrs);
  attrset(attrs | COLOR_PAIR(color_id));
  mvaddch(y, x, ch);
  move(y, x); // Don't move on
} /* }}} */

/* react_to_mouse {{{
 * React to mouse events
 */
local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]) {
  switch (mevent.bstate) {
  case BUTTON1_DOUBLE_CLICKED:
  case BUTTON1_CLICKED:
  case BUTTON1_PRESSED:
    /* Start dragging */
    if (mevent.y == 0) {
      cur_pair = buffer[0][mevent.x].color_id;
      log_add(log_info, "Selected color: %d\n", cur_pair);
      break;
    }
    is_dragging = true;
    drag_start.y = mevent.y;
    drag_start.x = mevent.x;

    /* Delete mouse position report */
    break;
  case BUTTON1_RELEASED:
    /* Stop dragging */
    is_dragging = false;
    drag_end.y = mevent.y;
    drag_end.x = mevent.x;
    break;
  case REPORT_MOUSE_POSITION:
    /* Update dragging */
    if (mode == mode_normal && is_dragging) {
      /* Draw at the mouse position */
      write_char(buffer, mevent.y, mevent.x, draw_ch, cur_pair, cur_attrs);
    } else if (mode == mode_select && is_dragging) {
      foreach (y, drag_start.y, drag_end.y) {
        foreach (x, drag_start.x, drag_end.x) {

          /* Write selected region to clip buffer and inverse to screen */
          struct CEntry *e = &buffer[y][x];
          char draw_ch = e->ch;
          short color_pair = e->color_id;
          attr_t attrs = ce_attrs2curs_attr_t(e->attrs);

          /* Write to screen */
          attrset(attrs | COLOR_PAIR(color_pair) ^ A_REVERSE);
          mvaddch(y, x, draw_ch);

          /* Write to clip buffer */
          clip_buf[y][x] = *e;
        }
      }
    }
    break;
  default:
    log_add(log_err, "Illegal mouse state: %d\n", mevent.bstate);
    finish(illegal_state);
  }
} /* }}} */

/* finish {{{
 * Clean up and exit safely with a given exit code
 */
local fn finish(int sig) {
  endwin();
  log_add(log_err, "Exiting with signal %d\n", sig);
  exit(sig);
} /* }}} */

// vim: foldmethod=marker
