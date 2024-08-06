#include <assert.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "centry.h"
#include "constants.h"
#include "header.h"
#include "log.h"

#define TESTS

/** startfold TODO:
 *  - Treat screen as a buffer (inch)
 *  - Split project into different files
 *    - main: main logic
 *    - centry: storage and related (also drawing at the same time)
 *    - cursed: thin wrapper around ncurses (try and stuff integrated?)
 *    - log: logging and errors
 *    - cmdline: command line input
 *  - Add tests
 *
 * FEATURES:
 * - [ ] Straight line drawing
 * - [X] Selection [80%]
 *   - [ ] Clipboard [50%]
 * - [X] Colors
 *
 * - [ ] Command line parsing
 *   - [X] Basic
 *   - [ ] Advanced (DSL?)
 * - [ ] Blink support
 *
 * endfold */

/** startfold Globals
 * Globals and accessors
 */
const char *SPACES_100 = "                                                  "
                         "                                                  ";

#define DRAW_AREA_MIN_X 0
#define DRAW_AREA_MAX_X COLS - 2
#define DRAW_AREA_MIN_Y 1
#define DRAW_AREA_MAX_Y LINES - 3
#define DRAW_AREA_WIDTH COLS - 1
#define DRAW_AREA_HEIGHT LINES - 2

// Length: excluding NULL terminator
#define COLOR_INDICATOR_LEN 3
#define COLOR_INDICATOR_RIGHT_OFFSET 0
#define COLOR_INDICATOR_STRING SPACES_100

// Length: excluding NULL terminator
#define MODE_INDICATOR_LEN 10

#define NOTIFY_AREA_Y LINES - 2
#define NOTIFY_AREA_X 0
#define NOTIFY_AREA_WIDTH COLS / 2 - MODE_INDICATOR_LEN

#define CURSOR_INVISIBLE 0
#define CURSOR_VISIBLE 1

#define SAVE_DIR "./saves"
#define SAVE_DIR_LEN 7

#define FILE_EXTENSION ".centry"
#define FILE_EXTENSION_LEN 7

const int UI_BG_ATTRS = COLOR_PAIR(DefaultCollection_GRAY) | A_REVERSE;
const int UI_MODE_INDICATOR_ATTRS = COLOR_PAIR(DefaultCollection_WHITE);
const int CMD_LINE_ATTRS = COLOR_PAIR(DefaultCollection_WHITE);
const int NOTIFY_ATTRS = COLOR_PAIR(DefaultCollection_GRAY) | A_REVERSE;

local enum Mode mode = mode_normal;

local char current_char = 'X';
local u8 current_attrs = CE_NONE;
local u8 current_color_id = DEFAULT_COLOR_ID;
local MEVENT mevent;
local char cmdline_buf[1024];

local char currently_open_file[128] = {0};

/* Drag event static variables */
local struct Cords drag_start = {-1, -1};
local struct Cords drag_end;
local bool is_dragging = false;

/* endfold */

/** startfold Prototypes
 * Forward declarations
 */
local fn die_gracefully(int sig);
local fn swallow_interrupt(int sig) {
  log_add(log_debug, "Caught signal %d\n", sig);
}

local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]);
local fn process_mouse_drag(struct CEntry buffer[LINES][COLS],
                            struct CEntry clip_buf[LINES][COLS]);

// Command line
local Result get_cmd_input();
local fn clear_cmdline();
local fn prefill_cmdline(char *str, int n);

// Status line
local fn notify(char *msg);
local fn clear_notifications();
local fn draw_status_line();
local fn clear_status_line();
local fn set_color(u8 color_id);
local fn set_mode(enum Mode new_mode);

// Buffer + Window
local fn clear_draw_area(struct CEntry buffer[LINES][COLS]);
local fn fill_buffer(struct CEntry buffer[LINES][COLS],
                     struct CEntry fill_centry);
local fn draw_buffer(struct CEntry buffer[LINES][COLS]);
local fn draw_ui();
local fn dump_buffer_readable(struct CEntry buffer[LINES][COLS], FILE *file);
local Result save_to_file(struct CEntry buffer[LINES][COLS], char *filename);
local Result load_from_file(struct CEntry buffer[LINES][COLS], int y, int x,
                            char *filename);
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attr);

