#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include "discovery.h"

static int fail(const wchar_t* message,int code=1) {
    fwprintf(stderr,L"%ls\n",message);
    MessageBoxW(nullptr,message,L"rekordbox - Custom Font",MB_OK|MB_ICONINFORMATION);return code;
}
static bool inject(HANDLE process,DWORD pid,const wchar_t* dll) {
    SIZE_T bytes=(wcslen(dll)+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote) return false;
    if(!WriteProcessMemory(process,remote,dll,bytes,nullptr)) {VirtualFreeEx(process,remote,0,MEM_RELEASE);return false;}
    auto load=reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"));
    HANDLE thread=CreateRemoteThread(process,nullptr,0,load,remote,0,nullptr);
    if(!thread) {VirtualFreeEx(process,remote,0,MEM_RELEASE);return false;}
    if(WaitForSingleObject(thread,20000)!=WAIT_OBJECT_0) {CloseHandle(thread);return false;}
    CloseHandle(thread); VirtualFreeEx(process,remote,0,MEM_RELEASE);
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snap==INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W entry{}; entry.dwSize=sizeof(entry); uintptr_t base=0;
    if(Module32FirstW(snap,&entry)) do {
        if(!_wcsicmp(entry.szExePath,dll)) {base=reinterpret_cast<uintptr_t>(entry.modBaseAddr);break;}
    }while(Module32NextW(snap,&entry));
    CloseHandle(snap);if(!base) return false;
    HMODULE local=LoadLibraryExW(dll,nullptr,DONT_RESOLVE_DLL_REFERENCES);if(!local)return false;
    auto proc=GetProcAddress(local,"Initialize");
    if(!proc){FreeLibrary(local);return false;}
    uintptr_t offset=reinterpret_cast<uintptr_t>(proc)-reinterpret_cast<uintptr_t>(local);
    FreeLibrary(local);
    thread=CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(base+offset),nullptr,0,nullptr);
    if(!thread)return false;
    DWORD code=0;
    bool ok=WaitForSingleObject(thread,20000)==WAIT_OBJECT_0 && GetExitCodeThread(thread,&code) && code==1;
    CloseHandle(thread);return ok;
}
int wmain(int argc,wchar_t** argv) {
    // Editor terminals can inherit this flag. It prevents the Electron
    // rekordboxAgent from starting as an application. Only this launcher's
    // child environment is changed; no user/system environment is modified.
    SetEnvironmentVariableW(L"ELECTRON_RUN_AS_NODE",nullptr);
    if(argc>=2&&!wcscmp(argv[1],L"--list")) {
        if(argc!=2&&!(argc==4&&!wcscmp(argv[2],L"--root"))){fwprintf(stderr,L"Usage: --list [--root directory]\n");return 1;}
        auto targets=DiscoverTargets(argc==4?argv[3]:L"");
        for(size_t i=0;i<targets.size();i++) {const auto& t=targets[i];wprintf(L"%ls %u.%u.%u.%u %ls\n",i==0?L"SELECTED":L"FOUND",t.version[0],t.version[1],t.version[2],t.version[3],t.path.c_str());}
        if(targets.empty())wprintf(L"No supported x64 rekordbox installation found.\n");return targets.empty()?1:0;
    }
    wchar_t dll[MAX_PATH];GetModuleFileNameW(nullptr,dll,MAX_PATH);
    wchar_t* slash=wcsrchr(dll,L'\\');if(!slash)return 1;wcscpy_s(slash+1,MAX_PATH-(slash+1-dll),L"font_hook.dll");
    if(argc==3 && !wcscmp(argv[1],L"--attach")) {
        DWORD pid=wcstoul(argv[2],nullptr,10);
        HANDLE p=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
        wchar_t exe[MAX_PATH];DWORD size=MAX_PATH;
        RekordboxTarget attached;
        if(!p||!QueryFullProcessImageNameW(p,0,exe,&size)||!ReadRekordboxTarget(exe,attached)){if(p)CloseHandle(p);fwprintf(stderr,L"Target process mismatch\n");return 1;}
        bool ok=inject(p,pid,dll);CloseHandle(p);wprintf(L"Attach: %ls\n",ok?L"OK":L"FAILED");return ok?0:1;
    }
    std::vector<RekordboxTarget> targets;
    if(argc==3&&!wcscmp(argv[1],L"--exe")) {AddTarget(argv[2],targets);}
    else if(argc==1)targets=DiscoverTargets();
    else return fail(L"Usage: rekordbox-font.exe [--exe path] or --list");
    if(targets.empty())return fail(L"No x64 rekordbox installation found. You can specify it with --exe followed by the path to rekordbox.exe.");
    const auto& target=targets.front();
    wchar_t versionText[64];swprintf_s(versionText,L"%u.%u.%u.%u",target.version[0],target.version[1],target.version[2],target.version[3]);
    wprintf(L"Selected rekordbox %ls: %ls\n",versionText,target.path.c_str());
    std::wstring launchLog=dll;launchLog.resize(launchLog.find_last_of(L'\\')+1);launchLog+=L"launcher.log";
    FILE* log=nullptr;if(!_wfopen_s(&log,launchLog.c_str(),L"a, ccs=UTF-8")){fwprintf(log,L"Selected rekordbox %ls: %ls\n",versionText,target.path.c_str());fclose(log);}
    if(GetFileAttributesW(dll)==INVALID_FILE_ATTRIBUTES)return fail(L"font_hook.dll is missing next to the launcher.");
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);bool running=false;
    if(Process32FirstW(snap,&pe))do {if(!_wcsicmp(pe.szExeFile,L"rekordbox.exe"))running=true;}while(Process32NextW(snap,&pe));
    CloseHandle(snap);
    if(running)return fail(L"Close rekordbox normally before using this launcher.",2);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    std::wstring dir(target.path);dir.resize(dir.find_last_of(L'\\'));
    if(!CreateProcessW(target.path.c_str(),nullptr,nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,dir.c_str(),&si,&pi)){wchar_t error[200];swprintf_s(error,L"Launch failed: Windows error %lu",GetLastError());return fail(error);}
    bool ok=inject(pi.hProcess,pi.dwProcessId,dll);
    ResumeThread(pi.hThread);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    wprintf(L"Font module: %ls; rekordbox PID %lu\n",ok?L"OK":L"FAILED (rekordbox resumed normally)",pi.dwProcessId);
    if(!_wfopen_s(&log,launchLog.c_str(),L"a, ccs=UTF-8")){fwprintf(log,L"Module %ls; PID %lu\n",ok?L"OK":L"FAILED",pi.dwProcessId);fclose(log);}
    if(!ok)MessageBoxW(nullptr,L"The custom font/colour module could not be initialized. rekordbox was resumed. See launcher.log and font-hook.log.",L"rekordbox - Custom Font",MB_OK|MB_ICONWARNING);
    return ok?0:1;
}
