#ifndef CE_CURSED_H
#define CE_CURSED_H

#include "config.h"
#include "log.h"
#include "vec.h"

void stash_pos();
fn restore_pos();
struct Vec2 pop_pos();
struct Vec2 get_pos();

#endif
