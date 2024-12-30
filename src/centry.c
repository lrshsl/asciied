#include "include/centry.h"
#include "include/colors.h"


const struct CEntry EMPTY_CENTRY = {
	.ch = ' ', .color_id = DEFAULT_COLOR_ID, .attrs = CE_NONE};

/* Curses attrs --> CEntry attrs */
attr_t ce2curs_attrs(u8 attr) {
	switch ( attr ) {
	case 0:
		return 0;
	case CE_REVERSE:
		return A_REVERSE;
	case CE_BOLD:
		return A_BOLD;
	case CE_ITALIC:
		return A_ITALIC;
	case CE_REVERSE | CE_BOLD:
		return A_REVERSE | A_BOLD;
	case CE_REVERSE | CE_ITALIC:
		return A_REVERSE | A_ITALIC;
	case CE_BOLD | CE_ITALIC:
		return A_BOLD | A_ITALIC;
	case CE_REVERSE | CE_BOLD | CE_ITALIC:
		return A_REVERSE | A_BOLD | A_ITALIC;
	default:
		log_add(LOG_WARN, "Unknown curs attr: %d\n", attr);
		return 0;
	}
}

/* Curses attrs <-- CEntry attrs */
u8 curs2ce_attrs(attr_t attr) {
	switch ( attr ) {
	case 0:
		return 0;
	case A_REVERSE:
		return CE_REVERSE;
	case A_BOLD:
		return CE_BOLD;
	case A_ITALIC:
		return CE_ITALIC;
	case A_REVERSE | A_BOLD:
		return CE_REVERSE | CE_BOLD;
	case A_REVERSE | A_ITALIC:
		return CE_REVERSE | CE_ITALIC;
	case A_BOLD | A_ITALIC:
		return CE_BOLD | CE_ITALIC;
	case A_REVERSE | A_BOLD | A_ITALIC:
		return CE_REVERSE | CE_BOLD | CE_ITALIC;
	default:
		log_add(LOG_WARN, "Unknown curs attr: %d\n", attr);
		return 0;
	}
}

/* Curses color_id <-> CEntry color_id are macros */

/* Curses chtype --> CEntry */
struct CEntry curs2ce_all(chtype ch) {
	struct CEntry ce;
	ce.ch = (u8)(ch & A_CHARTEXT);
	ce.color_id = curs2ce_color_id(ch & A_COLOR);
	ce.attrs = curs2ce_attrs(ch & A_ATTRIBUTES);
	return ce;
}

/* Curses chtype <-- CEntry */
chtype ce2curs_all(struct CEntry ce) {
	chtype ch = ce.ch;
	ch |= ce2curs_color_id(ce.color_id);
	ch |= ce2curs_attrs(ce.attrs);
	return ch;
}
