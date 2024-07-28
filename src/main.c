#include <assert.h>
#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "centry.h"
#include "constants.h"
#include "header.h"
#include "log.h"

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

/*** Globals and accessors { ***/
local enum Mode mode = mode_normal;

local char draw_ch = 'X';
local u8 current_attrs = CE_NONE;
local u8 current_pair = 0;
local MEVENT mevent;
local char cmdline_buf[1024];

/* Drag event static variables */
local struct Cords drag_start = {-1, -1};
local struct Cords drag_end;
local bool is_dragging = false;

/* } */

/*** Prototypes (forward declarations) {{{ ***/
local fn die_gracefully(int sig);

local Result get_cmd_input();
local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]);
local fn process_mouse_drag(struct CEntry buffer[LINES][COLS],
                            struct CEntry clip_buf[LINES][COLS]);
local fn clip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                int y);
local fn unclip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                  int y);
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

/*** Main ***/
int main(void) {
  /*** Setup {{{ ***/
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

  // Capture mouse events
  if (!mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED |
                     BUTTON1_DOUBLE_CLICKED | REPORT_MOUSE_POSITION,
                 NULL)) {
    fprintf(stderr, "No mouse events can be captured (try a different terminal "
                    "or set TERM=xterm)\n");
  }
  /// Note:
  /// Since I want both mouse reporting and mouse position reporting:
  ///
  /// I found that xterm-256color --> colors good
  /// xterm-1002 --> mouse reporting good
  ///
  /// So either include the XM string from xterm-1002 into `infocmp
  /// xterm-256color` and compile via `tic` as an answer in this thread
  /// suggests:
  /// https://stackoverflow.com/questions/29020638/which-term-to-use-to-have-both-256-colors-and-mouse-move-events-in-python-curse#29023361
  ///
  /// or simply print this sequence to enable mouse position reporting, and
  /// leave the terminal on xterm-256color
  printf("\033[?1003h");
  fflush(stdout);

  /*** Colors {{{ ***/
  if (!has_colors()) {
    log_add(log_err, "Terminal does not support colors\n");
    endwin();
    exit(1);
  } else {
    start_color();

    for (int i = 0; i < COLORS_LEN; i++) {
      init_pair(i, COLORS_ARRAY[i], 0);
    }
  } /*** }}} ***/

  /*** Initialization {{{ ***/
  int x, y;
  struct CEntry buffer[LINES][COLS];
  struct CEntry clip_buf[LINES][COLS];
  int clip_x = 0;
  int clip_y = 0;
  int palette_element_width_chars = COLS / COLORS_LEN;

  /* Initialize buffer with spaces */
  foreach (y, 0, LINES) {
    foreach (x, 0, COLS) {
      buffer[y][x].ch = ' ';
      buffer[y][x].color_id = 0;
      buffer[y][x].attrs = CE_NONE;
    }
  }

  move(0, 0);
  foreach (color_id, 0, COLORS_LEN) {
    attrset(COLOR_PAIR(color_id));
    foreach (ltr, 0, palette_element_width_chars) {
      addch('X');
      struct CEntry *e =
          &buffer[0][color_id * palette_element_width_chars + ltr];
      e->ch = 'X';
      e->color_id = color_id;
      e->attrs = CE_NONE;
    }
  }
  /* }}} */

  /*** Main loop ***/
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
      write_char(buffer, y, x, draw_ch, current_pair, current_attrs);
      break;
    case '\b':
      write_char(buffer, y, x, ' ', 0, 0);
      break;

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
          die_gracefully(res);
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
      attr_t attrs = ce2curs_attrs(e->attrs);

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
      e->color_id = ce_read_color_id(flags); /**< @todo: single instruction */
      e->attrs = ce_read_attrs(flags);
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
  attr_t attrs = ce2curs_attrs(ce_attrs);
  attrset(attrs | COLOR_PAIR(color_id));
  mvaddch(y, x, ch);
  move(y, x); // Don't move on
} /* }}} */

/* react_to_mouse {{{
 * React to mouse events
 */
local fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                        struct CEntry clip_buf[LINES][COLS]) {
  if (mevent.bstate &
      (BUTTON1_DOUBLE_CLICKED | BUTTON1_CLICKED | BUTTON1_PRESSED)) {
    /* Start dragging */
    if (mevent.y == 0) {
      /* Color selection */
      current_pair = buffer[0][mevent.x].color_id;
      log_add(log_info, "Selected color: %d\n", current_pair);
    } else {
      /* Start recording drag event */
      is_dragging = true;
      drag_start.y = mevent.y;
      drag_start.x = mevent.x;
    }
  }
  if (mevent.bstate & BUTTON1_RELEASED) {
    /* Stop dragging */
    is_dragging = false;
    drag_end.y = mevent.y;
    drag_end.x = mevent.x;
  }
  if (mevent.bstate & REPORT_MOUSE_POSITION) {
    process_mouse_drag(buffer, clip_buf);
  }
  if (!(mevent.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED |
                         BUTTON1_DOUBLE_CLICKED | REPORT_MOUSE_POSITION))) {
    log_add(log_err, "Illegal mouse state: %d\n", mevent.bstate);
    die_gracefully(illegal_state);
  }
} /* }}} */

local fn process_mouse_drag(struct CEntry buffer[LINES][COLS],
                            struct CEntry clip_buf[LINES][COLS]) {
  if (!is_dragging) {
    return;
  }
  /* Update dragging */
  if (mode == mode_normal) {
    /* Draw at the mouse position */
    write_char(buffer, mevent.y, mevent.x, draw_ch, current_pair,
               current_attrs);
  } else if (mode == mode_select) {
    int x, y;
    for (y = drag_start.y; y < mevent.y; ++y) {
      try(move(y, drag_start.x));
      for (x = drag_start.x; x < mevent.x; ++x) {

        clip_char_under_cursor(clip_buf, x, y);
        /* /1* Read char entry *1/ */
        /* struct CEntry *e = &buffer[y][x]; */

        /* /1* Write to clip buffer *1/ */
        /* clip_buf[y][x] = *e; */

        /* /1* Translate CEntry --> char + color + attrs *1/ */
        /* char draw_ch = e->ch; */
        /* short color_pair = e->color_id; */
        /* attr_t attrs = ce_attrs2curs_attr_t(e->attrs); */

        /* /1* Write to screen (with reverse toggled) *1/ */
        /* attrset((attrs | COLOR_PAIR(color_pair)) ^ A_REVERSE); */
        /* try(addch(draw_ch)); */
      }
      unclip_char_under_cursor(clip_buf, x + 1, y);
    }
    try(move(mevent.y + 1, drag_start.x));
    for (x = drag_start.x; x < mevent.x + 1; ++x) {
      unclip_char_under_cursor(clip_buf, x, y + 1);
    }
  }
}

/**
 * Clip a char under the cursor.
 * Writes the char to the clip buffer and the inverses it on the screen.
 * @param clip_buf The clip buffer
 * @param x The x position
 * @param y The y position
 *
 * @note Advances the cursor by one
 */
local fn clip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                int y) {
  chtype ch = inch();

  /* Write to clip buffer */
  clip_buf[y][x] = curs2ce_all(ch);

  /* Write to screen */
  attr_t attrs = (ch & A_ATTRIBUTES) ^ A_REVERSE;
  attrset(attrs | COLOR_PAIR(ch & A_COLOR));
  try(addch(ch));
}

local fn unclip_char_under_cursor(struct CEntry clip_buf[LINES][COLS], int x,
                                  int y) {
  /* Read from screen */
  chtype ch = inch();

  /* Delete char from clip buffer */
  clip_buf[y][x].ch = 0;

  /* Re-revert char on screen */
  attr_t attributes = (ch & A_ATTRIBUTES) ^ A_REVERSE;
  attrset(attributes | COLOR_PAIR(ch & A_COLOR));
  try(addch(ch & A_CHARTEXT));
}

/**
 * Do some cleaning up and exit safely.
 * @param sig The signal that caused the exit
 */
local fn die_gracefully(int sig) {
  endwin();
  log_add(log_err, "Exiting with signal %d\n", sig);
  exit(sig);
}

// vim: foldmethod=marker foldmarker={,}
