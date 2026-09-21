// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace BlurayAdvanced {
enum Field { Age, Profile, Restrictions, Output, Audio, Video, Display, Stereo, Uhd, UhdDisplay, Hdr, Sdr, Text, Decode, Count };
struct Spec { unsigned nativeId; const wchar_t* key; uint32_t initial; bool hex; };
inline constexpr std::array<Spec, Count> Specs{{
    {13,L"BluRayParentalAge",255,false}, {31,L"BluRayPlayerProfile",0x100,true},
    {0x102,L"BluRayUoRestrictions",5,false}, {21,L"BluRayOutputPreference",0,false},
    {15,L"BluRayAudioCapabilities",0xaaaa,true}, {29,L"BluRayVideoCapabilities",3,true},
    {23,L"BluRayDisplayCapabilities",0,true}, {24,L"BluRay3dCapabilities",0,true},
    {25,L"BluRayUhdCapabilities",0,true}, {26,L"BluRayUhdDisplayCapabilities",0,true},
    {27,L"BluRayHdrPreference",0,true}, {28,L"BluRaySdrPreference",0,true},
    {30,L"BluRayTextCapabilities",0x1ffff,true}, {0x100,L"BluRayDecodePg",0,false}
}};
struct Value { bool enabled = false; uint32_t number = 0; };
using Values = std::array<Value, Count>;
inline bool Valid(size_t field, uint32_t n) {
    switch (field) {
    case Age: return n <= 255;
    case Profile: return n == 0x100 || n == 0x10110 || n == 0x30200 || n == 0x80200 || n == 0x130240 || n == 0x300 || n == 0x310;
    case Restrictions: return n == 0 || n == 5 || n == 10 || n == 20;
    case Output: case Decode: return n <= 1;
    default: return field < Count;
    }
}
// Strict decimal or 0x hexadecimal, with overflow detection. No signed values.
inline bool Parse(const std::wstring& text, uint32_t& out) {
    size_t i = 0; unsigned base = 10;
    if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) { base = 16; i = 2; }
    if (i == text.size()) return false;
    uint64_t n = 0;
    for (; i < text.size(); ++i) {
        const wchar_t c = text[i];
        unsigned d = c >= L'0' && c <= L'9' ? c-L'0' : c >= L'a' && c <= L'f' ? c-L'a'+10 : c >= L'A' && c <= L'F' ? c-L'A'+10 : 99;
        if (d >= base || (n = n * base + d) > UINT32_MAX) return false;
    }
    out = uint32_t(n); return true;
}
}
