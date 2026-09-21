// Storage isolation regression checks; all writes go to a fresh fixture.
#include "../../src/apps/mplayerc/BlurayDiscStorage.h"
#include "../../src/apps/mplayerc/BlurayCatalog.h"
#include "../../src/apps/mplayerc/BlurayAdvancedSettings.h"
#include <iostream>
#include <stdexcept>
using namespace BlurayDiscStorage;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void put(const fs::path& p, const char* text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; }
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        const fs::path fixture(argv[1]), data = fixture / L"bdj-data";
        check(!fs::exists(fixture), "fixture must be new");
        fs::create_directories(fixture);
        const std::wstring a = Key(fixture, "7fff0c8e", "88000000000000000000000000146597");
        const std::wstring b = Key(fixture, "7fff0c8e", "88000000000000000000000000146598");
        check(ValidKey(a) && ValidKey(b) && a != b, "disc IDs must stay distinct within an organization");
        fs::path persistent, cache;
        put(data / L"persistent/legacy.res", "existing saved position");
        check(Resolve(data, a, persistent, cache) && persistent == data / L"persistent", "legacy storage preserved before reset");
        const auto activeSession = persistent;
        check(Reset(data, a), "reset first disc");
        check(Resolve(data, a, persistent, cache) && persistent != activeSession && fs::is_empty(persistent), "reset must start with an empty isolated store");
        check(fs::exists(activeSession / L"legacy.res"), "active and legacy data must survive reset");
        auto firstGeneration = persistent;
        put(firstGeneration / L"new.res", "new position");
        check(Reset(data, a) && Resolve(data, a, persistent, cache) && persistent != firstGeneration && fs::is_empty(persistent), "second reset needs another empty generation");
        check(fs::exists(firstGeneration / L"new.res"), "older generation retained");
        check(Resolve(data, b, persistent, cache) && persistent == activeSession, "another disc must not be reset");
        check(!Reset(data, L"../outside") && !Resolve(data, L"../outside", persistent, cache), "path traversal rejected");
        put(data / L"discs" / a / L"active.txt", "../../outside");
        check(!Resolve(data, a, persistent, cache), "damaged marker fails closed");
        check(Reset(data, a) && Resolve(data, a, persistent, cache), "explicit reset repairs damaged marker");
        const auto disc = fixture / L"disc";
        put(disc / L"BDMV/index.bdmv", "index");
        put(disc / L"BDMV/MovieObject.bdmv", "movie object");
        put(disc / L"BDMV/PLAYLIST/00001.mpls", "playlist A");
        const auto fallback = Key(disc, "00000000", "00000000000000000000000000000000");
        check(ValidKey(fallback) && fallback.substr(0, 5) == L"hash-", "missing certificate uses metadata fingerprint");
        check(fallback == Key(disc, "", ""), "stable fingerprint");
        put(disc / L"BDMV/PLAYLIST/00001.mpls", "playlist B");
        check(fallback != Key(disc, "", ""), "different playlists must not share fallback storage");
        check(Key(fixture / L"unreadable-disc", "", "").empty(), "unreadable metadata must not invent a key");
        const auto custom=fixture/L"custom";
        check(Resolve(data,a,persistent,cache,custom/L"saved",custom/L"cache") && persistent.parent_path().filename()==a && persistent!=cache, "custom roots preserve per-disc reset generation");
        check(Resolve(data,b,persistent,cache,custom/L"saved",custom/L"cache") && persistent==custom/L"saved", "custom root without reset stays shared");
        const auto cat=fixture/L"catalog-test";
        check(BlurayCatalog::Remember(cat,a,L"Тестовый диск",L"V:\\",data/L"persistent"),"catalogue write Unicode disc name");
        auto entry=BlurayCatalog::Read(cat,a);
        check(entry.name==L"Тестовый диск" && !entry.seen.empty(),"catalogue Unicode round trip");
        entry.alias=L"Моё название";
        check(BlurayCatalog::Write(cat,entry) && BlurayCatalog::Remember(cat,a,L"Disc original",L"W:\\",data/L"persistent"),"catalogue update");
        check(BlurayCatalog::Read(cat,a).Label()==L"Моё название","remembering disc preserves user alias");
        entry.id=L"../escape";check(!BlurayCatalog::Write(cat,entry),"catalogue rejects invalid ID");
        put(cat/L"persistent/7fff2222/64f1/CoffyUHD.properties","test");
        put(cat/L"persistent/7fff4c47/4001/426173696320496e7374696e63742055_pref.txt","test");
        put(cat/L"persistent/7fff7669/4050/3b42e7eb.res","test");
        auto listing=BlurayCatalog::List(cat,cat/L"persistent");
        check(listing.size()==4 && !listing.front().legacy,"catalogue separates identified disc and three legacy groups");
        check(BlurayCatalog::FileHint(L"cb797ae3.prp").empty(),"hex hash must not be guessed as a title");
        check(BlurayCatalog::FileHint(L"426173696320496e7374696e63742055_pref.txt")==L"Basic Instinct U","decode ASCII title in legacy filename");
        uint32_t value=0;
        check(BlurayAdvanced::Parse(L"0xffffffff",value)&&value==UINT32_MAX,"full unsigned setting");
        check(!BlurayAdvanced::Parse(L"4294967296",value)&&!BlurayAdvanced::Parse(L"-1",value)&&!BlurayAdvanced::Parse(L"1garbage",value),"numeric input overflow and junk rejected");
        check(!BlurayAdvanced::Valid(BlurayAdvanced::Restrictions,6)&&BlurayAdvanced::Valid(BlurayAdvanced::Restrictions,20),"restriction levels constrained to API constants");
        check(BlurayAdvanced::Valid(BlurayAdvanced::Profile,0x130240)&&!BlurayAdvanced::Valid(BlurayAdvanced::Profile,1),"profiles use encoded API values");
        std::cout << "PASS: legacy data, per-disc isolation, repeated reset, active-session preservation, invalid markers and identity fallback\n";
        std::cout << "PASS: custom roots, Unicode catalogue, preserved aliases, honest legacy grouping and advanced value validation\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
