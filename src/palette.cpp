#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <immintrin.h>
#include <intrin.h>
#include "../vendor/minhook/include/MinHook.h"
#include "palette.h"

using BlitFn=int (WINAPI*)(HDC,int,int,int,int,int,int,int,int,const void*,const BITMAPINFO*,UINT,DWORD);
static BlitFn originalBlit;
static uint8_t neutral[256];
static bool enabled=true, mutedBlue=true;
static int blueDimming=100, tintStrength=0;
static uint32_t tintColor=0;
static uint8_t tintTable[3][256]; // R, G, B; prepared once at startup.
static bool highlightEnabled=false, customHighlightSource=false;
static uint32_t highlightColor=0, highlightSourceColor=0x1473e6;
static int highlightStrength=100;
static int highlightSource[3]{20,115,230}, highlightTarget[3]{};
static int highlightSourceMin=20, highlightSourceChroma=210;
static unsigned highlightCoverage[256];
static wchar_t paletteLog[MAX_PATH];
static std::atomic<unsigned> frameCount{0};
static uint32_t greyTable[256];
static bool haveAvx2=false;
using ClearFn=void (WINAPI*)(float,float,float,float);
static ClearFn originalClear;
static std::atomic<unsigned> glClears{0};

static bool ParseRgb(const wchar_t* text,uint32_t& result) {
    if(!text)return false;
    if(*text==L'#')++text;
    if(wcslen(text)!=6)return false;
    uint32_t value=0;
    for(int i=0;i<6;i++) {
        wchar_t ch=text[i];int digit=-1;
        if(ch>=L'0'&&ch<=L'9')digit=ch-L'0';
        else if(ch>=L'a'&&ch<=L'f')digit=ch-L'a'+10;
        else if(ch>=L'A'&&ch<=L'F')digit=ch-L'A'+10;
        if(digit<0)return false;
        value=(value<<4)|digit;
    }
    result=value;return true;
}
static uint32_t TintPixel(uint32_t p) {
    if(!tintStrength)return p;
    return (p&0xff000000)|(uint32_t(tintTable[0][(p>>16)&255])<<16)|
           (uint32_t(tintTable[1][(p>>8)&255])<<8)|tintTable[2][p&255];
}
static bool IsNativeBlue(int r,int g,int b) {
    return b>g&&g>r&&(g-r)*4>(b-r)&&(g-r)*5<(b-r)*4;
}
static uint32_t HighlightPixel(uint32_t p,uint32_t base) {
    if(!highlightEnabled||!highlightStrength)return base;
    int rgb[]={int((p>>16)&255),int((p>>8)&255),int(p&255)};
    int low=std::min({rgb[0],rgb[1],rgb[2]});
    int chroma=std::max({rgb[0],rgb[1],rgb[2]})-low;
    if(customHighlightSource) {
        if(!highlightSourceChroma) {
            // Neutral source colors have no hue; only match that exact shade.
            if((p&0xffffff)!=highlightSourceColor)return base;
        } else {
            if(!chroma)return base;
            int tolerance=chroma*highlightSourceChroma/5;
            for(int i=0;i<3;i++)
                if(std::abs((rgb[i]-low)*highlightSourceChroma-(highlightSource[i]-highlightSourceMin)*chroma)>tolerance)return base;
        }
    } else if(!IsNativeBlue(rgb[0],rgb[1],rgb[2]))return base;
    unsigned coverage=highlightSourceChroma?highlightCoverage[chroma]:256;
    int background=low*256-highlightSourceMin*int(coverage);
    uint32_t result=p&0xff000000;
    for(int i=0;i<3;i++) {
        // Retain the neutral contribution at antialiased edges and dimmer shades.
        int target=std::clamp((background+highlightTarget[i]*int(coverage)+128)/256,0,255);
        int shift=16-i*8;
        int previous=(base>>shift)&255;
        unsigned value=unsigned((previous*(100-highlightStrength)+target*highlightStrength+50)/100);
        result|=value<<shift;
    }
    return result;
}

static void WINAPI clearHook(float r,float g,float b,float a) {
    if(enabled && a>=0.999f && r==g && g==b && r>=0 && r<=1) {
        int v=std::clamp(int(std::lround(r*255)),0,255);
        if(v<=32) {
            uint32_t color=greyTable[v];
            r=((color>>16)&255)/255.0f;g=((color>>8)&255)/255.0f;b=(color&255)/255.0f;
        }
    }
    originalClear(r,g,b,a);
    if(glClears.fetch_add(1)==0) {
        FILE* f=nullptr;if(!_wfopen_s(&f,paletteLog,L"a")){fprintf(f,"OpenGL background hook active\n");fclose(f);}
    }
}

