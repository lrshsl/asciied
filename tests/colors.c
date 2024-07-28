#include <ncurses.h>

int main() {
  initscr();
  start_color();
  for (int i = 0; i < 256; i++) {
    init_pair(i, i, 0);
    attrset(COLOR_PAIR(i) | A_REVERSE);
    printw("%d ", i);
  }
  getch();
  endwin();
}
