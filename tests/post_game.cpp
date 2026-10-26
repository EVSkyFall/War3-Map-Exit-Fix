#include "post_game.h"
#include "guard.h"
#include "MinHook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static PostGameSites sites;
static unsigned ticks,queries,executed,popped,queueCount,frontIndex;
static bool failPop;
static BYTE events[4][64];
static char names[16][16];
static DWORD (__thiscall* nativeGetter)(void*);
static void InitString(void* object,const char* value) {
    memset(object,0,24);strcpy((char*)object,value);
    *(DWORD*)((BYTE*)object+16)=(DWORD)strlen(value);
    *(DWORD*)((BYTE*)object+20)=15;
}
static DWORD __cdecl LocalPlayerId() {
    ++queries;
    if(!*sites.manager)return nativeGetter(0);
    return 5;
}
static void __cdecl RunScript(const char* name) {
    CHECK(executed<16);strcpy(names[executed++],name);
}
static bool __fastcall QueueEmpty(void* queue,void*) {CHECK(queue==sites.queue);return queueCount==0;}
static void* __fastcall QueueFront(void* queue,void*) {CHECK(queue==sites.queue && queueCount);return events[frontIndex];}
static void __fastcall QueuePop(void* queue,void*) {
    CHECK(queue==sites.queue && queueCount);
    if(failPop){failPop=false;RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);}
    --queueCount;++frontIndex;++popped;*(DWORD*)((BYTE*)sites.queue+16)=queueCount;
}
static void SeedQueue(const char* name,unsigned count) {
    frontIndex=0;queueCount=count;
    *(DWORD*)((BYTE*)sites.queue+16)=queueCount;
    for(unsigned i=0;i<count;++i){memset(events[i],0,64);InitString(events[i]+4,name);*(DWORD*)(events[i]+44)=5;}
}
static void __cdecl OriginalTick() {++ticks;}
static void Hook(void* entry,void* replacement) {
    void* ignored=0;CHECK(MH_CreateHook(entry,replacement,&ignored)==MH_OK);
}

int main(int argc,char** argv) {
    setvbuf(stdout,0,_IONBF,0);CHECK(argc==5 || argc==6);
    HMODULE game=LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(game);
    HMODULE dz=LoadLibraryExA(argv[2],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(dz);
    BYTE* g=(BYTE*)game;BYTE* d=(BYTE*)dz;
    CHECK(FindPostGameSites(g,d,&sites));
    // Fixed addresses only identify this previously hashed offline fixture.
    CHECK(sites.dispatch==d+0x18510 && sites.manager==(void**)(g+0xd305e0));
    CHECK(sites.clearCallback==d+0x1bc40 && sites.queuePop==d+0x1be50);
    nativeGetter=(DWORD(__thiscall*)(void*))(g+0x2135f0);
    *sites.manager=0;
    InitString(sites.callback,"old");SeedQueue("old-event",2);
    *(void**)(d+0x3364c)=(void*)OriginalTick;
    CHECK(MH_Initialize()==MH_OK);
    Hook(d+0x15fa0,(void*)LocalPlayerId);
    Hook(d+0xcbb0,(void*)RunScript);
    Hook(sites.queueEmpty,(void*)QueueEmpty);
    Hook(d+0x1be10,(void*)QueueFront);
    Hook(sites.queuePop,(void*)QueuePop);
    CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    void(__cdecl* wrapper)()=(void(__cdecl*)())(d+0x185b0);
    CHECK(!Guard([&](){wrapper();}));
    CHECK(queries==1 && executed==0 && ticks==0);
    puts("BASELINE original dz wrapper reaches native null-manager access violation.");
    CHECK(LoadLibraryA(argv[4]));
    for(unsigned i=0;i<300 && g[0x12135c]!=0xe9;++i)Sleep(50);
    CHECK(g[0x12135c]==0xe9);
    CHECK(LoadLibraryA(argv[3]));
    for(unsigned i=0;i<300 && sites.dispatch[0]!=0xe9;++i)Sleep(50);
    CHECK(sites.dispatch[0]==0xe9);
    CHECK(Guard([&](){wrapper();}));
    CHECK(ticks==1 && queries==1 && executed==0 && queueCount==2 && !strcmp((char*)sites.callback,"old"));
    puts("PASS initial menu: original tick continues, upcoming registrations preserved.");
    DWORD manager=0;*sites.manager=&manager;
    CHECK(Guard([&](){wrapper();}));
    CHECK(ticks==2 && queries==2 && executed==3 && queueCount==0 && popped==2);
    CHECK(!strcmp(names[0],"old") && !strcmp(names[1],"old-event") && !strcmp(names[2],"old-event"));
    SeedQueue("stale",2);*sites.manager=0;
    CHECK(Guard([&](){wrapper();}));
    CHECK(ticks==3 && queries==2 && executed==3 && queueCount==0 && popped==4);
    CHECK(*(DWORD*)((BYTE*)sites.callback+16)==0 && *(char*)sites.callback==0);
    CHECK(*(DWORD*)(d+0x33640)==0xffffffff && *(DWORD*)(d+0x3363c)==0);
    puts("PASS map exit clears actual callback string, drains modeled queue, resets context; tick continues.");
    InitString(sites.callback,"new");SeedQueue("new-event",1);
    CHECK(Guard([&](){wrapper();}));
    CHECK(ticks==4 && queries==2 && executed==3 && queueCount==1 && popped==4);
    CHECK(!strcmp((char*)sites.callback,"new"));
    *sites.manager=&manager;
    CHECK(Guard([&](){wrapper();}));
    CHECK(ticks==5 && queries==3 && executed==5 && queueCount==0 && popped==5);
    CHECK(!strcmp(names[3],"new") && !strcmp(names[4],"new-event"));
    puts("PASS next map executes only new callback/events; no permanent disable.");
    SeedQueue("fault-event",1);*sites.manager=0;failPop=true;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==6 && queueCount==1 && executed==5);
    InitString(sites.callback,"after-fault");
    InitString(events[1]+4,"fresh-event");*(DWORD*)(events[1]+44)=5;
    queueCount=2;*(DWORD*)((BYTE*)sites.queue+16)=queueCount;
    bool activeRetry=argc==6 && !strcmp(argv[5],"active-retry");
    if(activeRetry)*sites.manager=&manager;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==7);
    CHECK(!strcmp((char*)sites.callback,"after-fault"));
    if(!activeRetry) {
        CHECK(queueCount==1 && executed==5 && popped==6);
        *sites.manager=&manager;
        CHECK(Guard([&](){wrapper();}));CHECK(ticks==8);
    }
    CHECK(queueCount==0 && executed==7 && popped==7);
    CHECK(!strcmp(names[5],"after-fault") && !strcmp(names[6],"fresh-event"));
    puts("PASS cleanup retry preserves new registration/events and retires old work before next map.");
    CHECK(g[0x12135c]==0xe9);
    puts("PASS both map-exit MIX hooks coexist in isolated mappings; no game initialization.");
    return 0;
}
