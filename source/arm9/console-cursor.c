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

static u16 fbmvUnderCursor, cursorFbmv;
static bool isCursorShown = true;

void consoleSetCursorChar(const char c) {
	cursorFbmv = consoleComputeFontBgMapValue(c);
}

void consoleSaveFbmvUnderCursor(void) {
	if (!isCursorShown)
		fbmvUnderCursor = *consoleFontBgMapAtCursor();
}

void consoleRestoreFbmvUnderCursor(void) {
	*consoleFontBgMapAtCursor() = fbmvUnderCursor;
}

static void consoleFlashCursor(TickTask *const _) {
	u16 *const fbmAtCursor = consoleFontBgMapAtCursor();

	if (isCursorShown) {
		// "on" state, save the character under the cursor and display the cursor
		fbmvUnderCursor = *fbmAtCursor;
		*fbmAtCursor = cursorFbmv;
	} else {
		// "off" state, restore the character under the cursor
		*fbmAtCursor = fbmvUnderCursor;
	}

	isCursorShown = !isCursorShown;
}

static TickTask cursorFlashTickTask;

void consoleStartFlashingCursor(const int frequency) {
	tickTaskStart(&cursorFlashTickTask, consoleFlashCursor, ticksFromHz(frequency), ticksFromHz(frequency));
}

void consoleStopFlashingCursor(void) {
	tickTaskStop(&cursorFlashTickTask);
}

void consoleSetCursorPos(const int x, const int y) {
	consoleRestoreFbmvUnderCursor();
	currentConsole->cursorX = x;
	currentConsole->cursorY = y;
	consoleSaveFbmvUnderCursor();
}

void consoleSetCursorY(const int y) {
	consoleSetCursorPos(currentConsole->cursorX, y);
}

void consoleSetCursorX(const int x) {
	consoleSetCursorPos(x, currentConsole->cursorY);
}

void consoleMoveCursorX(const int dx) {
	const PrintConsole *const c = currentConsole;
	const int newX = c->cursorX + dx;
	const int maxX = c->windowWidth - 1;
	consoleSetCursorX((newX > maxX) ? maxX : newX);
}

void consoleMoveCursorY(const int dy) {
	const PrintConsole *const c = currentConsole;
	const int newY = c->cursorY + dy;
	const int maxY = c->windowHeight - 1;
	consoleSetCursorY((newY > maxY) ? maxY : newY);
}
