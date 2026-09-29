#include "../../src/apps/mplayerc/BlurayMenuColor.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    BLURAY_STREAM_INFO video{};
    assert(!BlurayMenuColor::IsPq2020(video));
    video.color_space = BLURAY_COLOR_SPACE_BT2020;
    assert(!BlurayMenuColor::IsPq2020(video)); // UHD does not imply HDR
    video.dynamic_range_type = BLURAY_DYNAMIC_RANGE_HDR10;
    assert(BlurayMenuColor::IsPq2020(video));
    video.dynamic_range_type = BLURAY_DYNAMIC_RANGE_DOLBY_VISION;
    assert(BlurayMenuColor::IsPq2020(video)); // HDR10-compatible base layer
    video.color_space = BLURAY_COLOR_SPACE_BT709;
    assert(!BlurayMenuColor::IsPq2020(video));

    // ST 2084 reference values at the nearest 8-bit codes to 0.5 and 0.58.
    const BlurayMenuColor::Tables tables;
    assert(tables.pq[0] == 0);
    assert(std::abs(tables.pq[128] * 203 - 94.0746) < 0.001);
    assert(std::abs(tables.pq[148] * 203 - 202.4245) < 0.001);
    assert(std::abs(tables.pq[255] * 203 - 10000) < 0.001);

    const uint8_t source[] = {
        159,159,159,255,  // real disc's opaque HDR white, ~305 nits
        159,159,159,64,   // antialiased edge of the same text
        86,138,147,255,   // real disc's yellow selection square, BGRA
        0,0,0,255,       // opaque black
        255,64,128,0,    // transparent retained RGB
        255,255,255,255, // PQ peak cannot overflow
        128,128,128,255  // ~94 nits must retain its intermediate brightness
    };
    uint8_t output[sizeof(source)], repeat[sizeof(source)], sdr[sizeof(source)];
    BlurayMenuColor::CopyToOsd(output, source, sizeof(source), true);
    assert(output[0] == 255 && output[1] == 255 && output[2] == 255 && output[3] == 255);
    assert(output[4] == 255 && output[5] == 255 && output[6] == 255 && output[7] == 64);
    assert(output[10] == 255 && output[9] > 200 && output[9] < 230 && output[8] < 30);
    assert(output[12] == 0 && output[13] == 0 && output[14] == 0 && output[15] == 255);
    assert(output[16] == 0 && output[17] == 0 && output[18] == 0 && output[19] == 0);
    assert(output[20] == 255 && output[21] == 255 && output[22] == 255);
    assert(output[24] >= 180 && output[24] <= 182 && output[24] == output[25] && output[25] == output[26]);
    BlurayMenuColor::CopyToOsd(repeat, source, sizeof(source), true);
    assert(std::memcmp(output, repeat, sizeof(output)) == 0);
    BlurayMenuColor::CopyToOsd(sdr, source, sizeof(source), false);
    assert(std::memcmp(sdr, source, sizeof(source)) == 0); // HDR -> SDR restores original pixels
    for (size_t i = 3; i < sizeof(source); i += 4) assert(output[i] == source[i]);

    // Exhaustive neutral ramp: no wraparound, preserved neutrality and alpha.
    uint8_t previous = 0;
    for (unsigned n = 0; n <= 255; ++n) {
        uint8_t pixel[] = {uint8_t(n), uint8_t(n), uint8_t(n), uint8_t(n)};
        uint8_t converted[4];
        BlurayMenuColor::CopyToOsd(converted, pixel, 4, true);
        assert(converted[0] >= previous && converted[0] == converted[1] && converted[1] == converted[2]);
        assert(converted[3] == n);
        previous = converted[0];
    }
    // HDMV carries limited-range Y'CbCr palettes, including HDR menus.
    // The same retained indices must render correctly after palette changes
    // and SDR/HDR transitions without modifying the source or alpha.
    std::array<BD_PG_PALETTE_ENTRY, 256> palette{};
    palette[0] = {16, 128, 128, 255};     // black
    palette[1] = {159, 128, 128, 255};    // PQ white is only RGB 167 without conversion
    palette[2] = {159, 128, 128, 64};     // antialiased white
    palette[3] = {136, 133, 63, 255};     // BT.2020 PQ yellow near reference white
    palette[4] = {235, 128, 128, 255};    // SDR white
    palette[5] = {97, 200, 80, 255};      // chromatic SDR control
    palette[255] = {235, 128, 128, 255};  // reserved transparent index wins
    const auto originalPalette = palette;
    const uint8_t indices[] = {0, 1, 2, 3, 4, 5, 255};
    uint8_t yuv[sizeof(indices) * 4];
    for (size_t i = 0; i < sizeof(indices); ++i) {
        const auto& c = palette[indices[i]];
        yuv[4*i] = c.Y; yuv[4*i+1] = c.Cr; yuv[4*i+2] = c.Cb;
        yuv[4*i+3] = indices[i] == 255 ? 0 : c.T;
    }
    const auto originalYuv = std::vector<uint8_t>(yuv, yuv + sizeof(yuv));
    uint8_t paletteSdr[sizeof(indices) * 4], paletteHdr[sizeof(paletteSdr)], restored[sizeof(paletteSdr)];
    BlurayMenuColor::CopyYcbcrToOsd(paletteSdr, yuv, sizeof(yuv), false);
    assert(paletteSdr[0] == 0 && paletteSdr[3] == 255);
    assert(paletteSdr[4] == 167 && paletteSdr[5] == 167 && paletteSdr[6] == 167);
    assert(paletteSdr[16] == 255 && paletteSdr[17] == 255 && paletteSdr[18] == 255);
    assert(paletteSdr[20] == 0 && paletteSdr[21] == 66 && paletteSdr[22] == 223);
    BlurayMenuColor::CopyYcbcrToOsd(paletteHdr, yuv, sizeof(yuv), true);
    assert(paletteHdr[4] == 255 && paletteHdr[5] == 255 && paletteHdr[6] == 255);
    assert(paletteHdr[8] == 255 && paletteHdr[9] == 255 && paletteHdr[10] == 255 && paletteHdr[11] == 64);
    assert(paletteHdr[12] < 10 && paletteHdr[13] > 245 && paletteHdr[14] > 245);
    for (size_t i = 3; i < sizeof(paletteHdr); i += 4) assert(paletteHdr[i] == paletteSdr[i]);
    for (size_t i = 24; i < 28; ++i) assert(paletteHdr[i] == 0 && paletteSdr[i] == 0);
    BlurayMenuColor::CopyYcbcrToOsd(restored, yuv, sizeof(yuv), false);
    assert(!std::memcmp(paletteSdr, restored, sizeof(restored)));
    assert(!std::memcmp(palette.data(), originalPalette.data(), sizeof(palette)));
    assert(!std::memcmp(yuv, originalYuv.data(), sizeof(yuv)));
    // A regional palette update changes only that region's retained colour.
    yuv[4] = 16; yuv[5] = yuv[6] = 128;
    BlurayMenuColor::CopyYcbcrToOsd(restored, yuv, sizeof(yuv), true);
    assert(restored[4] == 0 && restored[5] == 0 && restored[6] == 0 && restored[7] == 255);
    assert(!std::memcmp(restored + 8, paletteHdr + 8, sizeof(restored) - 8));
    const uint8_t flat[] = {159,128,128,255, 159,128,128,255, 16,128,128,255};
    uint8_t flatResult[sizeof(flat)];
    BlurayMenuColor::CopyYcbcrToOsd(flatResult, flat, sizeof(flat), true);
    assert(!std::memcmp(flatResult, flatResult + 4, 4) && flatResult[0] == 255 && flatResult[8] == 0);
    std::puts("PASS: metadata, PQ anchors, ARGB and HDMV palettes, SDR identity, alpha, colour/clip transitions");

    // Optional reconstruction of a diagnostic TGA; not a live player capture.
    if (argc == 3) {
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<uint8_t> data{std::istreambuf_iterator<char>(input), {}};
        assert(data.size() >= 18 && data[0] == 0 && data[2] == 2 && data[16] == 32);
        const size_t pixels = size_t(data[12] | (data[13] << 8)) * (data[14] | (data[15] << 8));
        assert(data.size() == 18 + 4 * pixels);
        auto converted = data;
        BlurayMenuColor::CopyToOsd(converted.data() + 18, data.data() + 18, pixels * 4, true);
        std::ofstream outputFile(argv[2], std::ios::binary);
        outputFile.write(reinterpret_cast<const char*>(converted.data()), converted.size());
        assert(outputFile.good());
    }
}
