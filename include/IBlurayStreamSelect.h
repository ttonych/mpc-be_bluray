// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Translate a disc PID into IAMStreamSelect's index and per-group ordinal.
// Kept separate from the public IMpegSplitterFilter settings interface.
interface __declspec(uuid("75951DA2-2B84-47C0-8D8D-6476E25E6706"))
IBlurayStreamSelect : public IUnknown {
    STDMETHOD(FindStream)(WORD pid, DWORD* index, DWORD* group, DWORD* ordinal) PURE;
};

// Restrict the demuxer to complete play items while the disc navigator holds
// a still. Set before IMediaSeeking::SetPositions (which flushes queued data).
interface __declspec(uuid("89E5DF56-9B39-4C91-8847-6ACB52DEBD75"))
IBlurayPlaybackControl : public IUnknown {
    STDMETHOD(SetPlayItemStop)(UINT endExclusive) PURE;
};
