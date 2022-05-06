#pragma once
#include <queue>
#include <condition_variable>
#include <mutex>

using audio_t = short;

struct Buffer {
	std::queue<audio_t> _read, _write;
	std::mutex mut;
	std::condition_variable cv;

	Buffer() = default;
	Buffer(const Buffer& other) = delete; //copy construct delete
	Buffer& operator=(const Buffer& other) = delete;
	Buffer(Buffer&& other) = default;
	Buffer& operator=(Buffer&& other) = default;
	~Buffer() = default;
};