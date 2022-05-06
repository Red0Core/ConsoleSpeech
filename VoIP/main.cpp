#include "opus\opus.h"
#include "portaudio.h"
#include <iostream>
#include "callback.cpp"
#include "Buffer.h"
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <windows.h>

inline void GetErrorPA(int err, const std::string &&type) //throw error from portaudio
{
	if (err != paNoError) {
		throw std::runtime_error(type + " error " + Pa_GetErrorText(err));
	}
}

int main()
{
	auto err = Pa_Initialize();
	GetErrorPA(err, "Initialize");

	Buffer* buf = new Buffer(); //Audio buffer
	PaStream* stream;
	err = Pa_OpenDefaultStream(&stream,
		1,
		0,
		paInt16,
		SAMPLE_RATE,
		FRAMES_PER_BUFFER,
		CallbackInput,
		static_cast<void*>(buf));
	GetErrorPA(err, "OpenDefaultStream");

	std::atomic<bool> endStream = false;
	std::thread opus([&buf, &endStream]() {
		auto OpusGetError = [](int err) {
			if (err < 0) {
				switch (err) {
				case -1:
					std::cout << "One or more invalid/out of range arguments" << std::endl;
					break;
				case -2:
					std::cout << "Not enough bytes allocated in the buffer" << std::endl;
					break;
				case -3:
					std::cout << "An internal error was detected" << std::endl;
					break;
				case -4:
					std::cout << "The compressed data passed is corrupted" << std::endl;
					break;
				case -5:
					std::cout << "Invalid/unsupported request number" << std::endl;
					break;
				case -6:
					std::cout << "An encoder or decoder structure is invalid or already freed" << std::endl;
					break;
				case -7:
					std::cout << "Memory allocation has failed" << std::endl;
					break;
				}
			}
		};

		constexpr int channels = 1;
		std::vector<std::pair<unsigned char*, int>> storageOpusData;
		int size = opus_encoder_get_size(channels);
		OpusEncoder* enc = static_cast<OpusEncoder*>(malloc(size));
		int OErr = opus_encoder_init(enc, SAMPLE_RATE, channels, OPUS_APPLICATION_VOIP);
		OpusGetError(OErr);
		opus_encoder_ctl(enc, OPUS_SET_BITRATE(128000)); //128kbits
		opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10)); //1-10 10 is slowest
		opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
		while (!endStream) {
			{
				std::unique_lock ul(buf->mut);
				buf->cv.wait_for(ul, std::chrono::seconds(1), [&buf] { return !buf->_write.empty(); });
				buf->_read = std::move(buf->_write);
			}
			while (!buf->_read.empty()) {
				opus_int16 audio[FRAMES_PER_BUFFER];
				for (int i = 0; i != FRAMES_PER_BUFFER; ++i) {
					audio[i] = buf->_read.front();
					buf->_read.pop();
				}
				unsigned char* out = reinterpret_cast<unsigned char*>(malloc(sizeof(audio_t) * FRAMES_PER_BUFFER));
				auto len = opus_encode(enc, audio, FRAMES_PER_BUFFER, out, sizeof(audio_t) * FRAMES_PER_BUFFER);
				storageOpusData.push_back(std::make_pair(out, len));
				OpusGetError(len);
			}
		}
		opus_encoder_destroy(enc);

		PaStream* stream;
		PaError err = Pa_OpenDefaultStream(&stream,
			0,
			1,
			paInt16,
			SAMPLE_RATE,
			FRAMES_PER_BUFFER,
			nullptr,
			nullptr);
		GetErrorPA(err, "OpenDefaultStream");
		err = Pa_StartStream(stream);
		GetErrorPA(err, "StartStream");

		OpusDecoder* dec = opus_decoder_create(SAMPLE_RATE, channels, &OErr);
		for (auto& [data, len] : storageOpusData) {
			opus_int16* pcmData = static_cast<opus_int16*>(malloc(sizeof(short) * FRAMES_PER_BUFFER));
			int frame_size = opus_decode(dec, data, len, pcmData, FRAMES_PER_BUFFER, 0);
			if (frame_size > 0) {
				Pa_WriteStream(stream, pcmData, frame_size);
			}
			else std::cout << "EBAL YA VSE V JOPU";
		}

		err = Pa_CloseStream(stream);
		GetErrorPA(err, "CloseStream");
		opus_decoder_destroy(dec);
		});

	err = Pa_StartStream(stream);
	GetErrorPA(err, "StartStream");
	
	std::cout << "Text \"exit\" to end speech" << std::endl;
	std::string exitText;
	while (std::cin >> exitText)
		if (exitText == "exit") break;

	err = Pa_StopStream(stream);
	GetErrorPA(err, "StopStream");
	endStream = true;

	err = Pa_CloseStream(stream);
	GetErrorPA(err, "CloseStream");
	opus.join();
	err = Pa_Terminate();
	GetErrorPA(err, "Terminate");
	delete buf;
	return 0;
}