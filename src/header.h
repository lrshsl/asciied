#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>

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

/*** Function-like macros and inline functions ***/
#define KEY_ESC 27
#define CTRL(x) ((x) & 0x1f)

/* Try an operation and panic (safely) if operation fails */
#define try(stmt)                                                              \
  if ((stmt) == ERR) {                                                         \
    log_add(log_err, "Try failed in %s on line %d\n", __FILE__, __LINE__);     \
    die_gracefully(didnt_try_hard_enough);                                     \
  };

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

#endif
/* vim: fdm=marker fmr=startfold,endfold */
