#include "post_game.h"
#include "guard.h"
#include "MinHook.h"
#include "menu_cursor.h"
#include <stdio.h>
#include <string.h>

template<size_t N> static BYTE* FindCode(BYTE* image,const short(&pattern)[N],unsigned operand=0,void* expected=0,unsigned sharedOperand=0) {
    IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)image;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return 0;
    IMAGE_SECTION_HEADER* sections=IMAGE_FIRST_SECTION(nt);
    BYTE* found=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const IMAGE_SECTION_HEADER& section=sections[i];
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE) || section.Misc.VirtualSize<N)continue;
        BYTE* start=image+section.VirtualAddress;
        for(size_t offset=0;offset<=section.Misc.VirtualSize-N;++offset) {
            BYTE* p=start+offset;
            if(p[0]!=pattern[0])continue;
            bool match=true;
            for(size_t j=1;j<N;++j)if(pattern[j]>=0 && p[j]!=pattern[j]){match=false;break;}
            if(match && (!operand || *(void**)(p+operand)==expected)) {
                if(found && (!sharedOperand || *(void**)(found+sharedOperand)!=*(void**)(p+sharedOperand)))return 0;
                found=p;
            }
        }
    }
    return found;
}
static void* CallTarget(BYTE* call) {int relative;memcpy(&relative,call+1,4);return call+5+relative;}
static bool Resolve(BYTE* game,BYTE* dz,PostGameSites* output) {
    static const short managerPattern[]={0x8b,0xf1,0xc7,0x45,0xe8,0,0,0,0,0x8b,0x0d,-1,-1,-1,-1,
        0xe8,-1,-1,-1,-1,0x85,0xf6,0x75,0x14,0x33,0xc0,0x8b,0x4d,0xf4,0x64,0x89,0x0d,0,0,0,0,
        0x59,0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc3};
    static const short dispatchPattern[]={0x55,0x8b,0xec,0x51,0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,
        0x0f,0xb6,0xc0,0x85,0xc0,0x75,0x22,0xe8,-1,-1,-1,-1,0xa3,-1,-1,-1,-1,
        0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,0x50,0xe8,-1,-1,-1,-1,0x83,0xc4,0x04,
        0xe8,-1,-1,-1,-1,0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,0x0f,0xb6,0xc8,0x85,0xc9,0x75,0x48};
    static const short clearPattern[]={0x55,0x8b,0xec,0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,
        0x83,0x7d,0x08,0,0x75,0x14};
    static const short popPattern[]={0xb9,-1,-1,-1,-1,0xe8,-1,-1,-1,-1,
        0xe8,-1,-1,-1,-1,0xeb,0xa7,0x8b,0xe5,0x5d,0xc3};
    // Several handle types use this lookup body; all must name the same
    // manager slot. We need that shared operand, not one chosen function.
    BYTE* manager=FindCode(game,managerPattern,0,0,11);
    BYTE* dispatch=FindCode(dz,dispatchPattern);
    if(!manager || !dispatch)return false;
    void* callback=*(void**)(dispatch+5);
    void* queue=*(void**)(dispatch+56);
    if(callback!=*(void**)(dispatch+32))return false;
    BYTE* clear=FindCode(dz,clearPattern,4,callback);
    BYTE* pop=FindCode(dz,popPattern,1,queue);
    if(!clear || !pop || CallTarget(dispatch+50)!=CallTarget(pop+10))return false;
    BYTE* empty=(BYTE*)CallTarget(dispatch+60);
    if(empty[10]!=0xe8)return false;
    BYTE* emptyBody=(BYTE*)CallTarget(empty+10);
    if(emptyBody[12]!=0x83 || emptyBody[13]!=0x78 || emptyBody[15]!=0)return false;
    *output={dispatch,*(void* volatile**)(manager+11),callback,queue,
        CallTarget(clear+8),empty,CallTarget(pop+5),CallTarget(pop+10),emptyBody[14]};
    return true;
}
bool FindPostGameSites(BYTE* game,BYTE* dz,PostGameSites* output) {
    PostGameSites sites={};bool found=false;
    if(!game || !dz || !output || !Guard([&](){found=Resolve(game,dz,&sites);}) || !found)return false;
    *output=sites;return true;
}