/* Clipping */
local fn clip_area(struct CEntry clip_buf[LINES][COLS], int start_y,
                   int start_x, int end_y, int end_x);
local fn unclip_area(struct CEntry clip_buf[LINES][COLS], int start_y,
                     int start_x, int end_y, int end_x);
local fn clip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                int y);
local fn unclip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                  int y);

#define PALETTE_COLOR_ID_AT(x) ((x) / (COLS / COLORS_LEN))
/* endfold */

/* startfold Main */

/** startfold init
 * Main function
 */
int main(void) {
  /** Setup **/
  log_add(log_none, "\n");
  log_add(log_info, "Starting...\n");

  signal(SIGINT, swallow_interrupt); /* Set up an interrupt handler */

  /* NCurses setup */
  initscr();            /* Initialize the curses library */
  keypad(stdscr, TRUE); /* Enable keyboard mapping (important for mouse) */
  mouseinterval(50);    /* Press+release under 200ms -> click */
  nonl();               /* Tell curses not to do NL->CR/NL on output */
  cbreak();             /* Take input chars one at a time, no wait for \n */
  noecho();             /* Turn off echoing */

  if (curs_set(CURSOR_INVISIBLE) == ERR) {
    log_add(log_warn, "Failed to hide cursor\n");
  }

  // Capture mouse events
  if (!mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED |
                     BUTTON1_DOUBLE_CLICKED | REPORT_MOUSE_POSITION,
                 NULL)) {
    fprintf(stderr, "No mouse events can be captured. Try a different terminal "
                    "or start with 'TERM=xterm-256color asciied')\n");
  }
  /// Note:
  /// Since I want both color support and mouse position reporting:
  ///
  /// I found that
  /// xterm-256color  --> colors good
  /// xterm-1002      --> mouse reporting good
  ///
  /// So either include the XM string from xterm-1002 into `infocmp
  /// xterm-256color` and compile via `tic`, as an answer in this thread
  /// suggests:
  /// https://stackoverflow.com/questions/29020638/which-term-to-use-to-have-both-256-colors-and-mouse-move-events-in-python-curse#29023361
  ///
  /// or simply print this sequence to enable mouse position reporting, and
  /// leave the terminal on xterm-256color
  printf("\033[?1003h");
  fflush(stdout);

  /* Colors */
  if (!has_colors() || !can_change_color()) {
    log_add(log_err, "Terminal does not support colors. Maybe try "
                     "'TERM=xterm-256color asciied'\n");
    endwin();
    exit(1);
  } else {
    start_color();

    for (int i = 0; i < COLORS_LEN; ++i) {
      init_pair(i, FG_COLOR_COLLECTION_DEFAULT[i],
                FG_COLOR_COLLECTION_DEFAULT[DefaultCollection_BLACK]);
    }
  }

  /* Initialization */
  int x, y;
  struct CEntry buffer[LINES][COLS]; // TODO: strip size to DRAWABLE_AREA
  struct CEntry clip_buf[LINES][COLS];
  int clip_x = 0;
  int clip_y = 0;

  /* Initialize buffer and screen with spaces */
  draw_ui();
  clear_draw_area(buffer);
  /* endfold */

  /** startfold loop **/
  loop {
    getyx(stdscr, y, x);
    try(move(y, x));
    try(refresh());

    /* Update */
    int ch = getch();

    // Save and load colors
    if (ch >= '0' && ch <= '9') {
      log_add(log_debug, "Selected quick color palette %d\n", ch - '0');
    }

    /* Change draw char with `space + new_char` */
    switch (ch) {
    case ' ':
      ch = getch();

      /* Only change if `ch` is in printable range */
      if (ch >= 32 && ch < 127) {

        /* Change what char to print */
        current_char = ch;
      }
      break;

    /* Quit */
    case CTRL('q'):
      goto quit;

    /* Toggle attributes */
    case 'i':
      current_attrs ^= CE_REVERSE;
      break;
    case CTRL('i'):
      current_attrs ^= CE_ITALIC;
      break;
    case CTRL('b'):
      current_attrs ^= CE_BOLD;
      break;

    /* New painting */
    case CTRL('n'):
      notify("New painting? (y/n)");
      if (get_cmd_input() == ok &&
          (strcmp(cmdline_buf, "") != 0 || strcmp(cmdline_buf, "y") != 0 ||
           strcmp(cmdline_buf, "Y") != 0)) {
        fill_buffer(buffer, EMPTY_CENTRY);
        draw_ui();
        draw_buffer(buffer);
      }
      break;

    /* Reload */
    case CTRL('r'):
      draw_buffer(buffer);
      break;

    /* Save and load file */
    case CTRL('s'):
#ifdef TESTS
      assert(SAVE_DIR_LEN == strlen(SAVE_DIR));
#endif // TESTS
      notify("Save as: ");
      if (cmdline_buf[0] != '\0') {
        char *filename_start = currently_open_file + SAVE_DIR_LEN - 1;
        int filename_len = strlen(filename_start) - FILE_EXTENSION_LEN;
        prefill_cmdline(filename_start, filename_len);
      }
      if (get_cmd_input() == ok) {
        clear_notifications();
        Result res = save_to_file(buffer, cmdline_buf);
        if (res != ok) {
          notify("Error saving file");
          log_add(log_err, "Error saving file: %s\n", cmdline_buf);
          die_gracefully(res);
        }
      } else {
        clear_notifications();
      }
      break;
    case CTRL('o'):
      notify("Open file:");
      if (get_cmd_input() == ok) {
        clear_notifications();
        Result res = load_from_file(buffer, y, x, cmdline_buf);
        if (res == file_not_found) {
          log_add(log_err, "File not found: %s\n", cmdline_buf);
          notify("File not found");
          break;
        } else if (res != ok && res != no_input) {
          log_add(log_err, "Error loading file: %s\n", cmdline_buf);
          die_gracefully(res);
        }
        draw_buffer(buffer);
      } else {
        clear_notifications();
      }
      break;

    /* Copy and paste */
    case 's':
      /* Select mode */
      if (mode != mode_select) {
        set_mode(mode_select);
      } else {
        set_mode(mode_normal);
      }
      break;
    case 'p':
      /* TODO */
      break;

    /* Move with arrows */
    case KEY_LEFT:
      curs_set(CURSOR_VISIBLE);
      if (x > 0)
        try(move(y, x - 1));
      break;
    case KEY_RIGHT:
      curs_set(CURSOR_VISIBLE);
      try(move(y, x + 1));
      break;
    case KEY_UP:
      curs_set(CURSOR_VISIBLE);
      if (y > 0)
        try(move(y - 1, x));
      break;
    case KEY_DOWN:
      curs_set(CURSOR_VISIBLE);
      try(move(y + 1, x));
      break;

    /* Write and delete under the cursor */
    case KEY_ENTER:
    case CTRL('m'): /* '\r' */
    case '\n':
      curs_set(CURSOR_VISIBLE);
      write_char(buffer, y, x, current_char, current_color_id, current_attrs);
      break;
    case '\b': /* '^h' */
      curs_set(CURSOR_VISIBLE);
      write_char(buffer, y, x, ' ', 0, 0);
      break;

    /* Mouse event */
    case KEY_MOUSE:
      try(getmouse(&mevent));
      react_to_mouse(buffer, clip_buf);
      break;
    }

    /* Else: ignore */
  }
  /* endfold */

  /** startfold quit **/