static uint32_t BasePixel(uint32_t p) {
    unsigned r=(p>>16)&255,g=(p>>8)&255,b=p&255;
    if(r==g && g==b) {
        unsigned v=neutral[r];return (p&0xff000000)|(v<<16)|(v<<8)|v;
    }
    // Continuous colour transform also covers antialiased edges around text.
    // A flat-fill-only replacement would leave bright blue fringes.
    if(mutedBlue && blueDimming && IsNativeBlue(int(r),int(g),int(b))) {
        int strength=std::min(210,int(b-r));
        int nr=std::clamp(int(neutral[r])+12*strength/210,0,255);
        int ng=std::clamp(int(neutral[g])-47*strength/210,0,255);
        int nb=std::clamp(int(neutral[b])-89*strength/210,0,255);
        nr=(int(r)*(100-blueDimming)+nr*blueDimming+50)/100;
        ng=(int(g)*(100-blueDimming)+ng*blueDimming+50)/100;
        nb=(int(b)*(100-blueDimming)+nb*blueDimming+50)/100;
        return (p&0xff000000)|(nr<<16)|(ng<<8)|nb;
    }
    return p;
}
static uint32_t mapPixel(uint32_t p) {
    uint32_t result=BasePixel(p);
    if(highlightEnabled&&highlightStrength)result=HighlightPixel(p,result);
    return TintPixel(result);
}
static void PreparePalette() {
    for(int i=0;i<3;i++) {
        highlightSource[i]=(highlightSourceColor>>(16-i*8))&255;
        highlightTarget[i]=(highlightColor>>(16-i*8))&255;
    }
    highlightSourceMin=std::min({highlightSource[0],highlightSource[1],highlightSource[2]});
    highlightSourceChroma=std::max({highlightSource[0],highlightSource[1],highlightSource[2]})-highlightSourceMin;
    for(unsigned v=0;v<256;v++)highlightCoverage[v]=highlightSourceChroma?std::min(256u,(v*256+highlightSourceChroma/2)/highlightSourceChroma):256;
    for(int channel=0;channel<3;channel++) {
        int target=(tintColor>>(16-8*channel))&255;
        for(int v=0;v<256;v++)tintTable[channel][v]=uint8_t((v*(100-tintStrength)+target*tintStrength+50)/100);
    }
    for(unsigned v=0;v<256;v++)greyTable[v]=mapPixel(v*0x010101);
    int cpu[4];__cpuid(cpu,0);int maxLeaf=cpu[0];__cpuid(cpu,1);
    if(maxLeaf>=7&&(cpu[2]&(1<<27))&&(cpu[2]&(1<<28))&&(_xgetbv(0)&6)==6) {
        __cpuidex(cpu,7,0);haveAvx2=(cpu[1]&(1<<5))!=0;
    }
}
__declspec(noinline) static void RenderAvx2(uint32_t* dst,const uint32_t* src,size_t count) {
    const __m256i byteMask=_mm256_set1_epi32(255),alphaMask=_mm256_set1_epi32(int(0xff000000));
    size_t i=0;
    for(;i+8<=count;i+=8) {
        auto p=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(src+i));
        auto b=_mm256_and_si256(p,byteMask),g=_mm256_and_si256(_mm256_srli_epi32(p,8),byteMask),r=_mm256_and_si256(_mm256_srli_epi32(p,16),byteMask);
        auto mask=_mm256_and_si256(_mm256_cmpeq_epi32(b,g),_mm256_cmpeq_epi32(g,r));
        unsigned lanes=unsigned(_mm256_movemask_ps(_mm256_castsi256_ps(mask)));
        if(lanes) {
            auto greys=_mm256_i32gather_epi32(reinterpret_cast<const int*>(greyTable),b,4);
            auto out=_mm256_or_si256(greys,_mm256_and_si256(p,alphaMask));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst+i),out);
        }
        if(lanes!=255)for(unsigned j=0;j<8;j++)if(!(lanes&(1u<<j)))dst[i+j]=mapPixel(src[i+j]);
    }
    for(;i<count;i++)dst[i]=mapPixel(src[i]);
    _mm256_zeroupper();
}
static void RenderPixels(uint32_t* dst,const uint32_t* src,size_t count) {
    if(haveAvx2)RenderAvx2(dst,src,count);
    else for(size_t i=0;i<count;i++)dst[i]=mapPixel(src[i]);
}
static int WINAPI blitHook(HDC dc,int dx,int dy,int dw,int dh,int sx,int sy,int sw,int sh,const void* bits,const BITMAPINFO* bi,UINT use,DWORD rop) {
    if(!enabled||!bits||!bi||bi->bmiHeader.biBitCount!=32||bi->bmiHeader.biPlanes!=1||
       bi->bmiHeader.biCompression!=BI_RGB||use!=DIB_RGB_COLORS||rop!=SRCCOPY)
        return originalBlit(dc,dx,dy,dw,dh,sx,sy,sw,sh,bits,bi,use,rop);
    int w=bi->bmiHeader.biWidth, h=bi->bmiHeader.biHeight;
    if(w<=0||w>8192||h==0||h>8192||h< -8192)
        return originalBlit(dc,dx,dy,dw,dh,sx,sy,sw,sh,bits,bi,use,rop);
    size_t count=size_t(w)*size_t(h<0?-h:h);
    try {
        thread_local std::vector<uint32_t> buffer;
        buffer.resize(count);
        const uint32_t* src=static_cast<const uint32_t*>(bits);
        LARGE_INTEGER start{},end{},freq{};QueryPerformanceCounter(&start);
        int height=h<0?-h:h;
        RECT clip{};
        int left=0,right=w,top=0,bottom=height;
        if(sx==0&&sy==0&&sw==w&&sh==height&&dw==w&&dh==height&&GetClipBox(dc,&clip)!=ERROR) {
            left=std::clamp(int(clip.left)-dx,0,w);right=std::clamp(int(clip.right)-dx,0,w);
            top=std::clamp(int(clip.top)-dy,0,height);bottom=std::clamp(int(clip.bottom)-dy,0,height);
        }
        for(int y=top;y<bottom;y++) {
            size_t row=size_t(h>0?height-1-y:y)*w;
            RenderPixels(buffer.data()+row+left,src+row+left,size_t(right-left));
        }
        QueryPerformanceCounter(&end);
        unsigned frame=frameCount.fetch_add(1);
        if(frame==0||frame==30||frame==120) {
            QueryPerformanceFrequency(&freq);
            FILE* f=nullptr;if(!_wfopen_s(&f,paletteLog,L"a")) {
                fprintf(f,"frame %u: %dx%d, palette %.3f ms\n",frame,w,h,1000.0*(end.QuadPart-start.QuadPart)/freq.QuadPart);
                if(frame==0) {
                    // Diagnostics: only colour values and counts, never pixels/content.
                    unsigned counts[256]{};
                    for(size_t i=0;i<count;i++){auto p=src[i];unsigned r=(p>>16)&255,g=(p>>8)&255,b=p&255;if(r<50&&g>60&&g<170&&b>170)counts[g]++;}
                    for(unsigned g=61;g<170;g++)if(counts[g]>500)fprintf(f,"blue pixels with G=%u: %u\n",g,counts[g]);
                }
                fclose(f);
            }
        }
        return originalBlit(dc,dx,dy,dw,dh,sx,sy,sw,sh,buffer.data(),bi,use,rop);
    } catch(...) {
        return originalBlit(dc,dx,dy,dw,dh,sx,sy,sw,sh,bits,bi,use,rop);
    }
}
static void ReadPaletteSettings(const wchar_t* path) {
    enabled=GetPrivateProfileIntW(L"Palette",L"Enabled",1,path)!=0;
    mutedBlue=GetPrivateProfileIntW(L"Palette",L"MutedBlue",1,path)!=0;
    blueDimming=std::clamp(int(GetPrivateProfileIntW(L"Palette",L"BlueDimming",100,path)),0,100);
    tintStrength=std::clamp(int(GetPrivateProfileIntW(L"Palette",L"TintStrength",0,path)),0,100);
    wchar_t tint[64]{};
    GetPrivateProfileStringW(L"Palette",L"TintColor",L"#000000",tint,64,path);
    if(!ParseRgb(tint,tintColor))tintStrength=0;
    wchar_t target[64]{},source[64]{};
    GetPrivateProfileStringW(L"Palette",L"HighlightColor",L"",target,64,path);
    highlightEnabled=ParseRgb(target,highlightColor);
    highlightStrength=std::clamp(int(GetPrivateProfileIntW(L"Palette",L"HighlightStrength",100,path)),0,100);
    GetPrivateProfileStringW(L"Palette",L"HighlightSourceColor",L"",source,64,path);
    customHighlightSource=source[0]!=0;
    highlightSourceColor=0x1473e6;
    if(customHighlightSource&&!ParseRgb(source,highlightSourceColor))highlightEnabled=false;
}
bool InstallPalette(const wchar_t* path) {
    ReadPaletteSettings(path);
    if(!enabled)return true;
    wcscpy_s(paletteLog,path);auto slash=wcsrchr(paletteLog,L'\\');if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-paletteLog),L"palette.log");
    int floor=std::clamp(int(GetPrivateProfileIntW(L"Palette",L"BlackLevel",24,path)),0,40);
    int white=std::clamp(int(GetPrivateProfileIntW(L"Palette",L"WhiteLevel",192,path)),160,255);
    const int x[]={0,16,32,64,96,128,160,192,224,255};
    const int y[]={floor,floor,32,64,96,122,148,166,182,white};
    for(int v=0;v<256;v++) {
        int k=0;while(k<8&&v>x[k+1])k++;
        neutral[v]=uint8_t(std::clamp(y[k]+(y[k+1]-y[k])*(v-x[k])/(x[k+1]-x[k]),0,255));
    }
    PreparePalette();
    MH_STATUS status=MH_CreateHook(reinterpret_cast<void*>(&StretchDIBits),reinterpret_cast<void*>(&blitHook),reinterpret_cast<void**>(&originalBlit));
    if(status!=MH_OK)return false;
    HMODULE gl=LoadLibraryW(L"opengl32.dll");
    auto clear=gl?GetProcAddress(gl,"glClearColor"):nullptr;
    if(clear && MH_CreateHook(reinterpret_cast<void*>(clear),reinterpret_cast<void*>(&clearHook),reinterpret_cast<void**>(&originalClear))!=MH_OK)return false;
    return true;
}
