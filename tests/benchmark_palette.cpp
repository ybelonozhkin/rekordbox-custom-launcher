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
    bool ok=set(L"TintColor",L"#804020")&&set(L"TintStrength",L"50")&&set(L"BlueDimming",L"50");
    ReadPaletteSettings(path);PreparePalette();
    // Black is lifted to 24, then blended halfway towards RGB(128,64,32).
    ok=ok&&tintStrength==50&&blueDimming==50&&mapPixel(0x7f000000)==0x7f4c2c1c;
    ok=ok&&set(L"TintStrength",L"100");ReadPaletteSettings(path);PreparePalette();
    ok=ok&&mapPixel(0x7f123456)==0x7f804020;
    ok=ok&&set(L"TintColor",L"bad hex");ReadPaletteSettings(path);
    ok=ok&&tintStrength==0;
    ok=ok&&set(L"TintColor",L"804020")&&set(L"TintStrength",L"999")&&set(L"BlueDimming",L"-1");
    ReadPaletteSettings(path);ok=ok&&tintStrength==100&&blueDimming==0;
    DeleteFileW(path);
    tintStrength=0;blueDimming=0;PreparePalette();
    ok=ok&&mapPixel(0xff1473e6)==0xff1473e6;
    blueDimming=100;uint32_t muted=mapPixel(0xff1473e6);
    blueDimming=50;uint32_t brighter=mapPixel(0xff1473e6);
    ok=ok&&((brighter>>8)&255)>((muted>>8)&255)&&(brighter&255)>(muted&255);
    blueDimming=100;tintStrength=0;PreparePalette();
    return ok;
}
int main(int argc, char** argv) {
    const int x[]={0,16,32,64,96,128,160,192,224,255};
    const int y[]={24,24,32,64,96,122,148,166,182,192};
    for(int v=0;v<256;v++){int k=0;while(k<8&&v>x[k+1])k++;neutral[v]=uint8_t(y[k]+(y[k+1]-y[k])*(v-x[k])/(x[k+1]-x[k]));}
    if(!CheckSettings()){fprintf(stderr,"Palette settings/known color test failed\n");return 3;}
    printf("PASS: INI color parsing, tint endpoints, alpha, clamping and brighter blue highlights.\n");
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
    for(int variant=0;variant<2;variant++) {
        tintColor=variant?0x204080:0xc88b55;tintStrength=variant?100:5;blueDimming=variant?0:50;
        PreparePalette();if(argc==2&&std::strcmp(argv[1],"--scalar")==0)haveAvx2=false;
        for(unsigned chunk=0;chunk<256;chunk++) {
            for(unsigned i=0;i<65536;i++)all[i]=0x7f000000|(chunk<<16)|i;
            RenderPixels(mapped.data(),all.data(),all.size());
            for(unsigned i=0;i<65536;i++)if(mapped[i]!=mapPixel(all[i]))return 4;
        }
    }
    printf("PASS: tinted palettes match scalar output for all RGB values; alpha preserved.\n");
}
