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
#pragma once

#include <calico.h>
#include <nds/arm9/console.h>
#include <nds/ndstypes.h>

// console.c
extern PrintConsole *currentConsole;
void consoleCls(char mode);
void consoleClearLine(char mode);
void newRow();

// console-print.c
u16 *consoleFontBgMapAt(const int x, const int y);
u16 *consoleFontBgMapAtCursor(void);
u16 consoleComputeFontBgMapValue(char);
void consolePrintChar(char);

// console-cursor.c
void consoleSaveFbmvUnderCursor(void);
void consoleRestoreFbmvUnderCursor(void);
void consoleMoveCursorX(int dx); // clamps cursorX to windowWidth
void consoleMoveCursorY(int dy); // clamps cursorY to windowHeight
void consoleSetCursorX(int x);
void consoleSetCursorY(int y);
void consoleSetCursorPos(int x, int y);
void consoleSetCursorChar(char);
void consoleStartFlashingCursor(int frequency);
void consoleStopFlashingCursor(void);

// console-esc.c
int consoleParseEscapeSequence(const char *ptr, int len);
