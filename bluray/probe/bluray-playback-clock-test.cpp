#include "../../src/apps/mplayerc/BlurayPlaybackClock.h"
#include <cassert>
#include <iostream>

int main() {
    constexpr int64_t second = 10000000;
    BlurayPlaybackClock clock;
    clock.Seek(22 * second, 1000);
    // Regression: empty audio reports completion while video has 18 s left.
    assert(!clock.AcceptEnd(40 * second));
    assert(clock.Update(40 * second, 1500, true, 1.0, 40 * second) == 22 * second + second / 2);
    assert(clock.Position(40 * second) == 22 * second + second / 2);
    assert(clock.Update(40 * second, 6500, false, 1.0, 40 * second) == 22 * second + second / 2);
    assert(clock.Update(40 * second, 7500, true, 2.0, 40 * second) == 24 * second + second / 2);
    assert(!clock.AcceptEnd(40 * second));
    assert(clock.Update(40 * second, 25000, true, 1.0, 40 * second) == 40 * second);
    assert(clock.AcceptEnd(40 * second));
    // The position can reach EOS before the queued completion event arrives.
    clock.Seek(22 * second, 1000);
    assert(clock.Update(40 * second, 1030, true, 1.0, 40 * second) == 22 * second + 300000);
    assert(!clock.AcceptEnd(40 * second));
    // An intentional seek to the end still finishes immediately.
    clock.Seek(40 * second, 2000);
    assert(clock.AcceptEnd(40 * second));
    // New playlist and normal seeks use the renderer's position again.
    clock.Reset();
    assert(clock.Update(second, 3000, true, 1.0, 40 * second) == second);
    clock.Seek(30 * second, 4000);
    assert(clock.Update(30 * second + second / 2, 4500, true, 1.0, 40 * second) == 30 * second + second / 2);
    // A two-frame still segment ends before the end of the entire playlist.
    clock.Seek(0, 5000);
    assert(clock.Update(833333, 5084, true, 1.0, 833333) == 833333);
    assert(clock.AcceptEnd(833333));
    std::cout << "Blu-ray playback clock: early EOS, pause, rate, seek, reset and still checks passed\n";
}
