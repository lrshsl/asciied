#include <ncurses.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct CEntry {
  char ch;
  u8 color_id : 5, flags : 3;
};

enum err {
  ok,
  any_err,
  alloc_fail,
  illegal_state = 69,
};

struct Cords {
  int x, y;
};

local char draw_ch = 'X';
local int cur_flags = 0;
local short cur_pair = 0;
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

local fn finish(int sig);
local fn react_to_mouse();
local fn write_char(struct CEntry buffer[COLS][LINES], int y, int x, char ch,
                    short color_id, int flags);

int main(void) {

  /*** Setup ***/

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
  }

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
  }

  /*** Main loop ***/

  struct CEntry buffer[COLS][LINES];
  int x, y;

  for (;;) {
    getyx(stdscr, y, x);
    try(move(y, x));
    try(refresh());

    /* Update */
    int ch = getch();
    if (ch >= 32 && ch < 127) {
      /* Printable character */
      draw_ch = ch;
    } else {
      switch (ch) {
      case KEY_ESC:
      case CTRL('q'):
        finish(0);
        break;
      case CTRL('i'):
        cur_flags ^= A_REVERSE;
        break;
      case CTRL('b'):
        cur_flags ^= A_BOLD;
        break;
      case CTRL('c'):
        clear();
        memset(buffer, 0, sizeof(buffer));
        break;
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
      case KEY_MOUSE:
        if (getmouse(&mevent) == OK) {
          react_to_mouse();
        }
        break;
      }
    }
    if (IS_DRAGGING) {
      write_char(buffer, mevent.y, mevent.x, draw_ch, cur_pair, cur_flags);
    }
  }
  finish(ok);
}

/* Write a char to the screen and make the corresponding entry into the buffer
 */
local fn write_char(struct CEntry buffer[COLS][LINES], int y, int x, char ch,
                    short color_id, int flags) {
  y = clamp(y, 0, LINES - 1);
  x = clamp(x, 0, COLS - 1);

  /* Write to buffer */
  buffer[y][x].ch = ch;
  buffer[y][x].color_id = color_id;
  buffer[y][x].flags = flags;

  /* Write to screen */
  chgat(1, flags, color_id, NULL);
  move(y, x);
  addch(ch);
  move(y, x); // Don't move on
}

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
    printf("state: %d\n", mevent.bstate);
    finish(illegal_state);
  }
}

local fn finish(int sig) {
  endwin();
  exit(sig);
}
