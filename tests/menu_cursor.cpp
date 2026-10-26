#include "menu_cursor.h"
#include "post_game.h"
#include "guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static BYTE glue[0x200],cursor[0x200],otherRoot[0x200],sprites[16][0x80],currentCategory[64],defaultCategory[64],node[64];
static void* cursorSprite;
static unsigned created,loaded,bound,ticks;
static bool failLoad,currentHasKey=true;
static const char* lastModel;
static const char customModel[]="war3mapImported\\custom-cursor.mdx";
static const char baseModel[]="UI\\Cursor\\HumanCursor.mdl";
static int modelResource,resourceContext,customSkin;

static int __fastcall HasCustom(void*,void*,const char*,const char*){return 1;}
static const char* __fastcall Custom(void*,void*,const char*,const char* key,void*) {
    return !strcmp(key,"Cursor")?customModel:"map-font.ttf";
}
static void* __cdecl Current(){return currentCategory;}
static void* __fastcall Category(const char* name,void*) {return !strcmp(name,"Default")?defaultCategory:currentCategory;}
static void* __fastcall FindKey(void* table,void*,const char* key) {
    if(strcmp(key,"Cursor") || (table==currentCategory+0x1c && !currentHasKey))return 0;
    *(const char**)(node+0x1c)=baseModel;return node;
}
static void* __fastcall Create(DWORD value,void*) {CHECK(value==1 && created<16);return sprites[created++];}
static void* __cdecl Resources(){return &resourceContext;}
static void __fastcall Load(void* sprite,const char* path,void* context,DWORD a,DWORD b,DWORD c) {
    CHECK(context==&resourceContext && a==0 && b==0 && c==0);
    lastModel=path;++loaded;
    *(void**)((BYTE*)sprite+0x20)=path && !strcmp(path,baseModel) && !failLoad?&modelResource:0;
    failLoad=false;
}
static float __fastcall Animate(void*,DWORD* offset,DWORD mode,DWORD repeat) {CHECK(*offset==0 && mode==1 && repeat==0);return 0.0f;}
static void __fastcall Assign(void* frame,void*,void* sprite) {CHECK(frame==glue);*(void**)(glue+0x2c)=sprite;}
static void __fastcall Release(void*,void*) {}
static void __fastcall Bind(void* frame,void*,void* sprite,DWORD index,DWORD flag) {
    CHECK(frame==cursor && index==0xffffffff && flag==0);
    cursorSprite=sprite;*(DWORD*)(cursor+0x174)=1;*(void***)(cursor+0x178)=&cursorSprite;++bound;
}
static void __cdecl Tick(){++ticks;}
static void Hook(void* target,void* replacement){void* ignored=0;CHECK(MH_CreateHook(target,replacement,&ignored)==MH_OK);}

int main(int argc,char** argv) {
    setvbuf(stdout,0,_IONBF,0);CHECK(argc==5);
    BYTE* game=(BYTE*)LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(game);
    BYTE* dz=(BYTE*)LoadLibraryExA(argv[2],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(dz);
    MenuCursorSites cs={};PostGameSites ps={};CHECK(FindMenuCursorSites(game,&cs));CHECK(FindPostGameSites(game,dz,&ps));
    CHECK(cs.lookup==game+0x3761e0 && cs.refresh==game+0x313330 && cs.bind==game+0x11f100);
    CHECK(cs.activeRoot==(void**)(game+0xd0f9a4) && cs.glueRoot==(void**)(game+0xd31824));
    *cs.activeRoot=glue;*cs.glueRoot=glue;*ps.manager=0;*(void**)(glue+0x16c)=cursor;
    *(void**)(game+0xd31f1c)=&customSkin;
    memset(ps.callback,0,24);*(DWORD*)((BYTE*)ps.callback+20)=15;
    *(DWORD*)((BYTE*)ps.queue+ps.queueSizeOffset)=0;
    *(void**)(dz+0x3364c)=(void*)Tick;
    CHECK(MH_Initialize()==MH_OK);
    Hook(game+0x6a3b0,(void*)HasCustom);Hook(game+0x6a2c0,(void*)Custom);
    Hook(game+0x375280,(void*)Current);Hook(game+0x377940,(void*)Category);Hook(game+0x384180,(void*)FindKey);
    Hook(game+0x1d5680,(void*)Create);Hook(game+0x38fce0,(void*)Resources);
    Hook(game+0x1d7040,(void*)Load);Hook(game+0x1d6b40,(void*)Animate);
    Hook(game+0x11f0c0,(void*)Assign);Hook(game+0x95fc0,(void*)Release);Hook(game+0x125de0,(void*)Bind);
    CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    auto refresh=(void(__thiscall*)(void*))cs.refresh;
    auto lookup=(const char*(__fastcall*)(const char*,const char*))cs.lookup;
    auto wrapper=(void(__cdecl*)())(dz+0x185b0);
    CHECK(Guard([&](){refresh(glue);}));
    CHECK(!strcmp(lastModel,customModel) && !*(void**)(sprites[0]+0x20) && bound==0);
    puts("BASELINE native Glue refresh selects map-only cursor while map resource is unavailable.");
    CHECK(LoadLibraryA(argv[4]));CHECK(LoadLibraryA(argv[3]));
    for(unsigned i=0;i<300 && cs.lookup[0]!=0xe9;++i)Sleep(50);
    CHECK(cs.lookup[0]==0xe9 && ps.dispatch[0]==0xe9);
    CHECK(game[0x12135c]==0xe9);
    failLoad=true;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==1 && bound==0 && !strcmp(lastModel,baseModel));
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==2 && bound==1);
    CHECK(cursorSprite==*(void**)(glue+0x2c) && *(void**)((BYTE*)cursorSprite+0x20)==&modelResource);
    unsigned prior=loaded;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==3 && loaded==prior);
    puts("PASS silent model-load failure retries; native bind/getter agree and successful menu refresh is not repeated.");
    CHECK(!strcmp(lookup("ChatFont",0),"map-font.ttf"));
    CHECK(*(void**)(game+0xd31f1c)==&customSkin);
    currentHasKey=false;CHECK(!strcmp(lookup("Cursor",0),baseModel));currentHasKey=true;
    DWORD manager=0;*ps.manager=&manager;*cs.activeRoot=otherRoot;
    CHECK(!strcmp(lookup("Cursor",0),customModel));
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==4 && loaded==prior);
    *ps.manager=0;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==5 && loaded==prior);
    CHECK(!strcmp(lookup("Cursor",0),customModel));
    *cs.activeRoot=glue;
    CHECK(Guard([&](){wrapper();}));CHECK(ticks==6 && loaded==prior+1 && bound==2);
    CHECK(!strcmp(lastModel,baseModel));
    *ps.manager=&manager;*cs.activeRoot=otherRoot;
    CHECK(!strcmp(lookup("Cursor",0),customModel));
    puts("PASS map custom cursor, loading-root isolation, menu restore, next-map custom cursor, other skin keys and native category fallback.");
    puts("PASS original skin resolver, Glue refresh and active cursor binding chain in isolated mappings; model/table APIs are fixtures, no GPU/game initialization.");
    return 0;
}
