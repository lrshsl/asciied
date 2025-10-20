#ifndef CE_MAIN_H
#define CE_MAIN_H

#include "centry.h"
#include "header.h"
fn die_gracefully(int sig);

fn swallow_interrupt(int sig);

fn react_to_mouse(struct CEntry buffer[LINES][COLS],
                  struct CEntry clip_buf[LINES][COLS]);
fn process_mouse_drag(struct CEntry buffer[LINES][COLS],
                      struct CEntry clip_buf[LINES][COLS]);

// Command line
fn cmdline_prepare();
Result cmdline_read_input();
fn clear_cmdline();
fn prefill_cmdline(char *str, int n);

// Status line
fn notify(char *msg);
fn clear_notifications();
fn draw_status_line();
fn clear_status_line();
fn set_color(u8 color_id);
fn set_mode(enum Mode new_mode);

// Buffer + Window
fn clear_draw_area(struct CEntry buffer[LINES][COLS]);
fn fill_buffer(struct CEntry buffer[LINES][COLS], struct CEntry fill_centry);
fn draw_buffer(struct CEntry clip_buf[LINES][COLS],
               struct CEntry buffer[LINES][COLS]);
fn draw_area(struct CEntry buffer[LINES][COLS], int min_y, int min_x, int max_y,
             int max_x, bool inverted);
fn redraw_char(struct CEntry buffer[LINES][COLS], int y, int x, bool inverted);
fn draw_ui();
fn dump_buffer_readable(struct CEntry buffer[LINES][COLS], FILE *file);
Result save_to_file(struct CEntry buffer[LINES][COLS], char *filename);
Result load_from_file(struct CEntry buffer[LINES][COLS], int y, int x,
                      char *filename);
fn write_char(struct CEntry buffer[LINES][COLS], int y, int x, char ch,
              u8 color_id, u8 ce_attr);


#define PALETTE_COLOR_ID_AT(x) ((x) / (COLS / COLORS_LEN))

#endif
