// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BlurayMenuAudio.h"
#include "FGManager.h"
#include <IBlurayStreamSelect.h>

// The main graph stops on a browsable still. Its soundtrack needs a separate
// audio-only graph, using the same decoder/renderer settings and volume.
class BlurayMenuAudioPlayer {
    CComPtr<IGraphBuilder2> graph;
    CComQIPtr<IMediaControl> control;
    CComQIPtr<IMediaSeeking> seeking;
    CComQIPtr<IMediaEvent> events;
    CComQIPtr<IBasicAudio> audio;
    BlurayMenuAudio selected;
    CStringW root;
    size_t item = 0;
    OAFilterState requested = State_Running;
    long volume = -10000;
    bool finished = false;
    REFERENCE_TIME start = 0, stop = 0;

    void CloseGraph() {
        if (control) control->Stop();
        audio.Release(); events.Release(); seeking.Release(); control.Release(); graph.Release();
    }
    HRESULT OpenClip() {
        CloseGraph();
        if (item >= selected.clips.size()) return E_INVALIDARG;
        const auto& clip = selected.clips[item];
        auto manager = DNew CFGManagerPlayer(L"Blu-ray menu audio", nullptr, nullptr);
        graph = manager;
        const CStringW path = root + L"\\BDMV\\STREAM\\" + clip.id.c_str() + L".m2ts";
        CComQIPtr<IGraphBuilderAudio> audioBuilder = graph;
        HRESULT hr = audioBuilder ? audioBuilder->RenderAudioFile(path) : E_NOINTERFACE;
        if (FAILED(hr)) return hr;
        control = graph; seeking = graph; events = graph; audio = graph;
        if (!control || !seeking || !events || !audio) return E_NOINTERFACE;
        // Find the exact PID in this subclip, not a similarly numbered stream
        // from the still-video graph. Use the demuxer's actual PTS origin.
        hr = VFW_E_NOT_FOUND;
        BeginEnumFilters(graph, enumerator, filter) {
            CComQIPtr<IBlurayStreamSelect> mapping = filter;
            CComQIPtr<IAMStreamSelect> streams = filter;
            CComQIPtr<IBlurayPlaybackControl> timestamps = filter;
            if (!mapping || !streams || !timestamps) continue;
            DWORD index = 0, group = 0, ordinal = 0;
            hr = mapping->FindStream(WORD(selected.pid), &index, &group, &ordinal);
            if (hr != S_OK) continue;
            if (group != 1) { hr = E_UNEXPECTED; break; }
            hr = streams->Enable(index, AMSTREAMSELECTENABLE_ENABLE);
            REFERENCE_TIME origin = 0;
            if (SUCCEEDED(hr)) hr = timestamps->GetPresentationStart(&origin);
            if (SUCCEEDED(hr) && !BlurayMenuAudioRange(clip, origin, start, stop)) hr = E_INVALIDARG;
            break;
        }
        EndEnumFilters;
        if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
        hr = seeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
        if (SUCCEEDED(hr)) hr = audio->put_Volume(volume);
        if (SUCCEEDED(hr)) hr = requested == State_Running ? control->Run()
            : requested == State_Paused ? control->Pause() : control->Stop();
        return hr;
    }
public:
    ~BlurayMenuAudioPlayer() { CloseGraph(); }
    void Reset() {
        CloseGraph(); selected = {}; item = 0; finished = false;
    }
    HRESULT Select(const CStringW& discRoot, const BlurayMenuAudio& selection) {
        // Keep music playing while switching pages of the same still menu.
        if (SameBlurayMenuAudio(selected, selection)) return S_FALSE;
        Reset(); root = discRoot; selected = selection;
        const HRESULT hr = OpenClip();
        if (FAILED(hr)) { CloseGraph(); finished = true; }
        return SUCCEEDED(hr) ? S_OK : hr;
    }
    bool Active() const { return graph != nullptr; }
    OAFilterState PlaybackState() const { return finished ? State_Stopped : requested; }
    HRESULT SetState(OAFilterState state) {
        requested = state;
        if (!control) return S_OK;
        if (state == State_Running && finished) {
            finished = false; item = 0;
            const HRESULT hr = OpenClip();
            if (FAILED(hr)) { CloseGraph(); finished = true; }
            return hr;
        }
        if (state == State_Stopped) {
            finished = false;
            HRESULT hr = control->Stop();
            if (SUCCEEDED(hr)) hr = seeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
            return hr;
        }
        return state == State_Running ? control->Run() : control->Pause();
    }
    HRESULT Tick(long newVolume) {
        HRESULT hr = S_OK;
        if (newVolume != volume) {
            volume = newVolume;
            if (audio) hr = audio->put_Volume(volume);
        }
        if (FAILED(hr) || !events || finished) return hr;
        bool complete = false;
        long code; LONG_PTR a, b;
        for (unsigned i = 0; i < 64 && events->GetEvent(&code, &a, &b, 0) == S_OK; ++i) {
            if (code == EC_COMPLETE) complete = true;
            if (code == EC_ERRORABORT) hr = HRESULT(a);
            events->FreeEventParams(code, a, b);
        }
        if (SUCCEEDED(hr) && complete) {
            ++item;
            if (item == selected.clips.size() && selected.repeat) item = 0;
            if (item < selected.clips.size()) hr = OpenClip();
            else { finished = true; hr = control->Stop(); }
        }
        if (FAILED(hr)) { CloseGraph(); finished = true; }
        return hr;
    }
};