quit:
  endwin();
  printf("Terminal size: %dx%d\n", COLS, LINES);
  printf("Buffer size: %d\n", LINES * COLS);

  /* Dump buffer to file */
#ifdef BUFFER_DUMP_FILE
  FILE *fp = fopen(BUFFER_DUMP_FILE, "w");
  dump_buffer_readable(buffer, fp);
  fclose(fp);
  printf("Buffer dumped to log/buffer_dump\n");
#endif

  exit(0);
}
/* endfold */

/* endfold Main */

/* startfold Command line input */

/** startfold get_cmd_input
 * Get input from user via cmd line. User input is written to `cmdline_buf`.
 * Returns `ok` on success, `any_err` on error or interrupt
 */
local Result get_cmd_input() {

  /* Go to cmdline position */
  int y_old = getcury(stdscr);
  int x_old = getcurx(stdscr);
  try(attrset(CMD_LINE_ATTRS));
  try(mvaddnstr(LINES - 1, 0, "> ", 2));
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
      clear_cmdline();
      try(move(y_old, x_old));
      refresh();

      /* Return error: `cmdline_buf` shouldn't be used */
      return any_err;

    /* Delete a character */
    case '\b':
    case CTRL('g'):
      if (i > 0) {
        cmdline_buf[i - 1] = '\0';
        try(addnstr("\b \b", 3));
        i--;
        refresh();
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
  clear_cmdline();
  try(move(y_old, x_old));
  refresh();
  return ok;
}
/* endfold */

/** startfold prefill_cmdline
 * Prefill command line with filename
 */
local fn prefill_cmdline(char *str, int n) {
  strncpy(cmdline_buf, str, n);
  try(attrset(CMD_LINE_ATTRS));
  try(mvaddnstr(LINES - 1, 2, str, n));
}
/* endfold */

/** startfold clear_cmdline
 * Clear command line.
 */
local fn clear_cmdline() {
  try(move(LINES - 1, 0));
  try(clrtoeol());
  try(attrset(UI_BG_ATTRS));
}
/* endfold */

/* endfold Command line input */

/* startfold Status line */
local fn clear_notifications() { draw_status_line(); }

local fn notify(char *msg) {
  int msg_len = strlen(msg);
  try(move(NOTIFY_AREA_Y, NOTIFY_AREA_X));
  try(attrset(NOTIFY_ATTRS));
  try(addnstr(msg, msg_len));
  try(addnstr(SPACES_100, NOTIFY_AREA_WIDTH - msg_len));
  refresh();
}

const char *mode_indicator() {
  switch (mode) {
  case mode_normal:
    return "  NORMAL  ";
  case mode_select:
    return "  SELECT  ";
  case mode_preview:
    return "  PREVIEW ";
  case mode_drag:
    return "  DRAG    ";
  }
  log_add(log_err, "Unknown mode: %d\n", mode);
  return "ERR";
}

local fn draw_status_line() {
  int y = getcury(stdscr);
  int x = getcurx(stdscr);

  move(LINES - 2, 0);
  attrset(UI_BG_ATTRS);
  try(clrtoeol());

  // Draw mode
  move(LINES - 2, COLS / 2 - MODE_INDICATOR_LEN / 2 - 1);
  attrset(UI_MODE_INDICATOR_ATTRS);
  const char *mode_str = mode_indicator();
  try(addnstr(mode_str, MODE_INDICATOR_LEN));

  // Draw color indicator
  move(LINES - 2,
       COLS - COLOR_INDICATOR_LEN - 1 - COLOR_INDICATOR_RIGHT_OFFSET);
  attrset(COLOR_PAIR(current_color_id) | A_REVERSE);
  try(addnstr(COLOR_INDICATOR_STRING, COLOR_INDICATOR_LEN));

#ifdef TESTS
  assert(strlen(COLOR_INDICATOR_STRING) >= COLOR_INDICATOR_LEN);
  assert(strlen(mode_str) == MODE_INDICATOR_LEN);
#endif // TESTS

  // Go back
  try(move(y, x));
}

local fn set_color(u8 color_id) {
  current_color_id = color_id;
  draw_status_line();
  log_add(log_info, "Selected color: %d\n", current_color_id);
}

local fn set_mode(enum Mode new_mode) {
  mode = new_mode;
  draw_status_line();
  log_add(log_info, "Changed mode: %d\n", mode);
}
/* endfold Status line */

/** startfold endswith
 * Check if a given string ends with a given suffix
 */
local bool endswith(char *str, char *suffix) {
  int i = strlen(str) - strlen(suffix);
  if (i < 0)
    return false;
  return strcmp(str + i, suffix) == 0;
}
/* endfold */

/* startfold Window & Buffer */

/** startfold clear_all
 * Clear the buffer
 * Set all entries in the buffer to EMPTY_CENTRY
 * and clear the screen with the correct attributes.
 *
 * Functionally almost equivalent to `fill_buffer(buf, EMPTY_CENTRY);
 * draw_buffer(buf);`, but faster (iterates only once) and only sets and draws.
 */
local fn clear_draw_area(struct CEntry buffer[LINES][COLS]) {
  try(attrset(COLOR_PAIR(EMPTY_CENTRY.color_id) |
              ce2curs_attrs(EMPTY_CENTRY.attrs)));
  int draw_area_width = DRAW_AREA_WIDTH;
  foreach (y, DRAW_AREA_MIN_Y, DRAW_AREA_MAX_Y + 1) {
    // Draw 100 spaces at once
    try(move(y, DRAW_AREA_MIN_X));
    for (int drawn = 0; drawn < DRAW_AREA_WIDTH; drawn += 100) {
      try(addnstr(SPACES_100, draw_area_width - drawn));
    }
    foreach (x, DRAW_AREA_MIN_X, DRAW_AREA_MAX_X + 1) {
      buffer[y][x] = EMPTY_CENTRY;
    }
  }
}
/* endfold */

/** startfold fill_buffer
 * Fill the buffer with `fill_centry`
 */
local fn fill_buffer(struct CEntry buffer[LINES][COLS],
                     struct CEntry fill_centry) {
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      buffer[y][x] = fill_centry;
    }
  }
}
/* endfold */

