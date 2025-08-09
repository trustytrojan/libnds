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

// These save the palette and character of whatever tile the cursor moves on top of.
// fb2mv should always contain the full block ASCII character (219).
static u16 fbmvUnderCursor, fb2mvUnderCursor;

void consoleSaveTileUnderCursor(void) {
	fbmvUnderCursor = *consoleFontBgMapAtCursor();
	fb2mvUnderCursor = *consoleFontBg2MapAtCursor();
}

void consoleRestoreTileUnderCursor(void) {
	*consoleFontBgMapAtCursor() = fbmvUnderCursor;
	*consoleFontBg2MapAtCursor() = fb2mvUnderCursor;
}

void consoleDrawCursor(void) {
	// foreground: this is the character itself. we need to turn it black,
	// which would usually be a fontCurPal of (0 << 12). however fbmvUnderCursor
	// is not being computed on the fly (like below), so 99% of the time it 
	// does not have those bits zeroed inside. (who uses black on black?)
	// so we need to zero-out those bits to make it black:
	*consoleFontBgMapAtCursor() = 0x0fff & fbmvUnderCursor;

	// background: just use bright write (15)
	*consoleFontBg2MapAtCursor() = (15 << 12) | consoleComputeFontBg2MapValue(219);
}

void consoleSetCursorPos(const int x, const int y) {
	if (currentConsole->bg2Id != -1)
		consoleRestoreTileUnderCursor();

	currentConsole->cursorX = x;
	currentConsole->cursorY = y;

	if (currentConsole->bg2Id != -1) {
		consoleSaveTileUnderCursor();

		// original tile saved! now let's make the cursor visible with
		// white background and black foreground.
		consoleDrawCursor();
	}
}

void consoleSetCursorY(const int y) {
	consoleSetCursorPos(currentConsole->cursorX, y);
}

void consoleSetCursorX(const int x) {
	consoleSetCursorPos(x, currentConsole->cursorY);
}

int clamp(const int n, const int min, const int max) {
	if (n < min)
		return min;
	if (n > max)
		return max;
	return n;
}

void consoleMoveCursorX(const int dx) {
	const PrintConsole *const c = currentConsole;
	const int newX = c->cursorX + dx;
	const int maxX = c->windowWidth - 1;
	consoleSetCursorX(clamp(newX, 0, maxX));
}

void consoleMoveCursorY(const int dy) {
	const PrintConsole *const c = currentConsole;
	const int newY = c->cursorY + dy;
	const int maxY = c->windowHeight - 1;
	consoleSetCursorY(clamp(newY, 0, maxY));
}
