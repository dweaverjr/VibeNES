#include "audio/audio_backend.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace nes {

AudioBackend::AudioBackend()
	: device_id_(0), stream_(nullptr), is_initialized_(false), is_playing_(false), volume_(1.0f), sample_rate_(44100),
	  buffer_size_(1024) {
}

AudioBackend::~AudioBackend() {
	if (is_initialized_.load()) {
		stop();
		if (stream_) {
			SDL_DestroyAudioStream(stream_);
			stream_ = nullptr;
		}
		if (device_id_) {
			SDL_CloseAudioDevice(device_id_);
			device_id_ = 0;
		}
	}
}

bool AudioBackend::initialize(int sample_rate, int buffer_size) {
	if (is_initialized_.load()) {
		std::cerr << "AudioBackend: Already initialized" << std::endl;
		return false;
	}

	buffer_size_ = buffer_size;

	// SDL3 audio spec: stereo float32
	SDL_AudioSpec spec{};
	spec.freq = sample_rate;
	spec.format = SDL_AUDIO_F32;
	spec.channels = 2;

	// Open an audio device with desired spec
	device_id_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
	if (device_id_ == 0) {
		std::cerr << "AudioBackend: Failed to open audio device: " << SDL_GetError() << std::endl;
		return false;
	}

	// Create audio stream with callback for pull-based audio
	stream_ = SDL_CreateAudioStream(&spec, &spec);
	if (!stream_) {
		std::cerr << "AudioBackend: Failed to create audio stream: " << SDL_GetError() << std::endl;
		SDL_CloseAudioDevice(device_id_);
		device_id_ = 0;
		return false;
	}

	// Set the get callback so SDL pulls data from us
	if (!SDL_SetAudioStreamGetCallback(stream_, audio_stream_callback, this)) {
		std::cerr << "AudioBackend: Failed to set audio stream callback: " << SDL_GetError() << std::endl;
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
		SDL_CloseAudioDevice(device_id_);
		device_id_ = 0;
		return false;
	}

	// Bind stream to the audio device
	if (!SDL_BindAudioStream(device_id_, stream_)) {
		std::cerr << "AudioBackend: Failed to bind audio stream: " << SDL_GetError() << std::endl;
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
		SDL_CloseAudioDevice(device_id_);
		device_id_ = 0;
		return false;
	}

	// Store the actual sample rate
	sample_rate_ = sample_rate;

	std::cout << "AudioBackend: Initialized successfully" << std::endl;
	std::cout << "  Sample rate: " << sample_rate << " Hz" << std::endl;
	std::cout << "  Channels: 2" << std::endl;
	std::cout << "  Buffer size: " << buffer_size << " samples" << std::endl;

	// Pre-allocate buffer (4096 samples = ~93ms at 44.1kHz)
	sample_buffer_.reserve(8192);

	is_initialized_.store(true);
	return true;
}

void AudioBackend::start() {
	if (!is_initialized_.load()) {
		std::cerr << "AudioBackend: Cannot start - not initialized" << std::endl;
		return;
	}

	SDL_ResumeAudioDevice(device_id_);
	is_playing_.store(true);
	std::cout << "AudioBackend: Started" << std::endl;
}

void AudioBackend::stop() {
	if (!is_initialized_.load()) {
		return;
	}

	SDL_PauseAudioDevice(device_id_);
	is_playing_.store(false);
	clear_buffer();
	std::cout << "AudioBackend: Stopped" << std::endl;
}

void AudioBackend::pause() {
	if (is_initialized_.load() && is_playing_.load()) {
		SDL_PauseAudioDevice(device_id_);
		is_playing_.store(false);
	}
}

void AudioBackend::resume() {
	if (is_initialized_.load() && !is_playing_.load()) {
		SDL_ResumeAudioDevice(device_id_);
		is_playing_.store(true);
	}
}

void AudioBackend::queue_sample(float sample) {
	// Convert mono to stereo (duplicate to both channels)
	queue_sample_stereo(sample, sample);
}

void AudioBackend::queue_sample_stereo(float left, float right) {
	if (!is_initialized_.load()) {
		return;
	}

	// Clamp samples to valid range
	left = std::clamp(left, -1.0f, 1.0f);
	right = std::clamp(right, -1.0f, 1.0f);

	// Apply volume
	float vol = volume_.load();
	left *= vol;
	right *= vol;

	// Add to buffer (thread-safe)
	std::lock_guard<std::mutex> lock(buffer_mutex_);

	// CRITICAL: Prevent buffer from growing uncontrollably
	// If buffer is already too large, skip this sample to prevent memory issues
	const std::size_t max_buffer_size = 8192; // ~93ms at 44.1kHz stereo (reduced from 16384)
	if (sample_buffer_.size() >= max_buffer_size) {
		// Buffer full - drop this sample to prevent memory leak
		// This prevents unbounded growth when APU generates samples faster than consumption
		return;
	}

	sample_buffer_.push_back(left);
	sample_buffer_.push_back(right);
}

void AudioBackend::set_volume(float volume) {
	volume_.store(std::clamp(volume, 0.0f, 1.0f));
}

std::size_t AudioBackend::get_buffer_size() const {
	std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(buffer_mutex_));
	return sample_buffer_.size() / 2; // Divide by 2 for stereo
}

void AudioBackend::clear_buffer() {
	std::lock_guard<std::mutex> lock(buffer_mutex_);
	sample_buffer_.clear();
}

void AudioBackend::audio_stream_callback(void *userdata, SDL_AudioStream *stream, int additional_amount,
										 int /*total_amount*/) {
	auto *backend = static_cast<AudioBackend *>(userdata);
	if (additional_amount <= 0)
		return;

	int sample_count = additional_amount / static_cast<int>(sizeof(float));
	std::vector<float> temp(sample_count, 0.0f);
	backend->fill_audio_buffer(temp.data(), sample_count);
	SDL_PutAudioStreamData(stream, temp.data(), additional_amount);
}

void AudioBackend::fill_audio_buffer(float *stream, int sample_count) {
	std::lock_guard<std::mutex> lock(buffer_mutex_);

	int samples_to_copy = std::min(sample_count, static_cast<int>(sample_buffer_.size()));

	if (samples_to_copy > 0) {
		// Copy samples from buffer to output stream
		std::memcpy(stream, sample_buffer_.data(), samples_to_copy * sizeof(float));

		// Remove consumed samples from buffer
		sample_buffer_.erase(sample_buffer_.begin(), sample_buffer_.begin() + samples_to_copy);
	}

	// Fill remaining with silence if buffer underrun
	if (samples_to_copy < sample_count) {
		std::memset(stream + samples_to_copy, 0, (sample_count - samples_to_copy) * sizeof(float));

		// Uncomment for debugging buffer underruns
		// static int underrun_count = 0;
		// if (++underrun_count % 100 == 0) {
		//     std::cerr << "AudioBackend: Buffer underrun (x" << underrun_count << ")" << std::endl;
		// }
	}
}

} // namespace nes