/** startfold draw_ui
 * Draw all elements, that are not part of the image
 */
local fn draw_ui() {
  // Draw color palette
  move(0, 0);
  foreach (color_id, 0, COLORS_LEN) {
    attrset(COLOR_PAIR(color_id) | A_REVERSE);
    foreach (ltr, 0, COLS / COLORS_LEN) {
      addch(' ');
      /* struct CEntry *e = */
      /*     &buffer[0][color_id * palette_element_width_chars + ltr]; */
      /* e->ch = 'X'; */
      /* e->color_id = color_id; */
      /* e->attrs = CE_NONE; */
    }
  }
  // TODO: draw quick palette
  // Draw status line
  draw_status_line();
  clear_cmdline();
}
/* endfold */

/** startfold draw_buffer
 * Draw the buffer
 */
local fn draw_buffer(struct CEntry buffer[LINES][COLS]) {
  draw_ui();
  move(DRAW_AREA_MIN_Y, DRAW_AREA_MIN_X);
  foreach (y, DRAW_AREA_MIN_Y, DRAW_AREA_MAX_Y + 1) {
    move(y, 0);
    foreach (x, DRAW_AREA_MIN_X, DRAW_AREA_MAX_X + 1) {
      struct CEntry *e = &buffer[y][x];

      /* Convert attrs */
      attr_t attrs = ce2curs_attrs(e->attrs);

      /* Write to screen */
      attrset(attrs | COLOR_PAIR(e->color_id));
      addch(e->ch);
    }
  }
  refresh();
}
/* endfold */

