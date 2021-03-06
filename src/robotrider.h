/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#if NON_UNITY_BUILD
#include "common.h"
#include "memory.h"
#include "world.h"
#include "wfc.h"
#include "editor.h"
#endif


enum class ConsoleEntryType
{
    Empty = 0,
    LogOutput,
    History,
    CommandOutput,
};

#define CONSOLE_LINE_MAXLEN 1024

struct ConsoleEntry
{
    char text[CONSOLE_LINE_MAXLEN];
    ConsoleEntryType type;
};

struct GameConsole
{
    // FIXME This is absurd and we should allocate this as needed in the gameArena
    // TODO Think how to pack this into a fixed size ring buffer for the text,
    // maybe indexed by a fixed sized ring buffer of ConsoleEntry's with a String in it
    ConsoleEntry entries[4096];
    char inputBuffer[CONSOLE_LINE_MAXLEN];

    i32 entryCount;
    i32 nextEntryIndex;
    bool scrollToBottom;
};


struct GameState
{
    MemoryArena worldArena;
    MemoryArena transientArena;

    World *world;

#if !RELEASE
    EditorState DEBUGeditorState;
#endif

    GameConsole gameConsole;
};


// This is for generated stuff that can be rebuilt on the spot if necessary
struct TransientState
{

};


#if !RELEASE
extern bool* DEBUGglobalQuit;
#endif

#endif /* __ROBOTRIDER_H__ */
