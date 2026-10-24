#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <cstdio>
#include <cwchar>
#include <atomic>
#include "../vendor/minhook/include/MinHook.h"
#include "palette.h"
#include "rekordbox_target.h"
#include "titlebar.h"

static HMODULE selfModule;
static wchar_t logPath[MAX_PATH], iniPath[MAX_PATH], replacement[LF_FACESIZE];
static SRWLOCK logLock = SRWLOCK_INIT;
static std::atomic<unsigned> calls{0};
static std::atomic<bool> initialized{false};
static bool fontTrace=false;
using FindFn = HRESULT (STDMETHODCALLTYPE*)(IDWriteFontCollection*, const wchar_t*, UINT32*, BOOL*);
using FormatFn = HRESULT (STDMETHODCALLTYPE*)(IDWriteFactory*,const wchar_t*,IDWriteFontCollection*,DWRITE_FONT_WEIGHT,DWRITE_FONT_STYLE,DWRITE_FONT_STRETCH,FLOAT,const wchar_t*,IDWriteTextFormat**);
using GdiFn = HFONT (WINAPI*)(const LOGFONTW*);
static FindFn originalFind;
static FormatFn originalFormat;
static GdiFn originalGdi;

static void log(const wchar_t* kind, const wchar_t* name, bool changed) {
    if (calls.fetch_add(1) > 2000) return;
    AcquireSRWLockExclusive(&logLock);
    FILE* f=nullptr;
    if (_wfopen_s(&f, logPath, L"a, ccs=UTF-8") == 0) {
        fwprintf(f,L"%ls: %ls%ls%ls\n", kind,name?name:L"(null)",changed?L" -> ":L"",changed?replacement:L"");
        fclose(f);
    }
    ReleaseSRWLockExclusive(&logLock);
}
static bool replace(const wchar_t* name) {
    return name && (!_wcsicmp(name,L"Arial") || !_wcsicmp(name,L"Arial Unicode MS"));
}
static HRESULT STDMETHODCALLTYPE findHook(IDWriteFontCollection* c,const wchar_t* name,UINT32* index,BOOL* exists) {
    bool change=replace(name);
    if(fontTrace)log(L"FindFamilyName",name,change);
    if(change) {
        HRESULT hr=originalFind(c,replacement,index,exists);
        if(SUCCEEDED(hr) && *exists) return hr;
    }
    return originalFind(c,name,index,exists);
}
static HRESULT STDMETHODCALLTYPE formatHook(IDWriteFactory* f,const wchar_t* name,IDWriteFontCollection* c,DWRITE_FONT_WEIGHT w,DWRITE_FONT_STYLE s,DWRITE_FONT_STRETCH st,FLOAT size,const wchar_t* locale,IDWriteTextFormat** result) {
    bool change=replace(name); if(fontTrace)log(L"CreateTextFormat",name,change);
    HRESULT hr=originalFormat(f,change?replacement:name,c,w,s,st,size,locale,result);
    if(FAILED(hr) && change) return originalFormat(f,name,c,w,s,st,size,locale,result);
    return hr;
}
static HFONT WINAPI gdiHook(const LOGFONTW* lf) {
    if(!lf) return originalGdi(lf);
    bool change=replace(lf->lfFaceName); if(fontTrace)log(L"CreateFontIndirectW",lf->lfFaceName,change);
    if(!change) return originalGdi(lf);
    LOGFONTW copy=*lf; wcscpy_s(copy.lfFaceName,replacement);
    HFONT result=originalGdi(&copy); return result?result:originalGdi(lf);
}
extern "C" __declspec(dllexport) DWORD WINAPI Initialize(void*) {
    if(initialized.exchange(true)) return 1;
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    RekordboxTarget target;
    if(!ReadRekordboxTarget(exe,target)) return 0;
    GetModuleFileNameW(selfModule,iniPath,MAX_PATH);
    wchar_t* slash=wcsrchr(iniPath,L'\\'); if(!slash) return 0; slash[1]=0;
    wcscpy_s(logPath,iniPath); wcscat_s(logPath,L"font-hook.log"); wcscat_s(iniPath,L"font.ini");
    GetPrivateProfileStringW(L"Font",L"Family",L"Segoe UI",replacement,LF_FACESIZE,iniPath);
    fontTrace=GetPrivateProfileIntW(L"Diagnostics",L"FontTrace",0,iniPath)!=0;
    log(L"Initialize",replacement,false);
    log(L"Target",exe,false);
    IDWriteFactory* factory=nullptr;
    HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&factory));
    if(FAILED(hr)) { log(L"ERROR",L"DWriteCreateFactory",false); return 0; }
    IDWriteFontCollection* collection=nullptr;
    hr=factory->GetSystemFontCollection(&collection);
    if(FAILED(hr)) { factory->Release();return 0; }
    UINT32 index=0; BOOL exists=FALSE;
    collection->FindFamilyName(replacement,&index,&exists);
    if(!exists) {
        log(L"Font unavailable; using Segoe UI",replacement,false);
        wcscpy_s(replacement,L"Segoe UI");
        collection->FindFamilyName(replacement,&index,&exists);
    }
    if(!exists) {log(L"ERROR",L"Fallback font missing",false);collection->Release();factory->Release();return 0;}
    if(MH_Initialize()!=MH_OK) return 0;
    void** cv=*reinterpret_cast<void***>(collection);
    void** fv=*reinterpret_cast<void***>(factory);
    MH_STATUS a=MH_CreateHook(cv[5],reinterpret_cast<void*>(&findHook),reinterpret_cast<void**>(&originalFind));
    MH_STATUS b=MH_CreateHook(fv[15],reinterpret_cast<void*>(&formatHook),reinterpret_cast<void**>(&originalFormat));
    MH_STATUS c=MH_CreateHook(reinterpret_cast<void*>(&CreateFontIndirectW),reinterpret_cast<void*>(&gdiHook),reinterpret_cast<void**>(&originalGdi));
    collection->Release();factory->Release();
    if(a!=MH_OK||b!=MH_OK||c!=MH_OK||!InstallPalette(iniPath)||!InstallTitleBar(iniPath)) {MH_Uninitialize();log(L"ERROR",L"Hook creation failed",false);return 0;}
    if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) {MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();return 0;}
    ApplyExistingTitleBars();
    log(L"READY",L"Hooks installed",false);return 1;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {selfModule=h; DisableThreadLibraryCalls(h);}
    return TRUE;
}