/** startfold dump_buffer_readable
 * Write the buffer to stdout (or another file), first the chars, then the
 * attributes [debug function]
 */
local fn dump_buffer_readable(struct CEntry buffer[LINES][COLS], FILE *file) {
  fprintf(file, "<--- Char dump --->\nCE%d,%d\n", LINES, COLS);
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      fprintf(file, "%c", buffer[y][x].ch);
    }
    fprintf(file, "\n");
  }
  fprintf(file, "<--- End char dump --->\n");
  fprintf(file, "<--- Attrs and color --->\nCE%d,%d\n", LINES, COLS);
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      fprintf(file, "|%c %2d %d", buffer[y][x].ch, buffer[y][x].color_id,
              buffer[y][x].attrs);
    }
    fprintf(file, "\n");
  }
  fprintf(file, "<--- End full dump --->\n");
}
/* endfold */

/** startfold save_to_file
 * Write the buffer to the file
 */
local Result save_to_file(struct CEntry buffer[LINES][COLS], char *filename) {

  /* Open the file */
  if (strlen(filename) > 64) {
    log_add(log_err, "Filename too long: %s\n", filename);
    return alloc_fail;
  }

  /* Build filename */
  if (strncmp(currently_open_file, "saves/", 6) != 0) {
    strncpy(currently_open_file, "saves/", 7);
  }
  strcat(currently_open_file, filename);
  if (!endswith(currently_open_file, ".centry")) {
    strcat(currently_open_file, ".centry");
  }

  FILE *fp = fopen(currently_open_file, "wb");
  if (fp == NULL) {
    log_add(log_err, "Could not open file: %s\n", currently_open_file);
    return file_not_found;
  }

  /* Write the header */
  fwrite("CE", sizeof(char), 2, fp);
  fwrite(&LINES, sizeof(int), 1, fp);
  fwrite(&COLS, sizeof(int), 1, fp);

  fwrite(buffer, sizeof(struct CEntry), LINES * COLS, fp);

  fclose(fp);
  return ok;
}
/* endfold */

/** startfold load_from_file
 * Load the buffer from the file
 */
