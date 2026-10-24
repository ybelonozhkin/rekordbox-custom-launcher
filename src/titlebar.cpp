#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <cstdio>
#include "titlebar.h"

static bool enabled;
static wchar_t logPath[MAX_PATH];

static void Apply(HWND window) {
    if(!enabled||!window)return;
    LONG_PTR style=GetWindowLongPtrW(window,GWL_STYLE);
    if((style&WS_CHILD)||(style&WS_CAPTION)!=WS_CAPTION)return;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId())return;
    BOOL allowDark=TRUE;
    HRESULT result=DwmSetWindowAttribute(window,20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,&allowDark,sizeof(allowDark));
    if(FAILED(result))result=DwmSetWindowAttribute(window,19,&allowDark,sizeof(allowDark));
    FILE* f=nullptr;
    if(!_wfopen_s(&f,logPath,L"a")) {
        fprintf(f,"Dark title bar: %s (HRESULT 0x%08lx)\n",SUCCEEDED(result)?"enabled":"unavailable",static_cast<unsigned long>(result));
        fclose(f);
    }
}
static BOOL CALLBACK FindMain(HWND window,LPARAM param) {
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId()||!IsWindowVisible(window)||GetWindow(window,GW_OWNER))return TRUE;
    wchar_t title[256];GetWindowTextW(window,title,256);
    if(_wcsicmp(title,L"rekordbox"))return TRUE;
    *reinterpret_cast<HWND*>(param)=window;return FALSE;
}
static DWORD WINAPI WaitForMain(void*) {
    // Startup-only worker. It exits after one application, or after 60 seconds.
    // No CreateWindowEx hooks, window subclassing, or recurring UI work.
    for(int attempt=0;attempt<240;attempt++) {
        HWND window=nullptr;EnumWindows(FindMain,reinterpret_cast<LPARAM>(&window));
        if(window){Apply(window);return 0;}
        Sleep(250);
    }
    return 0;
}
bool InstallTitleBar(const wchar_t* iniPath) {
    enabled=GetPrivateProfileIntW(L"Window",L"DarkTitleBar",1,iniPath)!=0;
    if(!enabled)return true;
    wcscpy_s(logPath,iniPath);
    auto slash=wcsrchr(logPath,L'\\');if(!slash)return false;
    wcscpy_s(slash+1,MAX_PATH-(slash+1-logPath),L"titlebar.log");
    return true;
}
void ApplyExistingTitleBars() {if(enabled){HANDLE worker=CreateThread(nullptr,0,WaitForMain,nullptr,0,nullptr);if(worker)CloseHandle(worker);}}
