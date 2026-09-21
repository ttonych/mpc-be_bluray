#include "../../src/apps/mplayerc/BlurayArgbBuffer.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

int main() {
    BlurayArgbBuffer b;
    BD_ARGB_OVERLAY e{};
    e.plane = 1; e.cmd = BD_ARGB_OVERLAY_INIT; e.w = 4; e.h = 3;
    b.Apply(&e);
    BlurayArgbBuffer::Frame f;
    assert(b.Take(1, f) && f.pixels.size() == 48);
    assert(!f.HasVisiblePixels());
    uint32_t image[] = {0x80402010, 0xffabcdef, 0, 0xff112233, 0xff445566, 0};
    e.cmd = BD_ARGB_OVERLAY_DRAW; e.x = 1; e.y = 1; e.w = 2; e.h = 2; e.stride = 3; e.argb = image;
    b.Apply(&e);
    assert(!b.Take(1, f)); // no partially drawn frame before FLUSH
    e.cmd = BD_ARGB_OVERLAY_FLUSH; e.pts = 123; b.Apply(&e);
    e.cmd = BD_ARGB_OVERLAY_DRAW; image[0] = 0xffffffff; b.Apply(&e);
    assert(b.Take(1, f) && f.pts == 123);
    const auto* pixels = reinterpret_cast<const uint32_t*>(f.pixels.data());
    assert(pixels[5] == 0x80402010 && pixels[6] == 0xffabcdef && pixels[9] == 0xff112233);
    assert(f.pixels[20] == 0x10 && f.pixels[23] == 0x80); // BGRA and alpha
    assert(f.HasVisiblePixels());
    assert(pixels[0] == 0 && pixels[11] == 0);
    // A scene can disappear without CLOSE; retained transparent RGB must not
    // keep stealing the player's seek/volume keys. Unflushed DRAW is invisible.
    uint32_t transparent[12];
    for (auto& pixel : transparent) pixel = 0x00ffffff;
    e.cmd = BD_ARGB_OVERLAY_DRAW; e.x = e.y = 0; e.w = 4; e.h = 3;
    e.stride = 4; e.argb = transparent; b.Apply(&e);
    assert(!b.Take(1, f) && f.HasVisiblePixels());
    e.cmd = BD_ARGB_OVERLAY_FLUSH; b.Apply(&e);
    assert(b.Take(1, f) && !f.HasVisiblePixels() && f.width == 4);
    // Reopening a popup can be a partial draw, even a translucent black pixel.
    uint32_t black = 0x01000000;
    e.cmd = BD_ARGB_OVERLAY_DRAW; e.x = 3; e.y = 2; e.w = e.h = 1;
    e.stride = 1; e.argb = &black; b.Apply(&e);
    e.cmd = BD_ARGB_OVERLAY_FLUSH; b.Apply(&e);
    assert(b.Take(1, f) && f.HasVisiblePixels());
    e.cmd = BD_ARGB_OVERLAY_CLOSE; b.Apply(&e);
    assert(b.Take(1, f) && !f.width && f.pixels.empty());
    assert(!f.HasVisiblePixels());
    assert(!b.Failed());
    e.cmd = BD_ARGB_OVERLAY_INIT; e.x = e.y = 0; e.w = 16; e.h = 16; b.Apply(&e);
    b.Take(1, f);
    std::atomic<bool> done{false};
    std::thread producer([&] {
        uint32_t data[256];
        auto event = e; event.argb = data; event.stride = 16;
        for (uint32_t n = 1; n <= 1000; ++n) {
            for (auto& pixel : data) pixel = n;
            event.cmd = BD_ARGB_OVERLAY_DRAW; b.Apply(&event);
            event.cmd = BD_ARGB_OVERLAY_FLUSH; b.Apply(&event);
        }
        done = true;
    });
    while (!done) if (b.Take(1, f)) {
        const auto* data = reinterpret_cast<const uint32_t*>(f.pixels.data());
        for (unsigned i = 1; i < 256; ++i) assert(data[i] == data[0]);
    }
    producer.join();
    b.Apply(nullptr); assert(b.Take(1, f) && f.pixels.empty());
    BlurayArgbBuffer invalid;
    e.cmd = BD_ARGB_OVERLAY_DRAW; invalid.Apply(&e);
    assert(invalid.Failed());
    std::puts("PASS: ARGB stride, clipping, alpha, visible/transparent scenes, flush isolation, close, concurrent publication, invalid input");
}
