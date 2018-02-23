// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

// state for ANSI escape sequence interpretation
enum {
	A_NORM = 0,
	A_TRANS,
	A_ESCAPE
};

// colormap - number (x - 30/40)[0, 7] -> attribute byte
static uint16_t colormap[8] =
{
	0x0000, 0x4400, 0x2200, 0x6600,
	0x1100, 0x5500, 0x3300, 0x7700,
};

static void
attribute_punch(int ch, int *cnt) {
	static int value = 0; // value
	static int state = A_NORM; // current state
	static int temp = 0x0000, attribute = 0x0000; // temp attribute, current attribute

	switch(state) { // state machine
		case A_NORM:
			if (ch == 0x1B) { // [ESC]
				state = A_TRANS; // transfer from A_NORM to A_TRANS
			} else {
				cputchar((attribute & 0xFF00) | ch); // put character with attribute
				*cnt++;
			}
			break;
		case A_TRANS:
			if (ch == '[') { // [
				state = A_ESCAPE; // transfer from A_TRANS to A_ESCAPE
			} else {
				state = A_NORM; // transfer from A_TRANS to A_NORM
			}
			break;
		case A_ESCAPE:
			if (ch >= '0' && ch <= '9') { // digit - update value
				value = value * 10 + ch - '0';
			} else if (ch == ';' || ch == 'm') { // ; or m set temp and clear value
				if (value == 0) {
					temp  = colormap[0];
				} else if (value == 5) {
					temp |= 0x8000;
				} else if (value >= 30 && value <= 38) {
					temp |= colormap[value - 30] & 0x0700; // look up in color map
				} else if (value >= 40 && value <= 48) {
					temp |= colormap[value - 40] & 0x7000; // avoid complex cases
				}
				value = 0;
				if (ch == 'm') { // m needed extra work - update attribute
					attribute = temp;
					temp = 0x0000;
					state = A_NORM; // transfer from A_ESCAPE to A_NORM
				}
			} else { // non_digit nor m
				state = A_NORM; // transfer from A_ESCAPE to A_NORM
			}
			break;
	}
}

static void
putch(int ch, int *cnt)
{
	cputchar(ch);
	*cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	// vprintfmt((void*)putch, &cnt, fmt, ap);
	// use attribute_punch rather than punch
	vprintfmt((void *)attribute_punch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
