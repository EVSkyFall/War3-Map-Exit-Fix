#pragma once
#include <windows.h>
#include "MinHook.h"
struct MenuCursorSites {
    BYTE* lookup;
    BYTE* fallback;
    void* refresh;
    void* bind;
    void* boundSprite;
    void** activeRoot;
    void** glueRoot;
};
bool FindMenuCursorSites(BYTE* game,MenuCursorSites* output);
bool InstallMenuCursor(BYTE* game,void* volatile* manager,MH_STATUS* status);
int UpdateMenuCursor();
