#include "map_exit.h"
#include "guard.h"
#include "MinHook.h"
#include <stdio.h>
#include <string.h>

static void* originalExit;
static void* finishExit;
static HMODULE selfModule;
static wchar_t logPath[MAX_PATH];

static bool Resolve(BYTE* image, MapExitSites* output) {
    IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)image;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    IMAGE_SECTION_HEADER* sections=IMAGE_FIRST_SECTION(nt);
    static const BYTE pattern[]={0x83,0xc7,0x18,0x56,0x8b,0xcf,0xe8,0,0,0,0,
        0x8b,0xf0,0x8b,0xcf,0x56,0xe8,0,0,0,0,0x8b,0x17,0x8b,0xcf,0x56,0xff,0x12,0x5f,0x5e,0xc3};
    static const BYTE teardown[]={0x8b,0x96,0x64,0x01,0,0,0x8b,0x8e,0x60,0x01,0,0,
        0xe8,0,0,0,0,0x8b,0x86,0x60,0x01,0,0,0x85,0xc0};
    BYTE* entry=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const IMAGE_SECTION_HEADER& section=sections[i];
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE) || section.Misc.VirtualSize<sizeof(teardown)) continue;
        BYTE* start=image+section.VirtualAddress;
        for(size_t offset=0;offset<=section.Misc.VirtualSize-sizeof(teardown);++offset) {
            BYTE* p=start+offset;
            if(memcmp(p,teardown,13) || memcmp(p+17,teardown+17,sizeof(teardown)-17)) continue;
            int displacement;memcpy(&displacement,p+13,4);
            BYTE* target=p+17+displacement;
            if(entry && entry!=target) return false;
            entry=target;
        }
    }
    if(!entry) return false;
    BYTE* found=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const IMAGE_SECTION_HEADER& section=sections[i];
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* start=image+section.VirtualAddress;
        size_t size=section.Misc.VirtualSize;
        if(size<sizeof(pattern)) continue;
        for(size_t offset=0;offset<=size-sizeof(pattern);++offset) {
            BYTE* p=start+offset;
            // The same container tail exists elsewhere. Bind this one to the
            // unregister call made by CFrame's name/context teardown.
            if((uintptr_t)p<(uintptr_t)entry || (uintptr_t)p-(uintptr_t)entry>128) continue;
            if(p[0]!=pattern[0]) continue;
            bool match=true;
            for(size_t j=1;j<sizeof(pattern);++j) {
                if((j>=7 && j<=10) || (j>=17 && j<=20)) continue;
                if(p[j]!=pattern[j]) {match=false;break;}
            }
            if(match) {if(found) return false;found=p;}
        }
    }
    if(!found) return false;
    *output={found+11,found+28};
    return true;
}

bool FindMapExitSites(BYTE* image, MapExitSites* output) {
    bool found=false;
    MapExitSites sites={};
    if(!image || !output || !Guard([&](){found=Resolve(image,&sites);}) || !found) return false;
    *output=sites;
    return true;
}

extern "C" __declspec(naked) void MapExitStub() {
    __asm {
        pushfd
        test eax,eax
        jz missing
        popfd
        jmp dword ptr [originalExit]
    missing:
        popfd
        // The lookup found no node. Leave only unregister; CFrame teardown
        // still owns and frees its name after this caller returns.
        jmp dword ptr [finishExit]
    }
}

static void Log(const char* operation, MH_STATUS status) {
    if(!logPath[0]) return;
    FILE* file=0;
    if(_wfopen_s(&file,logPath,L"ab") || !file) return;
    SYSTEMTIME time;GetLocalTime(&time);
    fprintf(file,"%04u-%02u-%02u %02u:%02u:%02u pid=%lu %s %s\r\n",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,
        GetCurrentProcessId(),operation,MH_StatusToString(status));
    fclose(file);
}

static DWORD WINAPI Install(void*) {
    DWORD length=GetModuleFileNameW(selfModule,logPath,MAX_PATH);
    wchar_t* slash=length && length<MAX_PATH?wcsrchr(logPath,L'\\'):0;
    static const wchar_t name[]=L"War3MapExitFix.log";
    if(slash && (size_t)(MAX_PATH-(slash+1-logPath))>=sizeof(name)/sizeof(wchar_t))
        wcscpy_s(slash+1,MAX_PATH-(slash+1-logPath),name);
    else logPath[0]=0;
    unsigned stage=0,lastStage=~0u;
    MH_STATUS lastResult=MH_OK;
    HMODULE game=0,pinned=0;
    MapExitSites sites={};
    for(;;) {
        const char* operation="";
        MH_STATUS result=MH_OK;
        switch(stage) {
        case 0:
            operation="RETRY retain MIX module";
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                (LPCWSTR)&Install,&pinned)) {++stage;continue;}
            break;
        case 1:
            operation="WAIT Game.dll";
            game=GetModuleHandleW(L"Game.dll");
            if(game) {++stage;continue;}
            break;
        case 2:
            operation="RETRY locate frame unregister";
            if(FindMapExitSites((BYTE*)game,&sites)) {finishExit=sites.finish;++stage;continue;}
            break;
        case 3:
            operation="RETRY initialize MinHook";
            result=MH_Initialize();
            if(result==MH_OK) {++stage;continue;}
            break;
        case 4:
            operation="RETRY create frame unregister hook";
            result=MH_CreateHook(sites.hook,(void*)MapExitStub,&originalExit);
            if(result==MH_OK) {++stage;continue;}
            break;
        case 5:
            operation="RETRY enable frame unregister hook";
            result=MH_EnableHook(sites.hook);
            if(result==MH_OK) {Log("INSTALLED missing-frame-registry-entry repair",result);return 0;}
            break;
        }
        if(stage!=lastStage || result!=lastResult) {
            Log(operation,result);lastStage=stage;lastResult=result;
        }
        Sleep(stage<=1?250:5000);
    }
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        selfModule=module;
        HANDLE thread=CreateThread(0,0,Install,0,0,0);
        if(thread) CloseHandle(thread);
    }
    return TRUE;
}
