#include "../src/palette.cpp"
#include <random>
#include <chrono>
#include <numeric>

static volatile uint32_t checksum=0;
using Clock=std::chrono::steady_clock;
static bool CheckSettings() {
    uint32_t color=0;
    if(!ParseRgb(L"#c88B55",color)||color!=0xc88b55||!ParseRgb(L"204080",color)||color!=0x204080)return false;
    if(ParseRgb(L"#12345",color)||ParseRgb(L"#GG2244",color)||ParseRgb(L"11223344",color))return false;
    wchar_t directory[MAX_PATH]{},path[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,directory)||!GetTempFileNameW(directory,L"rbp",0,path))return false;
    auto set=[&](const wchar_t* key,const wchar_t* value){return WritePrivateProfileStringW(L"Palette",key,value,path)!=FALSE;};
    bool ok=set(L"TintColor",L"#804020")&&set(L"TintStrength",L"50");
    ReadPaletteSettings(path);PreparePalette();
    // Black is lifted to 24, then blended halfway towards RGB(128,64,32).
    ok=ok&&tintStrength==50&&mapPixel(0x7f000000)==0x7f4c2c1c;
    ok=ok&&set(L"TintStrength",L"100");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0x7f123456)==0x7f804020;
    ok=ok&&set(L"TintColor",L"bad hex");ReadPaletteSettings(path);
    ok=ok&&tintStrength==0;
    ok=ok&&set(L"TintColor",L"804020")&&set(L"TintStrength",L"999");
    ReadPaletteSettings(path);ok=ok&&tintStrength==100;
    ok=ok&&set(L"TintStrength",L"0")&&set(L"HighlightSourceColor",L"#1473E6")&&set(L"HighlightColor",L"#B06C42")&&set(L"HighlightStrength",L"100");
    ReadPaletteSettings(path);PreparePalette();
    ok=ok&&highlightEnabled&&mapPixel(0x7f1473e6)==0x7fb06c42;
    // Half coverage over neutral RGB(32,32,32) retains the antialiasing background.
    ok=ok&&mapPixel(0xff1a4a83)==0xff684631&&mapPixel(0xffd23c28)==0xffd23c28;
    ok=ok&&set(L"HighlightStrength",L"50");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0xff1473e6)==0xff627094;
    ok=ok&&set(L"HighlightStrength",L"0");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0xff1473e6)==0xff1473e6;
    for(const wchar_t* target:{L"#000000",L"#FFFFFF",L"#808080",L"#20A060"}) {
        uint32_t expected=0;ParseRgb(target,expected);
        ok=ok&&set(L"HighlightColor",target)&&set(L"HighlightStrength",L"100");ReadPaletteSettings(path);PreparePalette();
        ok=ok&&mapPixel(0x7f1473e6)==(0x7f000000|expected);
    }
    ok=ok&&set(L"HighlightSourceColor",L"#D23C28");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0x7fd23c28)==0x7f20a060&&mapPixel(0xff1473e6)==0xff1473e6;
    ok=ok&&set(L"HighlightSourceColor",L"#404040");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0x7f404040)==0x7f20a060&&mapPixel(0xff414141)==BasePixel(0xff414141);
    ok=ok&&set(L"HighlightSourceColor",L"bad");ReadPaletteSettings(path);ok=ok&&!highlightEnabled;
    ok=ok&&set(L"HighlightColor",L"bad")&&set(L"HighlightSourceColor",L"");ReadPaletteSettings(path);ok=ok&&!highlightEnabled;
    ok=ok&&set(L"HighlightSourceColor",L"#1473E6")&&set(L"HighlightColor",L"#B06C42")&&set(L"HighlightStrength",L"999");
    ReadPaletteSettings(path);ok=ok&&highlightStrength==100;
    ok=ok&&set(L"HighlightStrength",L"-1");ReadPaletteSettings(path);ok=ok&&highlightStrength==0;
    ok=ok&&set(L"HighlightSourceColor",L"");ReadPaletteSettings(path);ok=ok&&!highlightEnabled;
    DeleteFileW(path);
    tintStrength=0;highlightEnabled=false;PreparePalette();
    ok=ok&&mapPixel(0xff1473e6)==0xff1473e6;
    return ok;
}
int main(int argc, char** argv) {
    const int x[]={0,16,32,64,96,128,160,192,224,255};
    const int y[]={24,24,32,64,96,122,148,166,182,192};
    for(int v=0;v<256;v++){int k=0;while(k<8&&v>x[k+1])k++;neutral[v]=uint8_t(y[k]+(y[k+1]-y[k])*(v-x[k])/(x[k+1]-x[k]));}
    if(!CheckSettings()){fprintf(stderr,"Palette settings/known color test failed\n");return 3;}
    printf("PASS: INI parsing, tint/highlight colors, arbitrary sources, endpoints, antialiasing, alpha and clamping.\n");
    // Exercise the shipped config values through the same generic replacement.
    highlightEnabled=true;highlightStrength=100;highlightSourceColor=0x1473e6;highlightColor=0x1c5aa2;
    auto start=Clock::now();PreparePalette();
    if(argc==2 && std::strcmp(argv[1],"--scalar")==0)haveAvx2=false;
    printf("SIMD available: %s; lookup table 1 KiB\n",haveAvx2?"AVX2":"scalar fallback");
    std::mt19937 rng(42);
    for(size_t count:{size_t(1888*32),size_t(1888*704),size_t(2080*1344)}) {
        std::vector<uint32_t> src(count),old(count),optimized(count);
        for(size_t i=0;i<count;i++) {
            unsigned v=rng()%256;
            src[i]=0xff000000|((i%10<8)?v*0x010101:(i%10==8?0x1473e6:rng()&0xffffff));
        }
        std::vector<double> oldTimes,newTimes;
        for(int round=0;round<20;round++) {
            start=Clock::now();memcpy(old.data(),src.data(),count*4);
            for(size_t i=0;i<count;i++)old[i]=mapPixel(src[i]);
            oldTimes.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            checksum=old[round%count];
            start=Clock::now();RenderPixels(optimized.data(),src.data(),count);
            newTimes.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            checksum=optimized[round%count];
            if(old!=optimized){fprintf(stderr,"Pixel mismatch\n");return 1;}
        }
        double before=std::accumulate(oldTimes.begin()+2,oldTimes.end(),0.0)/18;
        double after=std::accumulate(newTimes.begin()+2,newTimes.end(),0.0)/18;
        printf("%zu pixels: old %.3f ms; new %.3f ms; %.2fx faster; pixels identical\n",count,before,after,before/after);
    }
    std::vector<uint32_t> all(65536),mapped(65536);
    for(unsigned chunk=0;chunk<256;chunk++) {
        for(unsigned i=0;i<65536;i++)all[i]=0xff000000|(chunk<<16)|i;
        RenderPixels(mapped.data(),all.data(),all.size());
        for(unsigned i=0;i<65536;i++)if(mapped[i]!=mapPixel(all[i]))return 2;
    }
    printf("PASS: all 16,777,216 RGB outputs match reference exactly.\n");
    for(int variant=0;variant<5;variant++) {
        tintColor=variant==1?0x204080:0xc88b55;tintStrength=variant==1?100:5;
        highlightEnabled=variant>=2;highlightColor=0xb06c42;highlightStrength=100;
        highlightSourceColor=variant==4?0x404040:variant==3?0xd23c28:0x1473e6;
        PreparePalette();if(argc==2&&std::strcmp(argv[1],"--scalar")==0)haveAvx2=false;
        for(unsigned chunk=0;chunk<256;chunk++) {
            for(unsigned i=0;i<65536;i++)all[i]=0x7f000000|(chunk<<16)|i;
            RenderPixels(mapped.data(),all.data(),all.size());
            for(unsigned i=0;i<65536;i++)if(mapped[i]!=mapPixel(all[i]))return 4;
        }
    }
    printf("PASS: tint and arbitrary highlight colors/sources match scalar output for all RGB values; alpha preserved.\n");
}
