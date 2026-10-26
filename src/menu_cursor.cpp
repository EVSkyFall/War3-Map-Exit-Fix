#include "menu_cursor.h"
#include "guard.h"
#include <string.h>

template<size_t N> static BYTE* FindCode(BYTE* image,const short(&pattern)[N]) {
    IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)image;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 0;
    IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return 0;
    IMAGE_SECTION_HEADER* sections=IMAGE_FIRST_SECTION(nt);BYTE* found=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        if(!(sections[i].Characteristics&IMAGE_SCN_MEM_EXECUTE) || sections[i].Misc.VirtualSize<N)continue;
        BYTE* start=image+sections[i].VirtualAddress;
        for(size_t at=0;at<=sections[i].Misc.VirtualSize-N;++at) {
            BYTE* p=start+at;if(p[0]!=pattern[0])continue;bool match=true;
            for(size_t j=1;j<N;++j)if(pattern[j]>=0 && p[j]!=pattern[j]){match=false;break;}
            if(match){if(found)return 0;found=p;}
        }
    }
    return found;
}
static BYTE* CallTarget(BYTE* p){int relative;memcpy(&relative,p+1,4);return p+5+relative;}
static bool Resolve(BYTE* game,MenuCursorSites* result) {
    static const short refreshPattern[]={0x55,0x8b,0xec,0x51,0x56,0x57,0x8b,0xf9,0xb9,1,0,0,0,
        0xe8,-1,-1,-1,-1,0x6a,0,0x6a,0,0x6a,0,0x8b,0xf0,0xe8,-1,-1,-1,-1,0x50,0x33,0xd2,
        0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,0x8b,0xd0,0x8b,0xce,0xe8,-1,-1,-1,-1};
    static const short gluePattern[]={0xa1,-1,-1,-1,-1,0x85,0xc0,0x75,-1,0x50,0x68,-1,-1,-1,-1,
        0x68,-1,-1,-1,-1,0x68,0xac,1,0,0,0xe8,-1,-1,-1,-1};
    static const short bindPattern[]={0x55,0x8b,0xec,0xe8,-1,-1,-1,-1,0x85,0xc0,0x74,0x11,
        0x6a,1,0x8b,0xc8,0xe8,-1,-1,-1,-1,0x8b,0xc8,0x5d,0xe9,-1,-1,-1,-1,0x5d,0xc2,4,0};
    static const short boundPattern[]={0x83,0xb9,0x74,1,0,0,0,0x74,9,
        0x8b,0x81,0x78,1,0,0,0x8b,0,0xc3,0x33,0xc0,0xc3};
    BYTE* refresh=FindCode(game,refreshPattern);BYTE* glue=FindCode(game,gluePattern);
    BYTE* bind=FindCode(game,bindPattern);BYTE* getter=FindCode(game,boundPattern);
    if(!refresh || !glue || !bind || !getter)return false;
    if(strcmp(*(const char**)(refresh+35),"Cursor"))return false;
    BYTE* lookup=CallTarget(refresh+39);BYTE* root=CallTarget(bind+3);
    static const BYTE prefix[]={0x53,0x56,0x57,0x8b,0xf9,0x33,0xf6,0x8b,0x0d};
    if(memcmp(lookup,prefix,sizeof(prefix)) || lookup[17]!=0x74 || root[0]!=0xa1 || root[5]!=0xc3)return false;
    BYTE* fallback=lookup+19+(signed char)lookup[18];
    if(fallback[0]!=0x85 || fallback[1]!=0xdb)return false;
    *result={lookup,fallback,refresh,bind,getter,*(void***)(root+1),*(void***)(glue+1)};
    return true;
}
bool FindMenuCursorSites(BYTE* game,MenuCursorSites* output) {
    MenuCursorSites result={};bool found=false;
    if(!game || !output || !Guard([&](){found=Resolve(game,&result);}) || !found)return false;
    *output=result;return true;
}
static MenuCursorSites sites;
static void* volatile* managerSlot;
static const char* (__fastcall* originalLookup)(const char*,const char*);
static void* baseResume;
static volatile LONG ready=0;
static bool pending=true;

static __declspec(naked) const char* __fastcall BaseSkin(const char*,const char*) {
    __asm {
        push ebx
        push esi
        push edi
        mov edi,ecx
        xor esi,esi
        mov ebx,edx
        jmp dword ptr [baseResume]
    }
}
static const char* __fastcall Lookup(const char* key,const char* category) {
    if(!*managerSlot && *sites.activeRoot && *sites.activeRoot==*sites.glueRoot && key && !strcmp(key,"Cursor"))
        return BaseSkin(key,category);
    return originalLookup(key,category);
}
bool InstallMenuCursor(BYTE* game,void* volatile* manager,MH_STATUS* status) {
    static unsigned stage=0;*status=MH_OK;
    if(stage==0) {
        if(!FindMenuCursorSites(game,&sites))return false;
        managerSlot=manager;baseResume=sites.fallback;++stage;
    }
    if(stage==1) {
        *status=MH_CreateHook(sites.lookup,(void*)Lookup,(void**)&originalLookup);
        if(*status!=MH_OK)return false;
        ++stage;
    }
    if(stage==2) {
        *status=MH_EnableHook(sites.lookup);if(*status!=MH_OK)return false;
        InterlockedExchange(&ready,1);++stage;
    }
    return true;
}
int UpdateMenuCursor() {
    if(!InterlockedCompareExchange(&ready,0,0))return 0;
    if(*managerSlot){pending=true;return 0;}
    void* glue=*sites.glueRoot;
    if(!pending || !glue || *sites.activeRoot!=glue)return 0;
    bool attached=false;
    if(!Guard([&](){
        ((void(__thiscall*)(void*))sites.refresh)(glue);
        void* sprite=*(void**)((BYTE*)glue+0x2c);
        if(!sprite || !*(void**)((BYTE*)sprite+0x20))return;
        // Glue's virtual notification need not propagate its default sprite.
        // Use the native active-cursor binding path and retain its references.
        ((void(__stdcall*)(void*))sites.bind)(sprite);
        void* cursor=*(void**)((BYTE*)glue+0x16c);
        attached=cursor && ((void*(__thiscall*)(void*))sites.boundSprite)(cursor)==sprite;
    }) || !attached)return -1;
    pending=false;return 1;
}
