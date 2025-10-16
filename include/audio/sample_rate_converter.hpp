#pragma once

#include <cstddef>

namespace nes {

/**
 * SampleRateConverter - Downsample from NES CPU rate to audio output rate
 *
 * The NES APU generates samples at the CPU clock rate (~1.789773 MHz for NTSC).
 * This class downsamples to a standard audio rate (typically 44.1 kHz) using
 * linear interpolation for quality.
 *
 * Downsampling ratio: 1789773 Hz / 44100 Hz ≈ 40.58
 * This means we output 1 sample for every ~40.58 input samples.
 */
class SampleRateConverter {
  public:
	/**
	 * Constructor
	 * @param input_rate Input sample rate (NES CPU rate, default 1789773 Hz)
	 * @param output_rate Output sample rate (audio device rate, default 44100 Hz)
	 */
	explicit SampleRateConverter(float input_rate = 1789773.0f, float output_rate = 44100.0f);

	/**
	 * Input a sample from the APU (called every CPU cycle)
	 * @param sample Audio sample from APU
	 */
	void input_sample(float sample);

	/**
	 * Check if an output sample is ready
	 * @return true if output sample available
	 */
	bool has_output() const {
		return has_output_;
	}

	/**
	 * Get the downsampled output sample
	 * Resets has_output() to false
	 * @return Downsampled audio sample
	 */
	float get_output();

	/**
	 * Reset the converter state
	 */
	void reset();

	/**
	 * Get the downsampling ratio
	 */
	float get_ratio() const {
		return ratio_;
	}

  private:
	float ratio_;		   // Downsampling ratio (input_rate / output_rate)
	float accumulator_;	   // Fractional sample position
	float prev_sample_;	   // Previous input sample (for interpolation)
	float current_sample_; // Current input sample
	bool has_output_;	   // True when output sample is ready
	float output_sample_;  // Buffered output sample
};

} // namespace nes
