// Regression tests for the renderer-to-disc coordinate transform.
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include "../../src/apps/mplayerc/BlurayMenuCoordinates.h"

static unsigned checks = 0;
static void check(const char* name, POINT point, RECT renderer, RECT video,
    bool expectedHit, POINT expected = {})
{
    POINT actual{};
    const bool hit = MapBlurayMenuPoint(point, renderer, video, 1920, 1080, actual);
    if (hit != expectedHit || (hit && (actual.x != expected.x || actual.y != expected.y))) {
        std::fprintf(stderr, "%s: got hit=%d (%ld,%ld), expected hit=%d (%ld,%ld)\n",
            name, hit, actual.x, actual.y, expectedHit, expected.x, expected.y);
        std::exit(1);
    }
    ++checks;
}
int main()
{
    // 1600x720 rendering window, 1280x720 image: 160px bars on each side.
    // Original bug mapped a visible right-side button to x=1536 instead of 1680.
    const RECT renderer{10, 40, 1610, 760}, pillar{160, 0, 1440, 720};
    check("right-side button", {1290, 400}, renderer, pillar, true, {1680, 540});
    check("left-side button", {330, 400}, renderer, pillar, true, {240, 540});
    check("center", {810, 400}, renderer, pillar, true, {960, 540});
    check("left bar", {169, 400}, renderer, pillar, false);
    check("right bar", {1450, 400}, renderer, pillar, false);
    check("top-left pixel", {170, 40}, renderer, pillar, true, {0, 0});
    check("last pixel", {1449, 759}, renderer, pillar, true, {1918, 1078});
    check("exclusive bottom edge", {800, 760}, renderer, pillar, false);
    check("translated window", {1480, 660}, {200, 300, 1800, 1020}, pillar, true, {1680, 540});
    // 960x720 window with a 960x540 picture and top/bottom bars.
    check("letterbox bottom button", {480, 630}, {0, 0, 960, 720}, {0, 90, 960, 630}, false);
    check("letterbox lower area", {480, 540}, {0, 0, 960, 720}, {0, 90, 960, 630}, true, {960, 900});
    // Zoomed/cropped image: use the original negative origin, not its intersection.
    check("zoom crop", {0, 0}, {0, 0, 1280, 720}, {-320, -180, 1600, 900}, true, {320, 180});
    check("cropped outside window", {-1, 360}, {0, 0, 1280, 720}, {-320, -180, 1600, 900}, false);
    check("fullscreen", {1680, 540}, {0, 0, 1920, 1080}, {0, 0, 1920, 1080}, true, {1680, 540});
    check("smaller image", {1320, 810}, {0, 0, 1920, 1080}, {480, 270, 1440, 810}, false);
    check("smaller image right", {1320, 540}, {0, 0, 1920, 1080}, {480, 270, 1440, 810}, true, {1680, 540});
    check("empty renderer", {0, 0}, {}, {}, false);
    check("empty video", {1, 1}, {0, 0, 10, 10}, {}, false);
    check("inverted video", {1, 1}, {0, 0, 10, 10}, {10, 10, 0, 0}, false);
    std::printf("PASS: %u coordinate checks (bars, translation, zoom/crop, fullscreen, edges)\n", checks);
}
