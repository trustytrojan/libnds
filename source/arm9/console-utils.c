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
#include <nds/arm9/console.h>
#include <nds/ndstypes.h>
#include <stdio.h>

// from console.c
extern PrintConsole *currentConsole;
void consoleCls(char mode);
void consoleClearLine(char mode);

static void updateColorBright(const int param, int *const color, int *const bright) {
	if (param == 0) { // Reset
		*color = 15;  // bright white
		*bright = 0;
	} else if (param == 1) { // Bright/bold
		*bright = 1;
	} else if (param >= 30 && param <= 37) { // fg color
		*color = param - 30;
	} else if (param >= 40 && param <= 47) { // bg color
		*color = param - 40;
	} else if (param >= 90 && param <= 97) { // bright fg color
		*color = param - 90 + 8;
	} else if (param >= 100 && param <= 107) { // bright bg color
		*color = param - 100 + 8;
	} else if (param == 39 || param == 49) { // Default color
		*color = 15;						 // bright white
	}
}

static void consoleParseColor(const char *escapeseq, int escapelen) {
	// Special case: \x1b[m resets attributes
	if (*escapeseq == 'm') {
		currentConsole->fontCurPal = 15 << 12; // Default color (bright white)
		return;
	}

	// truecolor rgb sequences `ESC[38;2;{r};{g};{b}m` will simply be ignored
	if (siscanf(escapeseq, "38;2;%*d;%*d;%*dm") > 0)
		return;

	unsigned id;
	// 256 color format sequence `ESC[38;5;{ID}m`
	if (siscanf(escapeseq, "38;5;%um", &id) > 0) {
		if (id <= 15)
			currentConsole->fontCurPal = id << 12;
		return;
	}

	int color = -1, bright = 0;
	const char *p = escapeseq;
	int consumed;
	int param;

	while ((consumed = siscanf(p, "%d;", &param)) > 0) {
		p += consumed;
		updateColorBright(param, &color, &bright);
	}

	if (siscanf(p, "%dm", &param) > 0)
		updateColorBright(param, &color, &bright);

	int final_param = -1;
	if (color != -1) {
		final_param = color;
		if (bright && final_param < 8)
			final_param += 8;
	} else if (bright) {
		// if only intensity is set, brighten the current color
		int current_color = (currentConsole->fontCurPal >> 12);
		if (current_color < 8)
			final_param = current_color + 8;
		else
			final_param = current_color;
	}

	if (final_param != -1)
		currentConsole->fontCurPal = final_param << 12;
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
			if (sscanf(escapeseq, "%dA", &parameter) < 1)
				parameter = 1;
			currentConsole->cursorY =
				(currentConsole->cursorY - parameter) < 0 ? 0 : currentConsole->cursorY - parameter;
			return escapelen;

		case 'B':
			if (sscanf(escapeseq, "%dB", &parameter) < 1)
				parameter = 1;
			currentConsole->cursorY = (currentConsole->cursorY + parameter) > currentConsole->windowHeight - 1
										  ? currentConsole->windowHeight - 1
										  : currentConsole->cursorY + parameter;
			return escapelen;

		case 'C':
			if (sscanf(escapeseq, "%dC", &parameter) < 1)
				parameter = 1;
			currentConsole->cursorX = (currentConsole->cursorX + parameter) > currentConsole->windowWidth - 1
										  ? currentConsole->windowWidth - 1
										  : currentConsole->cursorX + parameter;
			return escapelen;

		case 'D':
			if (sscanf(escapeseq, "%dD", &parameter) < 1)
				parameter = 1;
			currentConsole->cursorX =
				(currentConsole->cursorX - parameter) < 0 ? 0 : currentConsole->cursorX - parameter;
			return escapelen;

		// Cursor position movement
		case 'H':
		case 'f':
			sscanf(escapeseq, "%d;%d", &currentConsole->cursorY, &currentConsole->cursorX);
			return escapelen;

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
			currentConsole->cursorX = currentConsole->prevCursorX;
			currentConsole->cursorY = currentConsole->prevCursorY;
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
