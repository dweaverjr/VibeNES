#pragma once

#include <cstddef>

namespace nes {

/**
 * SampleRateConverter - Downsample from NES CPU rate to audio output rate
 *
 * The NES APU generates samples at the CPU clock rate (~1.789773 MHz for NTSC).
 * This class downsamples to a standard audio rate (typically 44.1 kHz) using
 * a box-average (moving-average) anti-aliasing filter.
 *
 * Downsampling ratio: 1789773 Hz / 44100 Hz ≈ 40.58
 * This means we output 1 sample for every ~40.58 input samples.
 *
 * Box average: accumulates all input samples within each output period and
 * outputs the mean. This is equivalent to a rectangular-window FIR filter
 * (sinc in frequency domain), providing effective anti-aliasing that prevents
 * high-frequency harmonics from folding back as audible artifacts.
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
	 * Accumulates samples and produces output when enough have been collected.
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
	 * Get the base downsampling ratio
	 */
	float get_ratio() const {
		return base_ratio_;
	}

	/**
	 * Nudge the effective resampling ratio for dynamic rate control.
	 * @param factor Multiplier on base ratio. >1.0 = fewer output samples
	 *              (buffer filling too fast), <1.0 = more output samples
	 *              (buffer draining). Clamped to [0.995, 1.005] (~8.6 cents
	 *              pitch, well below audible threshold).
	 */
	void set_rate_adjustment(float factor);

  private:
	float base_ratio_;		// Base downsampling ratio (input_rate / output_rate)
	float effective_ratio_; // Adjusted ratio used for actual resampling
	float accumulator_;		// Fractional sample position tracker
	float sum_;				// Running sum of input samples in current output period
	int count_;				// Number of input samples accumulated
	bool has_output_;		// True when output sample is ready
	float output_sample_;	// Buffered output sample
};

} // namespace nes
