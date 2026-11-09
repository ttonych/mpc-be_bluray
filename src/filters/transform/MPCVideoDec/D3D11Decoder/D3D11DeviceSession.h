// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <dxgi.h>
#include <atomic>
#include <mutex>

struct AVBufferRef;

// A retained renderer can outlive several playlist decoders. The complete
// FFmpeg device context owns the immediate context and its native mutex.
class CD3D11DeviceSession
{
	friend class CD3D11Decoder;
	AVBufferRef* context = nullptr;
	DXGI_ADAPTER_DESC adapter = {};
	std::mutex mutex;
	std::atomic_uint generation{0}, activations{0};
public:
	CD3D11DeviceSession() = default;
	CD3D11DeviceSession(const CD3D11DeviceSession&) = delete;
	CD3D11DeviceSession& operator=(const CD3D11DeviceSession&) = delete;
	~CD3D11DeviceSession();
	unsigned Generation() const { return generation.load(); }
	unsigned Activations() const { return activations.load(); }
};
