#include "audio/audio_backend.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace nes {

AudioBackend::AudioBackend()
	: device_id_(0), stream_(nullptr), is_initialized_(false), is_playing_(false), volume_(1.0f),
	  ring_buffer_(RING_CAPACITY, 0.0f), ring_read_(0), ring_write_(0), ring_count_(0), last_left_(0.0f),
	  last_right_(0.0f), want_playing_(false), sample_rate_(44100), buffer_size_(1024) {
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
	std::cout << "  Channels: 2 (stereo)" << std::endl;
	std::cout << "  Ring buffer: " << RING_CAPACITY << " floats (~" << (RING_CAPACITY / 2 * 1000 / sample_rate)
			  << " ms)" << std::endl;

	is_initialized_.store(true);
	return true;
}

void AudioBackend::start() {
	if (!is_initialized_.load()) {
		std::cerr << "AudioBackend: Cannot start - not initialized" << std::endl;
		return;
	}

	// Don't resume SDL device yet — wait for the ring buffer to fill to
	// PRE_BUFFER_THRESHOLD.  queue_sample_stereo() will resume once enough
	// samples are queued, eliminating startup clicks from empty-buffer underruns.
	want_playing_ = true;
	std::cout << "AudioBackend: Pre-buffering (" << (PRE_BUFFER_THRESHOLD / 2 * 1000 / sample_rate_) << " ms)..."
			  << std::endl;
}

void AudioBackend::stop() {
	if (!is_initialized_.load()) {
		return;
	}

	SDL_PauseAudioDevice(device_id_);
	is_playing_.store(false);
	want_playing_ = false;
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

	// Write to ring buffer (thread-safe)
	std::lock_guard<std::mutex> lock(ring_mutex_);

	// Drop sample if ring buffer is full (prevents unbounded growth)
	if (ring_count_ + 2 > RING_CAPACITY) {
		return;
	}

	ring_buffer_[ring_write_] = left;
	ring_write_ = (ring_write_ + 1) % RING_CAPACITY;
	ring_buffer_[ring_write_] = right;
	ring_write_ = (ring_write_ + 1) % RING_CAPACITY;
	ring_count_ += 2;

	// Pre-buffer: once we accumulate enough samples, start the SDL device.
	// This ensures the first SDL callback has a comfortable cushion of data.
	if (want_playing_ && !is_playing_.load() && ring_count_ >= PRE_BUFFER_THRESHOLD) {
		SDL_ResumeAudioDevice(device_id_);
		is_playing_.store(true);
		// (cout omitted — we're under a lock, keep it fast)
	}
}

void AudioBackend::set_volume(float volume) {
	volume_.store(std::clamp(volume, 0.0f, 1.0f));
}

std::size_t AudioBackend::get_buffer_size() const {
	std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(ring_mutex_));
	return ring_count_ / 2; // Divide by 2 for stereo → sample pairs
}

void AudioBackend::clear_buffer() {
	std::lock_guard<std::mutex> lock(ring_mutex_);
	ring_read_ = 0;
	ring_write_ = 0;
	ring_count_ = 0;
}

void AudioBackend::audio_stream_callback(void *userdata, SDL_AudioStream *stream, int additional_amount,
										 int /*total_amount*/) {
	auto *backend = static_cast<AudioBackend *>(userdata);
	if (additional_amount <= 0)
		return;

	int sample_count = additional_amount / static_cast<int>(sizeof(float));

	// Use a stack buffer for typical callback sizes to avoid heap allocation
	// on the audio thread.  4096 floats = 16 KB, covers ~46 ms at 44.1kHz stereo.
	constexpr int STACK_BUF_SIZE = 4096;
	float stack_buf[STACK_BUF_SIZE];
	float *buf = (sample_count <= STACK_BUF_SIZE) ? stack_buf : new float[sample_count];

	backend->fill_audio_buffer(buf, sample_count);
	SDL_PutAudioStreamData(stream, buf, additional_amount);

	if (buf != stack_buf) {
		delete[] buf;
	}
}

void AudioBackend::fill_audio_buffer(float *stream, int sample_count) {
	std::lock_guard<std::mutex> lock(ring_mutex_);

	int samples_to_copy = std::min(sample_count, static_cast<int>(ring_count_));

	// Copy samples from ring buffer to output — O(1) per sample, no erases.
	for (int i = 0; i < samples_to_copy; i++) {
		stream[i] = ring_buffer_[ring_read_];
		ring_read_ = (ring_read_ + 1) % RING_CAPACITY;
	}
	ring_count_ -= samples_to_copy;

	// Track the last stereo pair for smooth underrun handling
	if (samples_to_copy >= 2) {
		last_left_ = stream[samples_to_copy - 2];
		last_right_ = stream[samples_to_copy - 1];
	}

	// Underrun: instead of hard silence, exponentially decay the last sample.
	// This eliminates the abrupt signal→silence transition that causes clicks.
	// Decay factor 0.97 ≈ -40 dB in ~130 stereo pairs (3ms at 44.1kHz).
	if (samples_to_copy < sample_count) {
		for (int i = samples_to_copy; i < sample_count; i += 2) {
			last_left_ *= 0.97f;
			last_right_ *= 0.97f;
			stream[i] = last_left_;
			if (i + 1 < sample_count) {
				stream[i + 1] = last_right_;
			}
		}
	}
}

} // namespace nes
