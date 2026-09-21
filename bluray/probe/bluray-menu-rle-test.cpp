#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../src/apps/mplayerc/BlurayMenuRle.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

static std::vector<unsigned> Decode(const BD_PG_RLE_ELEM* runs, unsigned w, unsigned h)
{
    std::vector<unsigned> result(std::size_t(w) * h, 999);
    assert(BlurayMenuRle::Decode(runs, w, h,
        [&](unsigned x, unsigned y, unsigned n, unsigned color) {
            for (unsigned j = 0; j < n; ++j) {
                const auto at = std::size_t(y) * w + x + j;
                assert(at < result.size() && result[at] == 999);
                result[at] = color;
            }
        }));
    for (auto c : result) assert(c <= 255);
    return result;
}

int main(int argc, char** argv)
{
    // The Kingdom's narrow crop begins with {len=0, color=79}.
    const BD_PG_RLE_ELEM crop[] = {{0,79},{1,46},{1,255},{0,0},
        {0,71},{1,255},{0,46},{1,56},{0,0},{2,255}};
    assert((Decode(crop,2,3) == std::vector<unsigned>{46,255,255,56,255,255}));
    const BD_PG_RLE_ELEM plain[] = {{2,1},{2,2}};
    assert((Decode(plain,2,2) == std::vector<unsigned>{1,1,2,2}));
    unsigned painted=0;
    auto paint=[&](unsigned,unsigned,unsigned,unsigned){++painted;};
    const BD_PG_RLE_ELEM tooLong[]={{3,1}}, badColor[]={{2,256}};
    assert(!BlurayMenuRle::Decode(tooLong,2,1,paint));
    assert(!BlurayMenuRle::Decode(badColor,2,1,paint));
    assert(!BlurayMenuRle::Decode(nullptr,2,1,paint));
    const BD_PG_RLE_ELEM noProgress[6] = {};
    assert(!BlurayMenuRle::Decode(noProgress,2,1,paint));
    assert(!painted);

    // A complete image may end exactly at the allocation boundary.
    SYSTEM_INFO sys{}; GetSystemInfo(&sys);
    auto mem=static_cast<unsigned char*>(VirtualAlloc(nullptr,sys.dwPageSize*2,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    assert(mem);
    DWORD old; assert(VirtualProtect(mem+sys.dwPageSize,sys.dwPageSize,PAGE_NOACCESS,&old));
    auto last=reinterpret_cast<BD_PG_RLE_ELEM*>(mem+sys.dwPageSize)-1;
    *last={2,7};
    assert((Decode(last,2,1) == std::vector<unsigned>{7,7}));
    assert(VirtualFree(mem,0,MEM_RELEASE));

    if(argc>1) {
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate); assert(f);
        auto size=static_cast<std::size_t>(f.tellg()); assert(size>=16 && (size-16)%4==0);
        f.seekg(0); uint32_t header[4]; f.read(reinterpret_cast<char*>(header),16);
        std::vector<BD_PG_RLE_ELEM> runs((size-16)/4);
        f.read(reinterpret_cast<char*>(runs.data()),size-16); assert(f);
        auto pixels=Decode(runs.data(),header[2],header[3]);
        assert(header[2]==2 && header[3]==1080 && pixels[0]==46 && pixels[1]==255);
        std::printf("Real disc RLE: %zu runs, %zu pixels, decoded successfully.\n",runs.size(),pixels.size());
    }
    std::puts("PASS: cropped empty runs, row markers, bounds, palette indices, non-progress and final allocation boundary.");
}
