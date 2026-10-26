#include "map_exit.h"
#include "guard.h"
#include "MinHook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static DWORD node[6],nextNode[6],previous;
static BYTE contextNode[80],frame[0x168];
static DWORD bucket[3],vtable[1];
static char frameName[]="name";
static void* lookupResult;
static unsigned lookups,nodeFrees,nameFrees,baseDestroys;
static void* unregisterEntry;
static DWORD beforeEsp,afterEsp,afterEbx,afterEsi,afterEdi;

static void* __fastcall Lookup(void* table,void*,const char* name) {
    CHECK(table==contextNode+0x18 && !strcmp(name,frameName));
    ++lookups;return lookupResult;
}
static void __fastcall FreeNode(void* table,void*,void* item) {
    CHECK(table==contextNode+0x18 && item==node);
    CHECK(node[1]==0 && node[2]==0);
    ++nodeFrees;
}
static void __fastcall BaseDestroy(void*,void*,DWORD) {++baseDestroys;}
static void __stdcall FreeName(void* name,const char*,DWORD,DWORD) {
    CHECK(name==frameName);++nameFrees;
}
static void __declspec(naked) Invoke() {
    __asm {
        push ebx
        push esi
        push edi
        mov ebx,12345678h
        mov esi,23456789h
        mov edi,3456789ah
        mov beforeEsp,esp
        lea ecx,frameName
        xor edx,edx
        call dword ptr [unregisterEntry]
        mov afterEsp,esp
        mov afterEbx,ebx
        mov afterEsi,esi
        mov afterEdi,edi
        pop edi
        pop esi
        pop ebx
        ret
    }
}

int main(int argc,char** argv) {
    setvbuf(stdout,0,_IONBF,0);
    CHECK(argc==3);
    HMODULE game=LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(game);
    BYTE* image=(BYTE*)game;
    MapExitSites sites={};CHECK(FindMapExitSites(image,&sites));
    // This bounded replay uses the separately verified 1.28.5 diagnostic layout.
    CHECK(sites.hook==image+0x12135c && sites.finish==image+0x12136d);
    unregisterEntry=image+0x121310;
    DWORD* mask=*(DWORD**)(image+0x12131d);
    DWORD** buckets=*(DWORD***)(image+0x12132d);
    *mask=0;*buckets=bucket;bucket[2]=(DWORD)contextNode;
    *(DWORD*)(contextNode+0x18)=(DWORD)vtable;vtable[0]=(DWORD)&FreeNode;
    void* ignored=0;
    CHECK(MH_Initialize()==MH_OK);
    CHECK(MH_CreateHook(image+0x122580,(void*)Lookup,&ignored)==MH_OK);
    CHECK(MH_CreateHook(image+0x11b300,(void*)BaseDestroy,&ignored)==MH_OK);
    CHECK(MH_CreateHook(image+0xa5f88,(void*)FreeName,&ignored)==MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    typedef void(__fastcall* Unregister)(const char*,DWORD);
    typedef void(__thiscall* Destroy)(void*,DWORD);
    Unregister unregisterFrame=(Unregister)unregisterEntry;
    Destroy destroy=(Destroy)(image+0x123c20);
    CHECK(!Guard([&](){unregisterFrame(frameName,0);}));
    CHECK(lookups==1 && nodeFrees==0);
    puts("BASELINE reproduced access violation from missing frame registration.");
    CHECK(LoadLibraryA(argv[2]));
    for(unsigned i=0;i<300 && sites.hook[0]!=0xe9;++i)Sleep(50);
    CHECK(sites.hook[0]==0xe9);
    bool continued=false;
    CHECK(Guard([&](){Invoke();continued=true;}));
    CHECK(continued && nodeFrees==0 && beforeEsp==afterEsp);
    CHECK(afterEbx==0x12345678 && afterEsi==0x23456789 && afterEdi==0x3456789a);
    puts("PASS missing entry returns; ESP and EBX/ESI/EDI preserved.");
    unsigned calls=lookups;
    CHECK(Guard([&](){unregisterFrame("",0);unregisterFrame(0,0);unregisterFrame(frameName,1);}));
    CHECK(lookups==calls);
    puts("PASS empty name and absent context preserve existing behavior.");
    *(DWORD*)(frame+0x160)=(DWORD)frameName;
    CHECK(Guard([&](){destroy(frame,0);}));
    CHECK(nameFrees==1 && baseDestroys==1 && *(DWORD*)(frame+0x160)==0 && nodeFrees==0);
    puts("PASS actual outer CFrame teardown continues and releases/clears name.");
    node[0]=0x13579bdf;node[5]=0x2468ace0;nextNode[0]=0x11223344;nextNode[5]=0x55667788;
    previous=(DWORD)node;node[1]=(DWORD)nextNode;node[2]=~(DWORD)&previous;nextNode[1]=(DWORD)node;
    lookupResult=node;*(DWORD*)(frame+0x160)=(DWORD)frameName;
    CHECK(Guard([&](){destroy(frame,0);}));
    CHECK(nodeFrees==1 && nameFrees==2 && baseDestroys==2 && *(DWORD*)(frame+0x160)==0);
    CHECK(previous==(DWORD)nextNode && nextNode[1]==~(DWORD)&previous);
    CHECK(node[0]==0x13579bdf && node[5]==0x2468ace0 && nextNode[0]==0x11223344 && nextNode[5]==0x55667788);
    puts("PASS existing entry unlinks/frees once; neighbor sentinels preserved.");
    puts("PASS actual MIX and original game functions; game initialization was not executed.");
    return 0;
}
