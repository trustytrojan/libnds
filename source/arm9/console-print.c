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

u16 *consoleFontBgMapAt(const int x, const int y) {
	const PrintConsole *const c = currentConsole;
	const int xOffset = x + c->windowX;
	const int yOffset = (y + c->windowY) * c->consoleWidth;
	return c->fontBgMap + xOffset + yOffset;
}

u16 *consoleFontBg2MapAt(const int x, const int y) {
	const PrintConsole *const c = currentConsole;
	const int xOffset = x + c->windowX;
	const int yOffset = (y + c->windowY) * c->consoleWidth;
	return c->fontBg2Map + xOffset + yOffset;
}

u16 *consoleFontBgMapAtCursor(void) {
	const PrintConsole *const c = currentConsole;
	return consoleFontBgMapAt(c->cursorX, c->cursorY);
}

u16 *consoleFontBg2MapAtCursor(void) {
	const PrintConsole *const c = currentConsole;
	return consoleFontBg2MapAt(c->cursorX, c->cursorY);
}

u16 consoleComputeFontBgMapValue(const char ch) {
	const PrintConsole *const c = currentConsole;
	return c->fontCurPal | (u16)(ch + c->fontCharOffset - c->font.asciiOffset);
}

u16 consoleComputeFontBg2MapValue(const char ch) {
	const PrintConsole *const c = currentConsole;
	return c->fontCurPal2 | (u16)(ch + c->fontCharOffset - c->font.asciiOffset);
}

void consoleCommitChar(const char ch) {
	PrintConsole *const c = currentConsole;

	*consoleFontBgMapAtCursor() = consoleComputeFontBgMapValue(ch); // fg
	if (c->bg2Id != -1)
		*consoleFontBg2MapAtCursor() = consoleComputeFontBg2MapValue(219); // bg

	++c->cursorX;

	if (c->bg2Id != -1) {
		consoleSaveTileUnderCursor();
		consoleDrawCursor();
	}
}

// could have a better name, since we aren't always printing a character
void consolePrintChar(const char ch) {
	if (!ch)
		return;

	PrintConsole *const c = currentConsole;

	if (!c->fontBgMap)
		return;

	if (ch + c->fontCharOffset - c->font.asciiOffset > c->font.numChars)
		// this prevents weird stuff showing up when printing!
		return;

	if (c->PrintChar && c->PrintChar(c, ch))
		return;

	if (c->cursorX >= c->windowWidth) {
		if (currentConsole->bg2Id == -1)
			c->cursorX = 0;
		else
			consoleSetCursorX(0);
		newRow();
	}

	switch (ch) {
	case '\a':
		// bell character: TODO: add a callback for applications to respond to this. e.g. playing a bell sound
		break;

	case '\b':
		/*
			the old code here actually moved the cursor back one (and up one row
			if needed) and then "erased" the character by writing a space to fontBgMap.

			in a linux terminal emulator with the termios attr ICANON off,
			backspace does NOT move the cursor. if the ECHO termios attr is on
			it prints the \b character (visualized as ^?).

			with ICANON and ECHO on, backspace does NOT wrap the cursor around to the
			last row.

			what dkp was trying to do here is emulate "canonical mode"
			(ICANON termios attr), which tells the terminal to line-buffer
			input from the keyboard before sending it to stdin. but it ALSO
			does what WE (the console) were doing here before: erasing the current character
			and moving the cursor back. THIS SHOULD BE KEYBOARD.C's JOB!

			SOLUTION:
			keyboard.c should have both an "echo" and "line buffer" option.
			when keyboard.c's "line buffering" is on, **it should interface with the
			currently selected console** to erase the character at the cursor and then move
			the cursor back. when "echo" is on, keyboard.c should send what it gets to the
			console to be ***immediately*** rendered, with non-visual characters visualized.
			for example, left arrow becomes ^[[D.

			the responsibilities should NOT be mixed together.
		*/

		if (c->echo)
			// *consoleFontBgMapAtCursor() = consoleComputeFontBgMapValue('\b');
			consoleCommitChar('\b');
		break;

	case '\t':
		if (currentConsole->bg2Id == -1)
			c->cursorX += c->tabSize - ((c->cursorX) % (c->tabSize));
		else
			consoleMoveCursorX(c->tabSize - ((c->cursorX) % (c->tabSize)));
		break;

	case '\n':
		newRow();
		// also return to first column:

	case '\r':
		if (currentConsole->bg2Id == -1)
			c->cursorX = 0;
		else
			consoleSetCursorX(0);
		break;

	default:
		consoleCommitChar(ch);
	}
}
