// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

enum {
	A_NORM = 0,
	A_TRANS,
	A_ESCAPE
};

static uint16_t colormap[8] =
{
	0x0000, 0x4400, 0x2200, 0x6600,
	0x1100, 0x5500, 0x3300, 0x7700,
};

static void
attribute_punch(int ch, int *cnt) {
	static int value = 0; // value
	static int state = A_NORM; // current state
	static int temp = 0x0000, attribute = 0x0000; // current attribute

	switch(state) {
		case A_NORM:
			if (ch == 0x1B) {
				state = A_TRANS;
			} else {
				cputchar((attribute & 0xFF00) | ch);
				*cnt++;
			}
			break;
		case A_TRANS:
			if (ch == '[') {
				state = A_ESCAPE;
			} else {
				state = A_NORM;
			}
			break;
		case A_ESCAPE:
			if (ch >= '0' && ch <= '9') {
				value = value * 10 + ch - '0';
			} else if (ch == ';' || ch == 'm') {
				if (value == 0) {
					temp  = colormap[0];
				} else if (value == 5) {
					temp |= 0x8000;
				} else if (value >= 30 && value <= 38) {
					temp |= colormap[value - 30] & 0x0700;
				} else if (value >= 40 && value <= 48) {
					temp |= colormap[value - 40] & 0x7000;
				}
				value = 0;
				if (ch == 'm') {
					attribute = temp;
					temp = 0x0000;
					state = A_NORM;
				}
			} else {
				state = A_NORM;
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
