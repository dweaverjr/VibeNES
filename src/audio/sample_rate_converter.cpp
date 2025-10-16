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

	// Store previous sample for interpolation
	prev_sample_ = current_sample_;
	current_sample_ = sample;

	// Increment accumulator
	accumulator_ += 1.0f;

	// Check if we've accumulated enough input samples to produce an output
	if (accumulator_ >= ratio_) {
		// Calculate fractional position for interpolation
		// When accumulator hits ratio_, we output a sample
		// The fractional part tells us where we are between prev and current
		float fraction = (accumulator_ - ratio_) / ratio_;

		// Linear interpolation between previous and current sample
		// This smooths the downsampling and reduces aliasing
		output_sample_ = prev_sample_ + fraction * (current_sample_ - prev_sample_);

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
