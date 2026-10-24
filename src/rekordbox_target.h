#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winver.h>
#include <string>
#include <vector>
#include <array>
#include <algorithm>

struct RekordboxTarget {
    std::wstring path;
    std::array<unsigned,4> version{};
};
inline std::wstring Lower(std::wstring s) {
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return wchar_t(towlower(c));});return s;
}
inline bool ReadRekordboxTarget(const std::wstring& path, RekordboxTarget& result) {
    auto slash=path.find_last_of(L"\\/");
    if(_wcsicmp(path.c_str()+(slash==std::wstring::npos?0:slash+1),L"rekordbox.exe"))return false;
    DWORD type=0;
    if(!GetBinaryTypeW(path.c_str(),&type)||type!=SCS_64BIT_BINARY)return false;
    DWORD ignored=0,size=GetFileVersionInfoSizeW(path.c_str(),&ignored);
    if(!size||size>1024*1024)return false;
    std::vector<BYTE> data(size);
    if(!GetFileVersionInfoW(path.c_str(),0,size,data.data()))return false;
    VS_FIXEDFILEINFO* info=nullptr;UINT infoSize=0;
    if(!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&infoSize)||infoSize<sizeof(*info)||info->dwSignature!=0xfeef04bd)return false;
    struct Translation {WORD language,codepage;};
    Translation* translations=nullptr;UINT translationBytes=0;bool productMatches=false;
    if(VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&translationBytes)) {
        for(UINT i=0;i<translationBytes/sizeof(Translation);i++) {
            wchar_t key[100];swprintf_s(key,L"\\StringFileInfo\\%04x%04x\\ProductName",translations[i].language,translations[i].codepage);
            wchar_t* value=nullptr;UINT length=0;
            if(VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&value),&length)&&length&&Lower(value).find(L"rekordbox")!=std::wstring::npos)productMatches=true;
        }
    }
    if(!productMatches)return false;
    result.version={HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS)};
    wchar_t absolute[32768];DWORD length=GetFullPathNameW(path.c_str(),32768,absolute,nullptr);
    if(!length||length>=32768)return false;
    result.path=absolute;return true;
}
