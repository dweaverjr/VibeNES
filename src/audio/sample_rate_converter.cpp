#include "audio/sample_rate_converter.hpp"
#include <algorithm>

namespace nes {

SampleRateConverter::SampleRateConverter(float input_rate, float output_rate)
	: base_ratio_(input_rate / output_rate), effective_ratio_(input_rate / output_rate), accumulator_(0.0f), sum_(0.0f),
	  count_(0), has_output_(false), output_sample_(0.0f) {
}

void SampleRateConverter::input_sample(float sample) {
	// Accumulate input samples for box-average anti-aliasing.
	// Every ~40.58 input samples (at 1.789MHz→44.1kHz), we output the
	// arithmetic mean of all samples in the window.  This acts as a
	// rectangular-window FIR low-pass filter, attenuating frequencies
	// above Nyquist/2 and preventing aliasing artifacts.
	sum_ += sample;
	count_++;
	accumulator_ += 1.0f;

	if (accumulator_ >= effective_ratio_) {
		// Output the average of all accumulated samples
		output_sample_ = sum_ / static_cast<float>(count_);
		has_output_ = true;

		// Reset accumulator (keep fractional part for timing accuracy)
		accumulator_ -= effective_ratio_;
		sum_ = 0.0f;
		count_ = 0;
	}
}

float SampleRateConverter::get_output() {
	has_output_ = false;
	return output_sample_;
}

void SampleRateConverter::set_rate_adjustment(float factor) {
	// Clamp to ±0.5% — inaudible pitch shift (~8.6 cents)
	factor = std::clamp(factor, 0.995f, 1.005f);
	effective_ratio_ = base_ratio_ * factor;
}

void SampleRateConverter::reset() {
	accumulator_ = 0.0f;
	sum_ = 0.0f;
	count_ = 0;
	has_output_ = false;
	output_sample_ = 0.0f;
	effective_ratio_ = base_ratio_;
}

} // namespace nes
