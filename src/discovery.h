#pragma once
#include "rekordbox_target.h"
#include <filesystem>
#include <set>

inline void AddTarget(const std::wstring& path,std::vector<RekordboxTarget>& targets) {
    RekordboxTarget target;
    if(!ReadRekordboxTarget(path,target))return;
    for(const auto& existing:targets)if(!_wcsicmp(existing.path.c_str(),target.path.c_str()))return;
    targets.push_back(std::move(target));
}
inline void ScanRoot(const std::wstring& root,std::vector<RekordboxTarget>& targets,int depth=0) {
    namespace fs=std::filesystem;
    AddTarget((fs::path(root)/L"rekordbox.exe").wstring(),targets);
    if(depth>=2)return;
    std::error_code ec;
    fs::directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
    for(;it!=end;it.increment(ec)) {
        if(ec)break;
        if(it->is_directory(ec)&&!it->is_symlink(ec))ScanRoot(it->path().wstring(),targets,depth+1);
    }
}
inline void ScanUninstall(HKEY hive,REGSAM view,std::vector<RekordboxTarget>& targets) {
    HKEY root=nullptr;
    if(RegOpenKeyExW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,KEY_READ|view,&root)!=ERROR_SUCCESS)return;
    for(DWORD index=0;;index++) {
        wchar_t name[512];DWORD size=512;
        LONG status=RegEnumKeyExW(root,index,name,&size,nullptr,nullptr,nullptr,nullptr);
        if(status==ERROR_NO_MORE_ITEMS)break;
        if(status!=ERROR_SUCCESS)continue;
        HKEY entry=nullptr;if(RegOpenKeyExW(root,name,0,KEY_READ|view,&entry)!=ERROR_SUCCESS)continue;
        wchar_t display[1024]{};DWORD bytes=sizeof(display),type=0;
        if(RegQueryValueExW(entry,L"DisplayName",nullptr,&type,reinterpret_cast<BYTE*>(display),&bytes)==ERROR_SUCCESS && (type==REG_SZ||type==REG_EXPAND_SZ)&&Lower(display).find(L"rekordbox")!=std::wstring::npos) {
            for(const wchar_t* field:{L"InstallLocation",L"DisplayIcon",L"UninstallString"}) {
                wchar_t value[32768]{};bytes=sizeof(value);
                if(RegQueryValueExW(entry,field,nullptr,&type,reinterpret_cast<BYTE*>(value),&bytes)!=ERROR_SUCCESS||(type!=REG_SZ&&type!=REG_EXPAND_SZ))continue;
                std::wstring path=value;
                if(type==REG_EXPAND_SZ){wchar_t expanded[32768];if(ExpandEnvironmentStringsW(value,expanded,32768))path=expanded;}
                if(field==std::wstring(L"InstallLocation")){ScanRoot(path,targets);continue;}
                if(!path.empty()&&path.front()==L'"'){auto q=path.find(L'"',1);if(q!=std::wstring::npos)path=path.substr(1,q-1);}
                else {auto exe=Lower(path).find(L".exe");if(exe!=std::wstring::npos)path.resize(exe+4);}
                AddTarget(path,targets);
                auto parent=std::filesystem::path(path).parent_path();
                if(!parent.empty())ScanRoot(parent.wstring(),targets);
            }
        }
        RegCloseKey(entry);
    }
    RegCloseKey(root);
}
inline std::vector<RekordboxTarget> DiscoverTargets(const std::wstring& explicitRoot=L"") {
    std::vector<RekordboxTarget> targets;
    if(!explicitRoot.empty())ScanRoot(explicitRoot,targets);
    else {
        std::set<std::wstring> roots;
        for(const wchar_t* var:{L"ProgramW6432",L"ProgramFiles",L"ProgramFiles(x86)"}) {
            wchar_t value[32768];DWORD size=GetEnvironmentVariableW(var,value,32768);
            if(size&&size<32768)roots.insert(std::filesystem::path(value).append(L"rekordbox").wstring());
        }
        for(const auto& root:roots)ScanRoot(root,targets);
        for(HKEY hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER})for(REGSAM view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY})ScanUninstall(hive,view,targets);
        for(HKEY hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER}) {
            wchar_t path[32768]{};DWORD bytes=sizeof(path);
            if(RegGetValueW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\rekordbox.exe",nullptr,RRF_RT_REG_SZ,nullptr,path,&bytes)==ERROR_SUCCESS)AddTarget(path,targets);
        }
    }
    std::sort(targets.begin(),targets.end(),[](const auto& a,const auto& b){if(a.version!=b.version)return a.version>b.version;return Lower(a.path)<Lower(b.path);});
    return targets;
}