local Result load_from_file(struct CEntry buffer[LINES][COLS], int insert_pos_y,
                            int insert_pos_x, char *filename) {

  /* Check length of filename */
  if (strlen(filename) > 64) {
    log_add(log_err, "Filename too long: %s\n", filename);
    return alloc_fail;
  }

  /* Add extension if necessary */
  if (endswith(filename, ".centry")) {
    sprintf(currently_open_file, "saves/%s", filename);
  } else {
    sprintf(currently_open_file, "saves/%s.centry", filename);
  }

  if (loglvl >= log_info) {
    log_add(log_info, "Loading file %s\n", currently_open_file);
  }

  /* Open the file */
  FILE *fp = fopen(currently_open_file, "rb");
  if (fp == NULL) {
    log_add(log_warn, "Could not open file: %s\n", currently_open_file);
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
    log_add(log_warn,
            "File %s cannot be loaded (dimensions of saved buffer: %d x %d, "
            "terminal: %d x %d)\n",
            currently_open_file, insert_lines, insert_cols, LINES, COLS);
    fclose(fp);
    return alloc_fail;
  }

  log_add(log_info, "Loading %dx%d bytes from %s\n", insert_lines, insert_cols,
          currently_open_file);

  /* Insert at mouse position, or move away if not enough space */
  insert_pos_y = min(insert_lines + insert_pos_y, LINES - insert_lines);
  insert_pos_x = min(insert_cols + insert_pos_x, COLS - insert_cols);

  fread(buffer, sizeof(struct CEntry), insert_lines * insert_cols, fp);

  /* insert_partial_buffer(buffer, clip_buf, y, x, insert_lines, insert_cols);
   */

  fclose(fp);
  return ok;
}
/* endfold */

/* startfold Clipping */

/** startfold clip_area
 * Clip an area
 * Write all char in the area start_x..end_x, start_y..end_y to the clip buffer
 * and invert the colors on the screen
 */
local fn clip_area(struct CEntry clip_buf[LINES][COLS], int start_y,
                   int start_x, int end_y, int end_x) {
  try(move(start_y, start_x));
  foreach (y, start_y, end_y + 1) {
    try(move(y, start_x));
    foreach (x, start_x, end_x + 1) {
      clip_char_under_cursor(clip_buf, y, x);
    }
  }
  try(move(end_y, end_x));
}

local fn unclip_area(struct CEntry clip_buf[LINES][COLS], int start_y,
                     int start_x, int end_y, int end_x) {
  try(move(start_y, start_x));
  foreach (y, start_y, end_y + 1) {
    try(move(y, start_x));
    foreach (x, start_x, end_x + 1) {
      unclip_char_under_cursor(clip_buf, y, x);
    }
  }
  try(move(end_y, end_x));
}
/* endfold */

/** startfold clip_char_under_cursor
 * Clip a char under the cursor.
 * Writes the char to the clip buffer and the inverses it on the screen.
 * @param clip_buf The clip buffer
 * @param y The y position
 * @param x The x position
 *
 * @note Advances the cursor by one
 */
local fn clip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int y,
                                int x) {
  chtype ch = inch();

  /* Write to clip buffer */
  clip_buf[y][x] = curs2ce_all(ch);

  /* Write to screen */
  attr_t attrs = (ch & A_ATTRIBUTES) ^ A_REVERSE;
  attrset(attrs | COLOR_PAIR(ch & A_COLOR));
  try(addch(ch));
}
/* endfold */

/** startfold unclip_char_under_cursor
 * Unclip a char under the cursor.
 * Removes char from the clip buffer and reverts it (back) on the screen.
 */
local fn unclip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int y,
                                  int x) {
  if (clip_buf[y][x].ch == 0) {
    return;
  }
  /* Read from screen */
  chtype ch = inch();

  /* Delete char from clip buffer */
  clip_buf[y][x].ch = 0;

  /* Re-revert char on screen */
  attr_t attributes = (ch & A_ATTRIBUTES) ^ A_REVERSE;
  attrset(attributes | COLOR_PAIR(ch & A_COLOR));
  try(addch(ch & A_CHARTEXT));
}
/* endfold */

/* endfold Clipping */

/** startfold write_char
 * Write a char to the screen and make the corresponding entry into the
 * buffer
 */
local fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
                    u8 color_id, u8 ce_attrs) {
  y = clamp(y, DRAW_AREA_MIN_Y, DRAW_AREA_MAX_Y);
  x = clamp(x, DRAW_AREA_MIN_X, DRAW_AREA_MAX_X);

  /* Write to buffer */
  buffer[y][x].ch = ch;
  buffer[y][x].color_id = color_id;
  buffer[y][x].attrs = ce_attrs;

  /* Write to screen */
  attr_t attrs = ce2curs_attrs(ce_attrs);
  attrset(attrs | COLOR_PAIR(color_id));
  mvaddch(y, x, ch);
  move(y, x); // Don't move on
}
/* endfold */

/* endfold Window & Buffer */

/** startfold react_to_mouse
 * React to mouse events
 */
local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]) {

  if (mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
    write_char(buffer, mevent.y, mevent.x, current_char, current_color_id,
               current_attrs);
  }
  if (mevent.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED)) {
    /* Mouse click */
    if (mevent.y == 0) {
      /* Color selection */
      set_color(PALETTE_COLOR_ID_AT(mevent.x));
    } else {
      /* Start recording drag event */
      is_dragging = true;
      drag_end.y = mevent.y;
      drag_end.x = mevent.x;
      drag_start.y = mevent.y;
      drag_start.x = mevent.x;
    }
  }

  /* Mouse release */
  if (mevent.bstate & BUTTON1_RELEASED) {
    /* Stop dragging */
    is_dragging = false;
    drag_end.y = mevent.y;
    drag_end.x = mevent.x;
  }

  /* Mouse drag */
  if (mevent.bstate & REPORT_MOUSE_POSITION) {
    process_mouse_drag(buffer, clip_buf);
  }
  if (!(mevent.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED |
                         BUTTON1_DOUBLE_CLICKED | REPORT_MOUSE_POSITION))) {
    log_add(log_err, "Illegal mouse state: %d\n", mevent.bstate);
    die_gracefully(illegal_state);
  }
  if (!(mevent.bstate & REPORT_MOUSE_POSITION)) {
    curs_set(CURSOR_INVISIBLE);
  }
}
/* endfold */

/** startfold process_mouse_drag
 * Update the buffer if dragging is active
 */
local fn process_mouse_drag(struct CEntry buffer[LINES][COLS],
                            struct CEntry clip_buf[LINES][COLS]) {
  if (!is_dragging) {
    return;
  }
  /* Update dragging */
  if (mode == mode_normal) {
    /* Draw at the mouse position */
    write_char(buffer, mevent.y, mevent.x, current_char, current_color_id,
               current_attrs);
  } else if (mode == mode_select) {
    mevent.y = clamp(mevent.y, DRAW_AREA_MIN_Y, DRAW_AREA_MAX_Y);
    mevent.x = clamp(mevent.x, DRAW_AREA_MIN_X, DRAW_AREA_MAX_X);

    int min_y = min(drag_start.y, mevent.y);
    int min_x = min(drag_start.x, mevent.x);
    int max_y = max(drag_start.y, mevent.y);
    int max_x = max(drag_start.x, mevent.x);

    clip_area(clip_buf, min_y, min_x, max_y, max_x);

    /* Clip area has shrinked -> unclip the edges */
    if (drag_end.y > max_y) {
      unclip_area(clip_buf, max_y + 1, min_x, drag_end.y, max_x);
    } else if (drag_end.y < min_y) {
      unclip_area(clip_buf, drag_end.y, min_x, min_y - 1, max_x);
    }
    if (drag_end.x > max_x) {
      unclip_area(clip_buf, min_y, max_x + 1, max_y, drag_end.x);
    } else if (drag_end.x < min_x) {
      unclip_area(clip_buf, min_y, drag_end.x, max_y, min_x - 1);
    }

    drag_end.y = mevent.y;
    drag_end.x = mevent.x;
    try(move(mevent.y, mevent.x));
  }
}
/* endfold */

/** startfold die_gracefully
 * Do some cleaning up and exit safely.
 * @param sig The signal that caused the exit
 */
local fn die_gracefully(int sig) {
  attrset(A_NORMAL);
  endwin();
  log_add(log_err, "Exiting with signal %d\n", sig);
  exit(sig);
}
/* endfold */

// vim: foldmethod=marker foldmarker=startfold,endfold
