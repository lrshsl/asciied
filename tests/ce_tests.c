#include "../src/include/centry.h"
#include <ncurses.h>

/* Tests */
fn test_ce_attrs_helpers() {
	assert(ce_read_attrs(0b00110001) == 0b001, "");
	assert(ce_read_attrs(0b01110111) == 0b011, "");

	assert(ce_read_color_id(0b00110001) == 0b10001, "");
	assert(ce_read_color_id(0b00101101) == 0b01101, "");
	assert(ce_read_color_id(0) == 0, "");
}

fn test_attrs_conversion() {
	assert(ce2curs_attrs(0) == 0, "");
	assert(ce2curs_attrs((CE_REVERSE | CE_ITALIC)) == (A_REVERSE | A_ITALIC),
	       "");
	assert(ce2curs_attrs((CE_REVERSE | CE_BOLD)) == (A_REVERSE | A_BOLD), "");
	assert(ce2curs_attrs(CE_REVERSE | CE_ITALIC | CE_BOLD) ==
	           (A_ITALIC | A_BOLD | A_REVERSE),
	       "");

	assert(curs2ce_attrs(0) == 0, "");
	assert(curs2ce_attrs((A_REVERSE | A_ITALIC)) == (CE_REVERSE | CE_ITALIC),
	       "");
	assert(curs2ce_attrs((A_REVERSE | A_BOLD)) == (CE_REVERSE | CE_BOLD), "");
	assert(curs2ce_attrs(A_REVERSE | A_BOLD | A_ITALIC) ==
	           (CE_REVERSE | CE_BOLD | CE_ITALIC),
	       "");
}

fn test_ce_conversion() { assert(sizeof(struct CEntry) == 2, ""); }

/* Conversion functions */
int main() {
	test_ce_attrs_helpers();
	test_attrs_conversion();
	test_ce_conversion();

	printf("All tests passed.\n");
	return 0;
}
