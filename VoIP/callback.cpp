#pragma once
#include "portaudio.h"
#include "Buffer.h"
#include <iostream>

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER 240

static int CallbackInput(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	audio_t* out = static_cast<audio_t*>(outputBuffer);
	Buffer* buf = static_cast<Buffer*>(userData);
	{
		std::lock_guard<std::mutex> mut(buf->mut);
		const audio_t* in = static_cast<const audio_t*>(inputBuffer);
		for (unsigned int i = 0; i < framesPerBuffer; i++) {
			buf->_write.emplace(*in++);
		}
	}
	buf->cv.notify_all();
	return paContinue;
}