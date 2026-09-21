// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Experimental Blu-ray playlist bridge. Public methods run on the UI thread;
// asynchronous Java graphics are copied through a protected frame buffer.
// Video/audio decoding stays in MPC-BE's existing DirectShow graph.
class CBlurayMenu
{
    struct State;
    std::unique_ptr<State> m;
public:
    CBlurayMenu();
    ~CBlurayMenu();
    bool Start(const CStringW& root, CStringW& error);
    CStringW DiscName() const;
    CStringW StorageKey() const;
    CStringW StoragePath() const;
    static CStringW DataDirectory();
    bool AttachRenderer(IUnknown* renderer);
    void DetachRenderer();
    bool RendererBusy() const;
    REFERENCE_TIME Tick(REFERENCE_TIME position, bool running, double rate);
    REFERENCE_TIME PlaybackPosition(REFERENCE_TIME reported) const;
    void PlayerSeek(REFERENCE_TIME position);
    bool CanSkip() const;
    bool Skip(bool forward, REFERENCE_TIME& position, unsigned& chapter, unsigned& chapterCount);
    void Complete();
    bool GraphComplete();
    bool NeedsPlaybackPause(REFERENCE_TIME position) const;
    bool PlaybackRange(REFERENCE_TIME position, REFERENCE_TIME& stop, UINT& endPlayItem);
    void PlaybackFailed(HRESULT result);
    bool TakePlaybackRequest(bool& paused);
    bool Key(UINT key);
    bool Mouse(CPoint point, const CRect& rendererRect, bool activate);
    bool MenuVisible() const;
    bool MenuActive() const;
    bool CanShowMenu(bool popup) const;
    CStringW TakeNotice();
    // -2 = unchanged; subtitle -1 = disabled; other values are transport PIDs.
    bool TakeStreams(int& audioPid, int& subtitlePid);
    void StreamResult(bool audio, int pid, HRESULT result);
    void PlaylistReused();
    bool TakePlaylist(CStringW& path);
    bool TakeSeek(REFERENCE_TIME& position);
    void SeekApplied(REFERENCE_TIME position);
    CStringW Error() const;
    void DeviceChange(UINT event, DWORD drives);
    CStringW DiscLossNotice() const;
};
