// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <libbluray/overlay.h>

namespace BlurayMenuRle {
// libbluray owns and validates the input array; its overlay API has no run
// count. Keep a work bound as well as the destination bounds. Cropped objects
// can contain empty runs with a nonzero colour, including at the start of a
// row. Like end-of-line markers, these consume no destination pixels.
template<class PaintRun>
bool Decode(const BD_PG_RLE_ELEM* rle, unsigned width, unsigned height, PaintRun paint)
{
    if (!width || !height) return true;
    if (!rle) return false;
    std::size_t budget = std::size_t(width) * height * 2 + height;
    for (unsigned y = 0; y < height; ++y) {
        unsigned x = 0;
        while (x < width) {
            if (!budget--) return false;
            const auto run = *rle++;
            if (!run.len) continue;
            if (run.color > 255 || run.len > width - x) return false;
            paint(x, y, run.len, run.color);
            x += run.len;
        }
    }
    // Do not peek for an EOL after the last pixel: no trailing element is
    // required. Any marker between rows is consumed by the next iteration.
    return true;
}
}
