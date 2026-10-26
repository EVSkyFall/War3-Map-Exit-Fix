#pragma once
#include <windows.h>
struct PostGameSites {
    BYTE* dispatch;
    void* volatile* manager;
    void* callback;
    void* queue;
    void* clearCallback;
    void* queueEmpty;
    void* queuePop;
    void* resetContext;
    unsigned queueSizeOffset;
};
bool FindPostGameSites(BYTE* game,BYTE* dz,PostGameSites* output);
