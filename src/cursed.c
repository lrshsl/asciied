#include "include/main.h"

#include <ncurses.h>

local struct Vec2 stack[POSITION_STACK_LENGTH] = {};
local usize stack_pos;

void stash_pos() {
	++stack_pos;
	stack_pos %= POSITION_STACK_LENGTH;
	stack[stack_pos] = get_pos();
}

fn restore_pos() {
	struct Vec2 pos = pop_pos();
	try(move(pos.y, pos.x));
}

struct Vec2 pop_pos() {
	--stack_pos;
	stack_pos %= POSITION_STACK_LENGTH;
	return stack[stack_pos--];
}

struct Vec2 get_pos() {
	struct Vec2 pos = {
		.x = getcurx(stdscr),
		.y = getcury(stdscr),
	};
	return pos;
}
