#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>

#define local static
#define fn void

#define KEY_ESC 27

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

/* Write a char to the buffer and the screen */
local inline fn write_char(char buffer[COLS][LINES], int y, int x, char ch) {
  y = clamp(y, 0, LINES - 1);
  x = clamp(x, 0, COLS - 1);

  /* Write to buffer */
  buffer[y][x] = ch;

  /* Write to screen */
  move(y, x);
  addch(ch);
  move(y, x); // Don't move on
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

local MEVENT mevent;
local struct Cords drag_start = {-1, -1};
local struct Cords drag_end;
local char draw_ch = 'x';

local inline void start_dragging(int y, int x) {
  drag_start.y = y;
  drag_start.x = x;
}
#define IS_DRAGGING drag_start.x != -1
local inline void END_DRAGGING(int y, int x) {
  drag_start.x = -1;
  drag_end.y = y;
  drag_end.x = x;
}

local void finish(int sig);
local void react_to_mouse();

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
    fprintf(stderr, "No mouse events can be captured\n");
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
  }

  /*** Main loop ***/
  char buffer[COLS][LINES];
  int x, y;

  for (;;) {
    getyx(stdscr, y, x);
    try(move(0, 0));
    try(refresh());

    /* Update */
    int ch = getch();
    if (isalnum(ch) || ch == ' ' || ch == '|' || ch == '_' || ch == '-' ||
        ch == '\\' || ch == '/' || ch == '.' || ch == ',' || ch == ':' ||
        ch == ';' || ch == '\'') {
      draw_ch = ch;
    } else {
      switch (ch) {
      case KEY_ESC:
        finish(0);
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
      write_char(buffer, mevent.y, mevent.x, draw_ch);
    }
  }
  finish(ok);
}

local void react_to_mouse() {
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

local void finish(int sig) {
  endwin();
  exit(sig);
}
