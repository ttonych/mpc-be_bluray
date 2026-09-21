#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../src/apps/mplayerc/BlurayMenuBackground.h"
#include <cassert>
#include <cstdio>
#include <vector>

template<class T> static T Resolve(HMODULE dll, const char* name)
{
    auto address=GetProcAddress(dll,name);
    T result; static_assert(sizeof(result)==sizeof(address));
    std::memcpy(&result,&address,sizeof(result));
    return result;
}

int main(int argc, char** argv)
{
    BLURAY_CLIP_INFO clip{};
    std::memcpy(clip.clip_id,"00051",6);
    clip.video_stream_count=1; clip.in_time=1048560; clip.out_time=6363870;
    std::vector<BLURAY_CLIP_INFO> clips(76,clip);
    BLURAY_TITLE_INFO title{};title.clip_count=76;title.clips=clips.data();
    assert(!IsBlurayMenuBackground(nullptr));
    assert(IsBlurayMenuBackground(&title));
    // An intro followed by the same short menu background is still a loop.
    std::memcpy(clips[0].clip_id,"00214",6);
    assert(IsBlurayMenuBackground(&title));
    // Seamless branching with different clips is not a menu background.
    std::memcpy(clips[35].clip_id,"00052",6);
    assert(!IsBlurayMenuBackground(&title));clips[35]=clip;
    // Reusing the same source with different edit ranges is not repetition.
    clips[35].in_time+=90000;
    assert(!IsBlurayMenuBackground(&title));clips[35]=clip;
    // Browsable still galleries keep their ordinary navigation.
    clips[35].still_mode=1;
    assert(!IsBlurayMenuBackground(&title));clips[35]=clip;
    // A single feature with many chapters must keep its chapter markers.
    title.clip_count=1;title.chapter_count=76;
    assert(!IsBlurayMenuBackground(&title));
    title.clip_count=76;
    for(auto& c:clips)c.out_time=c.in_time+3600*90000ULL;
    assert(!IsBlurayMenuBackground(&title));
    if(argc==3) {
        HMODULE dll=LoadLibraryExA(argv[1],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);assert(dll);
        auto open=Resolve<decltype(&bd_open)>(dll,"bd_open");
        auto close=Resolve<decltype(&bd_close)>(dll,"bd_close");
        auto get=Resolve<decltype(&bd_get_playlist_info)>(dll,"bd_get_playlist_info");
        auto release=Resolve<decltype(&bd_free_title_info)>(dll,"bd_free_title_info");
        assert(open && close && get && release);
        auto bd=open(argv[2],nullptr);assert(bd);
        // Integration fixtures from The Kingdom, not production disc IDs.
        for(auto id:{99u,2u,0u,666u}) {
            auto info=get(bd,id,0);assert(info);
            const bool background=IsBlurayMenuBackground(info);
            assert(background==(id!=0));
            std::printf("Playlist %05u: background=%d, clips=%u, chapters=%u\n",
                id,background,info->clip_count,info->chapter_count);
            release(info);
        }
        close(bd);FreeLibrary(dll);
    }
    std::puts("PASS: menu loops, intro plus loop, branching, edited ranges, still gallery and feature chapters.");
}
