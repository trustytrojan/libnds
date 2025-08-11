/*---------------------------------------------------------------------------------

Copyright (C) 2005
Michael Noland (joat)
Jason Rogers (dovoto)
Dave Murphy (WinterMute)

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

#include <default_font_bin.h>
#include <nds/arm9/background.h>
#include <nds/arm9/console.h>
#include <nds/arm9/video.h>
#include <nds/debug.h>
#include <nds/memory.h>
#include <nds/ndstypes.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/iosupport.h>

#include "console-priv.h"

const PrintConsole defaultConsole = {
	.font =
		{.gfx = (u16 *)default_font_bin,
		 .pal = 0,
		 .numColors = 0,
		 .bpp = 1,
		 .asciiOffset = 0,
		 .numChars = 256,
		 .convertSingleColor = true},
	.fontBgMap = 0,
	.fontBgGfx = 0,
	.mapBase = 22,
	.gfxBase = 3,
	.bgLayer = 0,
	.bgId = -1,
	.cursorX = 0,
	.cursorY = 0,
	.prevCursorX = 0,
	.prevCursorY = 0,
	.consoleWidth = 32,
	.consoleHeight = 24,
	.windowX = 0,
	.windowY = 0,
	.windowWidth = 32,
	.windowHeight = 24,
	.tabSize = 3,
	.fontCharOffset = 0,
	.fontCurPal = 0,
	.PrintChar = NULL,
	.consoleInitialised = false,
	.loadGraphics = true,
	.bg2Id = -1,
	.fontBg2Map = NULL,
	.fontBg2Gfx = NULL,
	.fontCurPal2 = 0,
	.escBuf = {},
	.escBufLen = 0};

PrintConsole currentCopy;
PrintConsole *currentConsole = &currentCopy;

const PrintConsole *consoleGetDefault(void) {
	return &defaultConsole;
}

void consoleClearScreen(const char mode) {
	PrintConsole *const c = currentConsole;
	const u16 blank = ' ' + c->fontCharOffset - c->font.asciiOffset;
	const u16 blank_main = (15 << 12) | blank;
	const u16 blank_bg2 = (0 << 12) | blank;

	switch (mode) {
	case '[': // if not specified, mode is 0
	case '0': // from cursor to end of screen
		// Clear from cursor to end of line
		consoleClearLine('0');
		// Clear lines below
		for (int y = c->cursorY + 1; y < c->windowHeight; y++) {
			for (int x = 0; x < c->windowWidth; x++) {
				*consoleFontBgMapAt(x, y) = blank_main;
				if (c->bg2Id != -1)
					*consoleFontBg2MapAt(x, y) = blank_bg2;
			}
		}
		break;

	case '1': // from cursor to beginning of screen
		// Clear lines above
		for (int y = 0; y < c->cursorY; y++) {
			for (int x = 0; x < c->windowWidth; x++) {
				*consoleFontBgMapAt(x, y) = blank_main;
				if (c->bg2Id != -1)
					*consoleFontBg2MapAt(x, y) = blank_bg2;
			}
		}
		// Clear from start of line to cursor
		consoleClearLine('1');
		break;

	case '2': // whole screen
		for (int y = 0; y < c->windowHeight; y++) {
			for (int x = 0; x < c->windowWidth; x++) {
				*consoleFontBgMapAt(x, y) = blank_main;
				if (c->bg2Id != -1)
					*consoleFontBg2MapAt(x, y) = blank_bg2;
			}
		}
		c->cursorX = 0;
		c->cursorY = 0;
		break;
	}
}

void consoleClearLine(const char mode) {
	const PrintConsole *const c = currentConsole;
	int line = c->cursorY;

	// \e[K is same as \e[0K: from cursor to end of line
	// so use its parameters as the default.
	int start = c->cursorX;
	int end = c->windowWidth;

	if (mode == '1') {
		// start of line to cursor
		start = 0;
		end = c->cursorX;
	} else if (mode == '2') {
		// whole line
		start = 0;
		end = c->windowWidth;
	}

	// The character offset for ' ' in the font.
	const u16 blank = ' ' + c->fontCharOffset - c->font.asciiOffset;

	for (int i = start; i < end; ++i) {
		// Clear with default colors (fg: white, bg: black) regardless of current palette.
		*consoleFontBgMapAt(i, line) = (15 << 12) | blank;
		if (c->bg2Id != -1)
			*consoleFontBg2MapAt(i, line) = (0 << 12) | blank;
	}
}

ssize_t nocash_write(struct _reent *, void *, const char *const ptr, const size_t len) {
	nocashWrite(ptr, len);
	return len;
}

ssize_t con_write(struct _reent *, void *, const char *const ptr, const size_t len) {
	if (!ptr || len <= 0)
		return -1;

	for (size_t i = 0; i < len; ++i) {
		const char chr = ptr[i];
		if (chr == '\e' || currentConsole->escBufLen > 0)
			consoleUpdateEscapeSequence(chr);
		else
			consolePrintChar(chr);
	}

	return len;
}

// clang-format off
static const devoptab_t
	dotab_stdout = {.name = "con", .write_r = con_write},
	dotab_nocash = {.name = "nocash", .write_r = nocash_write},
	dotab_null = {.name = "null"};
// clang-format on

void consoleLoadFont(PrintConsole *const console) {
	int i;

	u16 *palette = BG_PALETTE_SUB;

	// check which display is being utilized
	if (console->fontBgGfx < BG_GFX_SUB)
		palette = BG_PALETTE;

	if (console->font.bpp == 1) {
		const u8 *in = (const u8 *)console->font.gfx;
		u32 *out = (u32 *)console->fontBgGfx;
		u32 *out2 = (u32 *)console->fontBg2Gfx;
		for (i = 0; i < console->font.numChars * 8; i++) {
			unsigned cur = *in++;

			int j;
			u32 temp = 0;
			for (j = 0; j < 8; j++) {
				temp |= ((cur & 1) * 0xf) << (j * 4);
				cur >>= 1;
			}

			*out++ = temp;
			if (console->bg2Id != -1)
				*out2++ = temp;
		}

		goto _setUpPalette;

	} else if (console->font.bpp == 4) {
		if (!console->font.convertSingleColor) {
			if (console->font.gfx)
				dmaCopy(console->font.gfx, console->fontBgGfx, console->font.numChars * 64 / 2);
			if (console->font.pal)
				dmaCopy(console->font.pal, palette + console->fontCurPal * 16, console->font.numColors * 2);

			console->fontCurPal <<= 12;
		} else {
			for (i = 0; i < console->font.numChars * 16; i++) {
				u16 temp = 0;

				if (console->font.gfx[i] & 0xF)
					temp |= 0xF;
				if (console->font.gfx[i] & 0xF0)
					temp |= 0xF0;
				if (console->font.gfx[i] & 0xF00)
					temp |= 0xF00;
				if (console->font.gfx[i] & 0xF000)
					temp |= 0xF000;

				console->fontBgGfx[i] = temp;
			}

		_setUpPalette:
			// set up the palette for color printing
			palette[1 * 16 - 1] = RGB15(0, 0, 0);	// normal black
			palette[2 * 16 - 1] = RGB15(15, 0, 0);	// normal red
			palette[3 * 16 - 1] = RGB15(0, 15, 0);	// normal green
			palette[4 * 16 - 1] = RGB15(15, 15, 0); // normal yellow

			palette[5 * 16 - 1] = RGB15(0, 0, 15);	 // normal blue
			palette[6 * 16 - 1] = RGB15(15, 0, 15);	 // normal magenta
			palette[7 * 16 - 1] = RGB15(0, 15, 15);	 // normal cyan
			palette[8 * 16 - 1] = RGB15(24, 24, 24); // normal white

			palette[9 * 16 - 1] = RGB15(15, 15, 15); // bright black
			palette[10 * 16 - 1] = RGB15(31, 0, 0);	 // bright red
			palette[11 * 16 - 1] = RGB15(0, 31, 0);	 // bright green
			palette[12 * 16 - 1] = RGB15(31, 31, 0); // bright yellow

			palette[13 * 16 - 1] = RGB15(0, 0, 31);	  // bright blue
			palette[14 * 16 - 1] = RGB15(31, 0, 31);  // bright magenta
			palette[15 * 16 - 1] = RGB15(0, 31, 31);  // bright cyan
			palette[16 * 16 - 1] = RGB15(31, 31, 31); // bright white

			console->fontCurPal = 15 << 12;
			console->fontCurPal2 = 0 << 12;
		}

	} else if (console->font.bpp == 8) {
		console->fontCurPal = 0;

		if (!console->font.convertSingleColor) {
			if (console->font.gfx)
				dmaCopy(console->font.gfx, console->fontBgGfx, console->font.numChars * 64);
			if (console->font.pal)
				dmaCopy(console->font.pal, palette, console->font.numColors * 2);
		} else {
			for (i = 0; i < console->font.numChars * 16; i++) {
				u32 temp = 0;

				if (console->font.gfx[i] & 0xF)
					temp = 255;
				if (console->font.gfx[i] & 0xF0)
					temp |= 255 << 8;
				if (console->font.gfx[i] & 0xF00)
					temp |= 255 << 16;
				if (console->font.gfx[i] & 0xF000)
					temp |= 255 << 24;

				((u32 *)console->fontBgGfx)[i] = temp;
			}

			palette[255] = RGB15(31, 31, 31);
		}
	}

	palette[0] = RGB15(0, 0, 0);
	PrintConsole *tmp = consoleSelect(console);
	consoleClearScreen('2');
	consoleSelect(tmp);
}

PrintConsole *consoleInit(
	PrintConsole *console,
	const int layer,
	const BgType type,
	const BgSize size,
	const int mapBase,
	const int tileBase,
	const bool mainDisplay,
	const bool loadGraphics,
	const bool ansiBgColors) {
	static bool firstConsoleInit = true;

	if (firstConsoleInit) {
		devoptab_list[STD_OUT] = &dotab_stdout;
		devoptab_list[STD_ERR] = &dotab_stdout;
		setbuf(stdout, NULL);
		setbuf(stderr, NULL);
		firstConsoleInit = false;
	}

	if (console)
		currentConsole = console;
	else
		console = currentConsole;

	*currentConsole = defaultConsole;

	if (mainDisplay) {
		console->bgId = bgInit(layer, type, size, mapBase, tileBase);
		console->bg2Id = ansiBgColors ? bgInit(layer + 1, type, size, mapBase + 1, tileBase) : -1;
	} else {
		console->bgId = bgInitSub(layer, type, size, mapBase, tileBase);
		console->bg2Id = ansiBgColors ? bgInitSub(layer + 1, type, size, mapBase + 1, tileBase) : -1;
	}

	console->fontBgGfx = bgGetGfxPtr(console->bgId);
	console->fontBgMap = bgGetMapPtr(console->bgId);

	if (console->bg2Id != -1) {
		console->fontBg2Gfx = bgGetGfxPtr(console->bg2Id);
		console->fontBg2Map = bgGetMapPtr(console->bg2Id);
	}

	console->escBufLen = 0;
	console->consoleInitialised = 1;

	consoleClearScreen('2');

	if (loadGraphics)
		consoleLoadFont(console);

	return currentConsole;
}

PrintConsole *consoleSelect(PrintConsole *const console) {
	PrintConsole *const tmp = currentConsole;
	currentConsole = console;
	return tmp;
}

void consoleSetFont(PrintConsole *console, ConsoleFont *const font) {
	if (!console)
		console = currentConsole;
	console->font = *font;
	consoleLoadFont(console);
}

void consoleDebugInit(const DebugDevice device) {
	int buffertype = _IONBF;

	switch (device) {
	case DebugDevice_NOCASH:
		devoptab_list[STD_ERR] = &dotab_nocash;
		buffertype = _IOLBF;
		break;
	case DebugDevice_CONSOLE:
		devoptab_list[STD_ERR] = &dotab_stdout;
		break;
	case DebugDevice_NULL:
		devoptab_list[STD_ERR] = &dotab_null;
		break;
	}
	setvbuf(stderr, NULL, buffertype, 0);
}

PrintConsole *consoleDemoInit(void) {
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);

	return consoleInit(
		NULL,
		defaultConsole.bgLayer,
		BgType_Text4bpp,
		BgSize_T_256x256,
		defaultConsole.mapBase,
		defaultConsole.gfxBase,
		false,
		true,
		false);
}

void consoleClear(void) {
	iprintf("\x1b[2J");
}

void consoleSetWindow(PrintConsole *console, const int x, const int y, const int width, const int height) {
	if (!console)
		console = currentConsole;
	console->windowWidth = width;
	console->windowHeight = height;
	console->windowX = x;
	console->windowY = y;
	console->cursorX = 0;
	console->cursorY = 0;
}
