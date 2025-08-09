/*---------------------------------------------------------------------------------

Copyright (C) 2025
trustytrojan

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.
2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.
3.	This notice may not be removed or altered from any source
distribution.

---------------------------------------------------------------------------------*/
#include "console-priv.h"
#include <stdio.h>

/*
TODO, if it becomes important enough:

don't use the scanf strategy after all, because incomplete sequences written
to consoles will just be shown raw. to do what every modern terminal (emulator) does,
we need to implement a state machine with a buffer.

as an example:
run `cat`, then press the Esc and Enter keys on your keyboard. this does three things:
1. '\e' is added to cat's input buffer
2. your terminal reacts to Enter and moves the cursor to the next line
3. cat sends its input buffer (containing "\e\n" in C string format) to stdout,
   causing your terminal to:
   1. recognize the start of an escape sequence (\e)
   2. move the cursor to the next line (again) (\n)
then type "[36m" without the quotes. on my terminal emulator (foot), the "[36m"
was never shown, and everything i typed afterwards was cyan in color. my terminal
emulator let the \n go through without printing the \e, meaning it did in fact
store the \e in a buffer, or set a flag indicating that "i should check for the rest
of the escape sequence", then processed the "[36m", setting the color to cyan.

we should recreate this strategy here as well to handle cases where not all of an
escape sequence is passed to a single con_write() call.

not to mention: "\e[" is just one type of sequence (known as a control sequence introducer,
or CSI), but there are also "\e " sequences as well which can do other things. a state
machine would help here.
*/

static void updateColorBright(const int param, int *const color, int *const bgcolor, int *const bright) {
	if (param == 0) { // Reset
		*color = 15;  // fg: bright white
		*bgcolor = 0; // bg: black
		*bright = 0;
	} else if (param == 1) { // Bright/bold
		*bright = 1;
	} else if (param >= 30 && param <= 37) { // fg color
		*color = param - 30;
	} else if (param >= 40 && param <= 47) { // bg color
		*bgcolor = param - 40;
	} else if (param >= 90 && param <= 97) { // bright fg color
		*color = param - 90 + 8;
	} else if (param >= 100 && param <= 107) { // bright bg color
		*bgcolor = param - 100 + 8;
	} else if (param == 39 || param == 49) { // Default color
		*color = 15;						 // fg: bright white
		*bgcolor = 0;						 // bg: black
	}
}

static void consoleParseColor(const char *escapeseq, int escapelen) {
	if (!currentConsole)
		return;

	// Special case: \x1b[m resets attributes
	if (*escapeseq == 'm') {
		currentConsole->fontCurPal = 15 << 12; // Default color (bright white)
		currentConsole->fontCurPal2 = 0;	   // black bg
		return;
	}

	// ignore truecolor rgb sequences in the form `ESC[38;2;{r};{g};{b}m`
	if (siscanf(escapeseq, "38;2;%*d;%*d;%*dm") > 0)
		return;

	unsigned id;
	// 256 color format sequence: `ESC[38;5;{ID}m`
	// support it, but just use 0-15, ignore everything else.
	if (siscanf(escapeseq, "38;5;%um", &id) > 0) {
		if (id <= 15)
			currentConsole->fontCurPal = id << 12;
		return;
	}

	// -1 color means unchanged
	int color = -1, bgcolor = -1, bright = 0, items_matched, chars_consumed, param;
	const char *p = escapeseq;

	// %n does not match anything, it stores the number of characters
	// consumed thus far into the next pointer! use this to advance `p`.

	// start consuming arguments, delimited with ';'
	while (true) {
		// Try to parse a parameter followed by a semicolon
		items_matched = siscanf(p, "%d;%n", &param, &chars_consumed);
		if (items_matched > 0) {
			updateColorBright(param, &color, &bgcolor, &bright);
			p += chars_consumed;
			continue;
		}

		// If that failed, try to parse the final parameter ending in 'm'
		items_matched = siscanf(p, "%dm", &param);
		if (items_matched > 0)
			updateColorBright(param, &color, &bgcolor, &bright);

		// End of sequence
		break;
	}

	// handle cases when only bold (1) modifier is used. this only affects foreground.
	// this should simply turn the current color into its bright variant.
	int final_color = -1;
	if (color != -1) {
		final_color = color;
		if (bright && final_color < 8)
			final_color += 8;
	} else if (bright) {
		// if only intensity is set, brighten the current color
		int current_color = (currentConsole->fontCurPal >> 12);
		if (current_color < 8)
			final_color = current_color + 8;
		else
			final_color = current_color;
	}

	// final_param is a 4-bit integer (0-15). this is placed into the
	// last 4 bits of fontCurPal, which is |'d with the character offset.
	// this ALSO means we can extract it out of an FBMV by >>'ing 12.
	// see consoleComputeFontBgMapValue() for reference.

	if (final_color != -1)
		currentConsole->fontCurPal = final_color << 12;
	if (bgcolor != -1)
		currentConsole->fontCurPal2 = bgcolor << 12;
}

int consoleParseEscapeSequence(const char *ptr, int len) {
	char chr;
	const char *escapeseq = ptr;
	int escapelen = 0;
	int parameter;

	do {
		chr = *(ptr++);
		escapelen++;

		switch (chr) {
		// Private modes - commonly end in 'h' or 'l'. We won't implement any (yet), so just consume them.
		case 'h':
		case 'l':
			return escapelen;

		// Cursor directional movement
		case 'A':
			if (siscanf(escapeseq, "%dA", &parameter) < 1)
				parameter = 1;
			consoleMoveCursorY(-parameter);
			return escapelen;

		case 'B':
			if (siscanf(escapeseq, "%dB", &parameter) < 1)
				parameter = 1;
			consoleMoveCursorY(parameter);
			return escapelen;

		case 'C':
			if (siscanf(escapeseq, "%dC", &parameter) < 1)
				parameter = 1;
			consoleMoveCursorX(parameter);
			return escapelen;

		case 'D':
			if (siscanf(escapeseq, "%dD", &parameter) < 1)
				parameter = 1;
			consoleMoveCursorX(-parameter);
			return escapelen;

		// Cursor position movement
		case 'H':
		case 'f': {
			int x, y;
			if (siscanf(escapeseq, "%d;%d", &y, &x) == 2)
				consoleSetCursorPos(x, y);
			else
				consoleSetCursorPos(0, 0);
			return escapelen;
		}

		// Screen clear
		case 'J':
			consoleCls(escapeseq[escapelen - 2]);
			return escapelen;

		// Line clear
		case 'K':
			consoleClearLine(escapeseq[escapelen - 2]);
			return escapelen;

		// Save cursor position
		case 's':
			currentConsole->prevCursorX = currentConsole->cursorX;
			currentConsole->prevCursorY = currentConsole->cursorY;
			return escapelen;

		// Load cursor position
		case 'u':
			// currentConsole->cursorX = currentConsole->prevCursorX;
			// currentConsole->cursorY = currentConsole->prevCursorY;
			consoleSetCursorPos(currentConsole->prevCursorX, currentConsole->prevCursorY);
			return escapelen;

		// Color/style modes
		case 'm':
			consoleParseColor(escapeseq, escapelen);
			return escapelen;
		}
	} while (escapelen < len);

	// reached end of buffer! tell con_write to NOT add to its counter.
	return 0;
}