static PostGameSites sites;
static void (__cdecl* originalDispatch)();
static bool seenActive=false,cleanupFault=false;
static unsigned cleanupStage=0,oldRemaining=0;
static bool cursorFault=false;
static HMODULE selfModule;
static wchar_t logPath[MAX_PATH];
static void Log(const char* message,MH_STATUS status) {
    if(!logPath[0])return;
    FILE* file=0;if(_wfopen_s(&file,logPath,L"ab") || !file)return;
    SYSTEMTIME time;GetLocalTime(&time);
    fprintf(file,"%04u-%02u-%02u %02u:%02u:%02u pid=%lu %s %s\r\n",time.wYear,time.wMonth,
        time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId(),message,MH_StatusToString(status));
    fclose(file);
}
static void __cdecl Dispatch() {
    bool completed=Guard([&](){
        if(!*sites.manager && seenActive && !cleanupStage) {
            oldRemaining=*(unsigned*)((BYTE*)sites.queue+sites.queueSizeOffset);
            cleanupStage=1;
        }
        // Preserve both completed steps and the old map's FIFO boundary if a
        // native pop fails. Newly appended work belongs to the next map.
        if(cleanupStage==1) {
            ((void(__thiscall*)(void*))sites.clearCallback)(sites.callback);
            cleanupStage=2;
        }
        if(cleanupStage==2) {
            while(oldRemaining) {
                if(((bool(__thiscall*)(void*))sites.queueEmpty)(sites.queue)) {oldRemaining=0;break;}
                ((void(__thiscall*)(void*))sites.queuePop)(sites.queue);
                --oldRemaining;
            }
            cleanupStage=3;
        }
        if(cleanupStage==3) {
            ((void(__cdecl*)())sites.resetContext)();
            cleanupStage=0;seenActive=false;
        }
    });
    if(!completed) {
        if(!cleanupFault)Log("RETRY fault in native old-map callback cleanup",MH_OK);
        cleanupFault=true;
        return;
    }
    cleanupFault=false;
    int cursor=UpdateMenuCursor();
    if(cursor<0 && !cursorFault)Log("RETRY native menu cursor model/binding",MH_OK);
    if(cursor>0)Log("CURSOR native menu model loaded and bound",MH_OK);
    cursorFault=cursor<0;
    if(*sites.manager) {seenActive=true;originalDispatch();}
}
static DWORD WINAPI Install(void*) {
    DWORD length=GetModuleFileNameW(selfModule,logPath,MAX_PATH);
    wchar_t* slash=length && length<MAX_PATH?wcsrchr(logPath,L'\\'):0;
    static const wchar_t name[]=L"War3PostGameFix.log";
    if(slash && (size_t)(MAX_PATH-(slash+1-logPath))>=sizeof(name)/sizeof(wchar_t))
        wcscpy_s(slash+1,MAX_PATH-(slash+1-logPath),name);
    else logPath[0]=0;
    HMODULE game=0,dz=0,pinned=0;
    unsigned stage=0,lastStage=~0u;MH_STATUS lastResult=MH_OK;
    for(;;) {
        const char* operation="";MH_STATUS result=MH_OK;
        switch(stage) {
        case 0:
            operation="RETRY retain MIX module";
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                (LPCWSTR)&Install,&pinned)) {++stage;continue;}
            break;
        case 1:
            operation="WAIT Game.dll and dzclient_api.dll";
            game=GetModuleHandleW(L"Game.dll");dz=GetModuleHandleW(L"dzclient_api.dll");
            if(game && dz) {++stage;continue;}
            break;
        case 2:
            operation="RETRY locate map script dispatcher";
            if(FindPostGameSites((BYTE*)game,(BYTE*)dz,&sites)) {++stage;continue;}
            break;
        case 3:
            operation="RETRY initialize MinHook";
            result=MH_Initialize();if(result==MH_OK) {++stage;continue;}break;
        case 4:
            operation="RETRY create dispatcher hook";
            result=MH_CreateHook(sites.dispatch,(void*)Dispatch,(void**)&originalDispatch);
            if(result==MH_OK) {++stage;continue;}break;
        case 5:
            operation="RETRY enable dispatcher hook";
            result=MH_EnableHook(sites.dispatch);
            if(result==MH_OK) {Log("INSTALLED post-game callback lifecycle repair",result);++stage;continue;}break;
        case 6:
            operation="RETRY optional menu cursor setup";
            if(InstallMenuCursor((BYTE*)game,sites.manager,&result)) {Log("INSTALLED native menu cursor restoration",result);return 0;}break;
        }
        if(stage!=lastStage || result!=lastResult) {Log(operation,result);lastStage=stage;lastResult=result;}
        Sleep(stage<=1?250:5000);
    }
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        selfModule=module;HANDLE thread=CreateThread(0,0,Install,0,0,0);
        if(thread)CloseHandle(thread);
    }
    return TRUE;
}
