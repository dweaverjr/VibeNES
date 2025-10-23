#include "audio/sample_rate_converter.hpp"
#include <algorithm>

namespace nes {

SampleRateConverter::SampleRateConverter(float input_rate, float output_rate)
	: ratio_(input_rate / output_rate), accumulator_(0.0f), prev_sample_(0.0f), current_sample_(0.0f),
	  has_output_(false), output_sample_(0.0f) {
}

void SampleRateConverter::input_sample(float sample) {
	// Clamp input to valid range
	sample = std::clamp(sample, -1.0f, 1.0f);

	// Store current sample
	current_sample_ = sample;

	// Increment accumulator
	accumulator_ += 1.0f;

	// Check if we've accumulated enough input samples to produce an output
	if (accumulator_ >= ratio_) {
		// Zero-order hold (nearest-neighbor sampling)
		// Just output the most recent sample - no interpolation
		// This preserves high frequencies better than linear interpolation
		output_sample_ = current_sample_;

		// Reset accumulator (keep fractional part for accuracy)
		accumulator_ -= ratio_;

		// Mark that we have an output sample ready
		has_output_ = true;
	}
}

float SampleRateConverter::get_output() {
	has_output_ = false;
	return output_sample_;
}

void SampleRateConverter::reset() {
	accumulator_ = 0.0f;
	prev_sample_ = 0.0f;
	current_sample_ = 0.0f;
	has_output_ = false;
	output_sample_ = 0.0f;
}

} // namespace nes
