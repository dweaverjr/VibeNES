#pragma once

#include <SDL3/SDL.h>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

namespace nes {

/**
 * AudioBackend - SDL3 audio output interface
 *
 * Manages audio device initialization, sample buffering, and playback.
 * Operates at 44.1kHz stereo output with a fixed-size ring buffer.
 *
 * Thread-safe: Audio stream callback runs on SDL's audio thread, so all
 * ring buffer operations use a lightweight mutex.  The ring buffer avoids
 * any allocations or O(N) erases on the audio thread, preventing
 * micro-stutters and clicks.
 */
class AudioBackend {
  public:
	AudioBackend();
	~AudioBackend();

	// Disable copying (owns SDL audio device)
	AudioBackend(const AudioBackend &) = delete;
	AudioBackend &operator=(const AudioBackend &) = delete;

	/**
	 * Initialize SDL audio device
	 * @param sample_rate Target sample rate (default 44100 Hz)
	 * @param buffer_size Audio buffer size in samples (default 1024)
	 * @return true if initialization successful
	 */
	bool initialize(int sample_rate = 44100, int buffer_size = 1024);

	/**
	 * Start audio playback
	 */
	void start();

	/**
	 * Stop audio playback
	 */
	void stop();

	/**
	 * Pause audio playback
	 */
	void pause();

	/**
	 * Resume audio playback
	 */
	void resume();

	/**
	 * Queue a single mono audio sample
	 * Will be automatically converted to stereo
	 * @param sample Audio sample in range [-1.0, 1.0]
	 */
	void queue_sample(float sample);

	/**
	 * Queue a stereo audio sample
	 * @param left Left channel sample in range [-1.0, 1.0]
	 * @param right Right channel sample in range [-1.0, 1.0]
	 */
	void queue_sample_stereo(float left, float right);

	/**
	 * Set master volume
	 * @param volume Volume level [0.0 = mute, 1.0 = full]
	 */
	void set_volume(float volume);

	/**
	 * Get current master volume
	 * @return Volume level [0.0, 1.0]
	 */
	float get_volume() const {
		return volume_.load();
	}

	/**
	 * Check if audio is playing
	 */
	bool is_playing() const {
		return is_playing_.load();
	}

	/**
	 * Get number of samples currently in buffer
	 */
	std::size_t get_buffer_size() const;

	/**
	 * Get sample rate
	 */
	int get_sample_rate() const {
		return sample_rate_;
	}

	/**
	 * Clear all buffered audio
	 */
	void clear_buffer();

  private:
	SDL_AudioDeviceID device_id_;
	SDL_AudioStream *stream_;

	// Audio state
	std::atomic<bool> is_initialized_;
	std::atomic<bool> is_playing_;
	std::atomic<float> volume_;

	// Ring buffer for stereo audio (interleaved: L, R, L, R, ...)
	// Fixed capacity avoids allocations and O(N) erases on the audio thread.
	static constexpr std::size_t RING_CAPACITY = 32768; // ~372ms at 44.1kHz stereo

	// Pre-buffer threshold: don't start SDL playback until we have this many
	// stereo floats queued.  Prevents startup clicks from empty-buffer underruns.
	static constexpr std::size_t PRE_BUFFER_THRESHOLD = 4096; // ~46ms at 44.1kHz stereo

	std::vector<float> ring_buffer_;
	std::size_t ring_read_;
	std::size_t ring_write_;
	std::size_t ring_count_;
	std::mutex ring_mutex_;

	// Underrun fade state — when the ring runs dry, we exponentially decay
	// the last output sample instead of hard-cutting to silence.
	float last_left_ = 0.0f;
	float last_right_ = 0.0f;

	// Pre-buffer flag: true after start() but before we have enough samples
	bool want_playing_ = false;

	// Audio parameters
	int sample_rate_;
	int buffer_size_;

	// SDL3 audio stream callback (called from SDL's audio thread)
	static void SDLCALL audio_stream_callback(void *userdata, SDL_AudioStream *stream, int additional_amount,
											  int total_amount);

	// Instance audio callback
	void fill_audio_buffer(float *stream, int sample_count);
};

} // namespace nes
