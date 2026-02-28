#include "apu/apu.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include <algorithm>
#include <cstring>
#include <format>
#include <iostream>

namespace nes {

// Length counter lookup table
const uint8_t APU::LENGTH_TABLE[32] = {10, 254, 20, 2,	40, 4,	80, 6,	160, 8,	 60, 10, 14, 12, 26, 14,
									   12, 16,	24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

// Duty cycle sequences
const uint8_t APU::DUTY_TABLE[4][8] = {
	{0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
	{0, 1, 1, 0, 0, 0, 0, 0}, // 25%
	{0, 1, 1, 1, 1, 0, 0, 0}, // 50%
	{1, 0, 0, 1, 1, 1, 1, 1}  // 25% negated
};

// Triangle wave sequence
const uint8_t APU::TRIANGLE_SEQUENCE[32] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,	4,	3,	2,	1,	0,
											0,	1,	2,	3,	4,	5,	6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

// Noise period table (NTSC)
const uint16_t APU::NOISE_PERIOD_TABLE[16] = {4,   8,	16,	 32,  64,  96,	 128,  160,
											  202, 254, 380, 508, 762, 1016, 2034, 4068};

// DMC rate table (NTSC)
const uint16_t APU::DMC_RATE_TABLE[16] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54};

APU::APU()
	: frame_counter_{}, pulse1_{}, pulse2_{}, triangle_{}, noise_{}, dmc_{}, frame_irq_flag_(false),
	  dmc_irq_flag_(false), prev_irq_line_state_(false), dmc_dma_pending_(false), dmc_dma_address_(0), cycle_count_(0),
	  cpu_(nullptr), bus_(nullptr), audio_backend_(nullptr),
	  sample_rate_converter_(static_cast<float>(CPU_CLOCK_NTSC), 44100.0f), audio_enabled_(false),
	  hp_filter_prev_input_(0.0f), hp_filter_prev_output_(0.0f) {
}

void APU::power_on() {
	reset();
}

void APU::reset() {
	// Reset frame counter
	frame_counter_ = {};
	frame_counter_.irq_inhibit = true; // IRQs disabled on reset

	// Reset all channels
	pulse1_ = {};
	pulse2_ = {};
	triangle_ = {};
	noise_ = {};
	dmc_ = {};

	// Initialize noise shift register
	noise_.shift_register = 1;

	// Clear flags
	frame_irq_flag_ = false;
	dmc_irq_flag_ = false;
	prev_irq_line_state_ = false;
	dmc_dma_pending_ = false;
	dmc_dma_address_ = 0;
	cycle_count_ = 0;

	// Reset high-pass filter state
	hp_filter_prev_input_ = 0.0f;
	hp_filter_prev_output_ = 0.0f;
}

void APU::tick(CpuCycle cycles) {
	int cycle_count = static_cast<int>(cycles.count());
	for (int i = 0; i < cycle_count; i++) {
		cycle_count_++;

		// Clock frame counter at APU rate (every other CPU cycle)
		// Clock on ODD cycles (1, 3, 5, 7, ...)
		if ((cycle_count_ & 1) == 1) {
			clock_frame_counter();
		}

		// Triangle timer runs at CPU rate (ultrasonic range)
		// Output freq = CPU / (32 * (t + 1))
		triangle_.clock_timer();

		// Pulse and noise timers run at APU rate (every other CPU cycle)
		// Output freq = fCPU / (16 * (t + 1)) for pulse
		// Clock on ODD cycles (1, 3, 5, 7, ...)
		if ((cycle_count_ & 1) == 1) {
			pulse1_.clock_timer();
			pulse2_.clock_timer();
			noise_.clock_timer();
		}

		// DMC clocks at CPU rate when enabled
		if (dmc_.enabled) {
			dmc_.clock_timer();
		}

		// After clocking the DMC, check if it needs a new sample byte.
		// If the sample buffer is empty and there are bytes remaining to
		// read, request a DMA fetch.  The CPU will stall and deliver the
		// byte via complete_dmc_dma() on a subsequent cycle.
		if (dmc_.enabled && dmc_.sample_buffer_empty && dmc_.bytes_remaining > 0 && !dmc_dma_pending_) {
			dmc_dma_pending_ = true;
			dmc_dma_address_ = dmc_.current_address;
		}

		// Generate audio sample every CPU cycle
		if (audio_enabled_ && audio_backend_) {
			float sample = get_audio_sample();
			sample_rate_converter_.input_sample(sample);

			if (sample_rate_converter_.has_output()) {
				float output_sample = sample_rate_converter_.get_output();
				audio_backend_->queue_sample(output_sample);
			}
		}

		// Update IRQ line to CPU
		update_irq_line();
	}
}

void APU::clock_frame_counter() {
	// Handle reset delay
	if (frame_counter_.reset_delay > 0) {
		frame_counter_.reset_delay--;
		if (frame_counter_.reset_delay == 0) {
			frame_counter_.divider = 0;
			frame_counter_.step = 0;
		}
		return;
	}

	frame_counter_.divider++;

	uint16_t target_cycles;
	if (frame_counter_.mode == 0) {
		target_cycles = FrameCounter::STEP_CYCLES_4[frame_counter_.step];
	} else {
		target_cycles = FrameCounter::STEP_CYCLES_5[frame_counter_.step];
	}

	if (frame_counter_.divider >= target_cycles) {
		frame_counter_.divider = 0;

		if (frame_counter_.mode == 0) { // 4-step mode
			switch (frame_counter_.step) {
			case 0:
				clock_quarter_frame();
				break;
			case 1:
				clock_quarter_frame();
				clock_half_frame();
				break;
			case 2:
				clock_quarter_frame();
				break;
			case 3:
				clock_quarter_frame();
				clock_half_frame();
				// Set IRQ flag if not inhibited
				if (!frame_counter_.irq_inhibit) {
					frame_irq_flag_ = true;
				}
				break;
			}
			frame_counter_.step = (frame_counter_.step + 1) & 3;
		} else { // 5-step mode
			switch (frame_counter_.step) {
			case 0:
			case 2:
				clock_quarter_frame();
				break;
			case 1:
			case 4:
				clock_quarter_frame();
				clock_half_frame();
				break;
			case 3:
				// Nothing
				break;
			}
			frame_counter_.step = (frame_counter_.step + 1) % 5;
		}
	}
}

void APU::clock_quarter_frame() {
	// Clock envelopes and triangle linear counter
	pulse1_.clock_envelope();
	pulse2_.clock_envelope();
	noise_.clock_envelope();
	triangle_.clock_linear();
}

void APU::clock_half_frame() {
	// Clock length counters and sweep units
	pulse1_.clock_length();
	pulse1_.clock_sweep(true); // Pulse 1
	pulse2_.clock_length();
	pulse2_.clock_sweep(false); // Pulse 2
	triangle_.clock_length();
	noise_.clock_length();
}

void APU::update_irq_line() {
	if (cpu_) {
		// Determine current IRQ line state
		bool current_irq_line = (frame_irq_flag_ || dmc_irq_flag_);

		// Edge detection: only trigger on rising edge (0 -> 1 transition)
		if (current_irq_line && !prev_irq_line_state_) {
			cpu_->trigger_irq();
		}
		// Falling edge: clear IRQ line when both flags are clear
		else if (!current_irq_line && prev_irq_line_state_) {
			cpu_->clear_irq_line();
		}

		prev_irq_line_state_ = current_irq_line;
	}
}

// Register access implementation
void APU::write(uint16_t address, uint8_t value) {
	switch (address) {
	// Pulse 1
	case 0x4000:
		pulse1_.duty = (value >> 6) & 3;
		pulse1_.length_enabled = !(value & 0x20);
		pulse1_.constant_volume = (value & 0x10) != 0;
		pulse1_.envelope_volume = value & 0x0F;
		break;
	case 0x4001:
		pulse1_.sweep_enabled = (value & 0x80) != 0;
		pulse1_.sweep_period = (value >> 4) & 7;
		pulse1_.sweep_negate = (value & 0x08) != 0;
		pulse1_.sweep_shift = value & 7;
		pulse1_.sweep_reload = true;
		break;
	case 0x4002:
		pulse1_.timer_period = (pulse1_.timer_period & 0xFF00) | value;
		break;
	case 0x4003:
		pulse1_.timer_period = (pulse1_.timer_period & 0x00FF) | ((value & 7) << 8);
		if (pulse1_.enabled) {
			pulse1_.length_counter = LENGTH_TABLE[value >> 3];
		}
		pulse1_.envelope_start = true;
		pulse1_.duty_sequence_pos = 0;
		break;

	// Pulse 2
	case 0x4004:
		pulse2_.duty = (value >> 6) & 3;
		pulse2_.length_enabled = !(value & 0x20);
		pulse2_.constant_volume = (value & 0x10) != 0;
		pulse2_.envelope_volume = value & 0x0F;
		break;
	case 0x4005:
		pulse2_.sweep_enabled = (value & 0x80) != 0;
		pulse2_.sweep_period = (value >> 4) & 7;
		pulse2_.sweep_negate = (value & 0x08) != 0;
		pulse2_.sweep_shift = value & 7;
		pulse2_.sweep_reload = true;
		break;
	case 0x4006:
		pulse2_.timer_period = (pulse2_.timer_period & 0xFF00) | value;
		break;
	case 0x4007:
		pulse2_.timer_period = (pulse2_.timer_period & 0x00FF) | ((value & 7) << 8);
		if (pulse2_.enabled) {
			pulse2_.length_counter = LENGTH_TABLE[value >> 3];
		}
		pulse2_.envelope_start = true;
		pulse2_.duty_sequence_pos = 0;
		break;

	// Triangle
	case 0x4008:
		triangle_.control_flag = (value & 0x80) != 0;
		triangle_.linear_counter_period = value & 0x7F;
		break;
	case 0x400A:
		triangle_.timer_period = (triangle_.timer_period & 0xFF00) | value;
		break;
	case 0x400B:
		triangle_.timer_period = (triangle_.timer_period & 0x00FF) | ((value & 7) << 8);
		if (triangle_.enabled) {
			triangle_.length_counter = LENGTH_TABLE[value >> 3];
		}
		triangle_.linear_counter_reload = true;
		break;

	// Noise
	case 0x400C:
		noise_.length_enabled = !(value & 0x20);
		noise_.constant_volume = (value & 0x10) != 0;
		noise_.envelope_volume = value & 0x0F;
		break;
	case 0x400E:
		noise_.mode = (value & 0x80) != 0;
		noise_.timer_period = NOISE_PERIOD_TABLE[value & 0x0F];
		break;
	case 0x400F:
		if (noise_.enabled) {
			noise_.length_counter = LENGTH_TABLE[value >> 3];
		}
		noise_.envelope_start = true;
		break;

	// DMC
	case 0x4010:
		dmc_.irq_enabled = (value & 0x80) != 0;
		dmc_.loop_flag = (value & 0x40) != 0;
		dmc_.timer_period = DMC_RATE_TABLE[value & 0x0F];
		break;
	case 0x4011:
		dmc_.output_level = value & 0x7F;
		break;
	case 0x4012:
		dmc_.sample_address = 0xC000 + (value * 64);
		break;
	case 0x4013:
		dmc_.sample_length = (value * 16) + 1;
		break;

	// Status register
	case 0x4015:
		pulse1_.enabled = (value & 0x01) != 0;
		pulse2_.enabled = (value & 0x02) != 0;
		triangle_.enabled = (value & 0x04) != 0;
		noise_.enabled = (value & 0x08) != 0;
		dmc_.enabled = (value & 0x10) != 0;

		// Clear length counters if disabled
		if (!pulse1_.enabled)
			pulse1_.length_counter = 0;
		if (!pulse2_.enabled)
			pulse2_.length_counter = 0;
		if (!triangle_.enabled)
			triangle_.length_counter = 0;
		if (!noise_.enabled) {
			noise_.length_counter = 0;
		}
		if (!dmc_.enabled) {
			dmc_.bytes_remaining = 0;
		} else if (dmc_.bytes_remaining == 0) {
			dmc_.start_sample();
			// Request DMA fetch for the first byte of the restarted sample.
			// The CPU will fulfil this on a subsequent cycle.
			if (dmc_.bytes_remaining > 0 && !dmc_dma_pending_) {
				dmc_dma_pending_ = true;
				dmc_dma_address_ = dmc_.current_address;
			}
		}

		// Clear DMC IRQ
		dmc_irq_flag_ = false;
		break;

	// Frame counter
	case 0x4017:
		frame_counter_.mode = (value & 0x80) != 0;
		frame_counter_.irq_inhibit = (value & 0x40) != 0;

		// Reset with delay
		frame_counter_.reset_delay = (cycle_count_ & 1) ? 4 : 3;

		// Clear frame IRQ if inhibited
		if (frame_counter_.irq_inhibit) {
			frame_irq_flag_ = false;
		}

		// If 5-step mode, clock immediately
		if (frame_counter_.mode) {
			clock_quarter_frame();
			clock_half_frame();
		}
		break;
	}
}

uint8_t APU::read(uint16_t address) {
	if (address == 0x4015) {
		uint8_t result = 0;
		result |= (pulse1_.length_counter > 0) ? 0x01 : 0;
		result |= (pulse2_.length_counter > 0) ? 0x02 : 0;
		result |= (triangle_.length_counter > 0) ? 0x04 : 0;
		result |= (noise_.length_counter > 0) ? 0x08 : 0;
		result |= (dmc_.bytes_remaining > 0) ? 0x10 : 0;
		result |= frame_irq_flag_ ? 0x40 : 0;
		result |= dmc_irq_flag_ ? 0x80 : 0;

		// Reading clears frame IRQ
		frame_irq_flag_ = false;

		return result;
	}

	return 0; // Open bus for other addresses
}

float APU::get_audio_sample() {
	// Get raw output from each channel
	uint8_t pulse1_out = pulse1_.get_output();
	uint8_t pulse2_out = pulse2_.get_output();
	uint8_t triangle_out = triangle_.get_output();
	uint8_t noise_out = noise_.get_output();
	uint8_t dmc_out = dmc_.get_output();

	// NES APU uses non-linear mixing to prevent overflow
	// Formula from NESDev wiki: https://www.nesdev.org/wiki/APU_Mixer
	//
	// Output ranges:
	//   Pulse 1/2: 0-15 (4-bit volume from envelope)
	//   Triangle:  0-15 (4-bit from 32-step sequence, but 16 unique levels)
	//   Noise:     0-15 (4-bit volume from envelope)
	//   DMC:       0-127 (7-bit PCM sample)

	// Pulse channel mixing
	// pulse_out = 95.88 / ((8128 / (pulse1 + pulse2)) + 100)
	float pulse_output = 0.0f;
	float pulse_sum = static_cast<float>(pulse1_out + pulse2_out);
	if (pulse_sum > 0.0f) {
		pulse_output = 95.88f / ((8128.0f / pulse_sum) + 100.0f);
	}

	// TND channel mixing (Triangle + Noise + DMC)
	// tnd_out = 159.79 / ((1 / (triangle/8227 + noise/12241 + dmc/22638)) + 100)
	float tnd_output = 0.0f;
	float tnd_sum = (static_cast<float>(triangle_out) / 8227.0f) + (static_cast<float>(noise_out) / 12241.0f) +
					(static_cast<float>(dmc_out) / 22638.0f);
	if (tnd_sum > 0.0f) {
		tnd_output = 159.79f / ((1.0f / tnd_sum) + 100.0f);
	}

	// Combine both outputs
	// Pulse range: 0.0 to ~0.95
	// TND range: 0.0 to ~1.59
	// Combined range: 0.0 to ~2.54
	float mixed = pulse_output + tnd_output;

	// When all channels are silent, output 0.0 to prevent popping
	if (pulse1_out == 0 && pulse2_out == 0 && triangle_out == 0 && noise_out == 0 && dmc_out == 0) {
		return 0.0f;
	}

	// Scale to reasonable range to prevent clipping
	// The 0.5 factor reduces the maximum amplitude to ~1.27 which is safe
	return mixed * 0.5f;
}

// Pulse Channel methods
void APU::PulseChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;
		duty_sequence_pos = (duty_sequence_pos + 1) & 7;
	} else {
		timer--;
	}
}

void APU::PulseChannel::clock_sequencer() {
	// Unused - kept for compatibility
}

void APU::PulseChannel::clock_length() {
	if (length_enabled && length_counter > 0) {
		length_counter--;
	}
}

void APU::PulseChannel::clock_envelope() {
	if (envelope_start) {
		envelope_start = false;
		envelope_decay_level = 15;
		envelope_divider = envelope_volume;
	} else {
		if (envelope_divider == 0) {
			envelope_divider = envelope_volume;
			if (envelope_decay_level > 0) {
				envelope_decay_level--;
			} else if (!length_enabled) {
				envelope_decay_level = 15;
			}
		} else {
			envelope_divider--;
		}
	}
}

void APU::PulseChannel::clock_sweep(bool is_pulse1) {
	// Calculate target period for sweep
	uint16_t change_amount = timer_period >> sweep_shift;
	uint16_t target_period = timer_period;

	if (sweep_negate) {
		target_period -= change_amount;
		// Pulse 1 uses one's complement, Pulse 2 uses two's complement
		if (is_pulse1) {
			target_period--;
		}
	} else {
		target_period += change_amount;
	}

	// Muting conditions:
	// 1. Timer period < 8
	// 2. Target period >= 0x800 (would overflow 11-bit timer)
	bool muted = (timer_period < 8) || (target_period >= 0x800);

	// Update period when divider reaches 0
	if (sweep_divider == 0 && sweep_enabled && sweep_shift > 0 && !muted) {
		timer_period = target_period;
	}

	// Sweep divider logic (correct NESDev order):
	// 1. If reload flag is set, reload divider and clear flag
	// 2. Else if divider is zero, reload divider
	// 3. Else decrement divider
	if (sweep_reload) {
		sweep_divider = sweep_period;
		sweep_reload = false;
	} else if (sweep_divider == 0) {
		sweep_divider = sweep_period;
	} else {
		sweep_divider--;
	}
}

uint8_t APU::PulseChannel::get_output() {
	// Muting conditions (silence the channel):
	// 1. Channel disabled
	if (!enabled) {
		return 0;
	}

	// 2. Length counter reached zero
	if (length_counter == 0) {
		return 0;
	}

	// 3. Timer period too high (>= 0x800) - sweep overflow
	if (timer_period >= 0x800) {
		return 0;
	}

	// 4. Timer period too low (< 8) - ultrasonic garbage/screeching
	// The sweep unit prevents updating to < 8, but register writes can still set it.
	// Period < 8 produces frequencies above 111kHz (way beyond audible range).
	// These create the "dial-up modem" screeching/bleeping artifacts.
	if (timer_period < 8) {
		return 0;
	}

	// 5. Check duty cycle output (wave position)
	uint8_t duty_output = APU::DUTY_TABLE[duty][duty_sequence_pos];
	if (duty_output == 0) {
		return 0; // Duty cycle is low (silent part of waveform)
	}

	// 6. Calculate volume (either constant or from envelope)
	uint8_t volume = constant_volume ? envelope_volume : envelope_decay_level;

	// 7. Silence if volume is zero (envelope decayed or explicitly set to 0)
	if (volume == 0) {
		return 0;
	}

	return volume;
}

// Triangle Channel methods
void APU::TriangleChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;
		// Sequencer only advances when both length_counter AND linear_counter are non-zero
		// This is critical: timer runs always, but sequencer is gated by the counters
		if (length_counter > 0 && linear_counter > 0) {
			sequence_pos = (sequence_pos + 1) & 31;
		}
	} else {
		timer--;
	}
}

void APU::TriangleChannel::clock_length() {
	if (!control_flag && length_counter > 0) {
		length_counter--;
	}
}

void APU::TriangleChannel::clock_linear() {
	if (linear_counter_reload) {
		linear_counter = linear_counter_period;
	} else if (linear_counter > 0) {
		linear_counter--;
	}

	if (!control_flag) {
		linear_counter_reload = false;
	}
}

uint8_t APU::TriangleChannel::get_output() {
	// Silence conditions:
	// 1. Channel disabled
	if (!enabled) {
		return 0;
	}

	// 2. Length counter is zero
	if (length_counter == 0) {
		return 0;
	}

	// 3. Linear counter is zero (specific to triangle channel)
	if (linear_counter == 0) {
		return 0;
	}

	// 4. Timer period too low - creates ultrasonic artifacts and aliasing
	// Triangle is clocked at CPU rate, so it can produce much higher frequencies than pulse
	// Period < 2 creates artifacts. Additionally, very low periods create aliasing/harsh sounds.
	// NESDev recommends muting for periods that would produce frequencies above ~12kHz
	// to avoid aliasing artifacts in the output.
	// At CPU rate 1.789773 MHz: freq = 1789773 / (32 * (period + 1))
	// For 12kHz: period ~= 4.7, so we use period < 5 as the threshold
	if (timer_period < 5) {
		return 0;
	}

	return APU::TRIANGLE_SEQUENCE[sequence_pos];
}

// Noise Channel methods
void APU::NoiseChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;

		uint16_t feedback = shift_register & 1;
		if (mode) {
			feedback ^= (shift_register >> 6) & 1;
		} else {
			feedback ^= (shift_register >> 1) & 1;
		}

		shift_register >>= 1;
		shift_register |= (feedback << 14);
	} else {
		timer--;
	}
}

void APU::NoiseChannel::clock_sequencer() {
	// Unused - kept for compatibility
}

void APU::NoiseChannel::clock_length() {
	if (length_enabled && length_counter > 0) {
		length_counter--;
	}
}

void APU::NoiseChannel::clock_envelope() {
	if (envelope_start) {
		envelope_start = false;
		envelope_decay_level = 15;
		envelope_divider = envelope_volume;
	} else {
		if (envelope_divider == 0) {
			envelope_divider = envelope_volume;
			if (envelope_decay_level > 0) {
				envelope_decay_level--;
			} else if (!length_enabled) {
				envelope_decay_level = 15;
			}
		} else {
			envelope_divider--;
		}
	}
}

uint8_t APU::NoiseChannel::get_output() {
	// Silence if channel disabled
	if (!enabled) {
		return 0;
	}

	// Silence if length counter is zero
	if (length_counter == 0) {
		return 0;
	}

	// Silence if shift register bit 0 is 1 (noise generator output is 0)
	if (shift_register & 1) {
		return 0;
	}

	// Calculate volume (either constant or from envelope)
	uint8_t volume = constant_volume ? envelope_volume : envelope_decay_level;

	// Silence if volume is zero
	if (volume == 0) {
		return 0;
	}

	return volume;
}

// DMC Channel methods
void APU::DMCChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;

		if (!silence) {
			// Update output level based on current bit in shift register
			if (shift_register & 1) {
				if (output_level <= 125) {
					output_level += 2;
				}
			} else {
				if (output_level >= 2) {
					output_level -= 2;
				}
			}
		}

		// Shift to next bit
		shift_register >>= 1;
		bits_remaining--;

		// When all 8 bits processed, load next sample byte
		if (bits_remaining == 0) {
			bits_remaining = 8;

			if (sample_buffer_empty) {
				silence = true;
			} else {
				silence = false;
				shift_register = sample_buffer;
				sample_buffer_empty = true;
				// Note: the actual DMA fetch for the NEXT byte is requested
				// below; it will be fulfilled by the CPU on a subsequent cycle.
			}
		}
	} else {
		timer--;
	}
}

void APU::DMCChannel::start_sample() {
	current_address = sample_address;
	bytes_remaining = sample_length;
	sample_buffer_empty = true;
}

// Called by the CPU after it performs the DMA read on behalf of the DMC.
void APU::complete_dmc_dma(uint8_t data) {
	dmc_dma_pending_ = false;

	dmc_.sample_buffer = data;
	dmc_.sample_buffer_empty = false;

	// Advance the DMC address pointer (wraps from $FFFF → $8000)
	dmc_.current_address++;
	if (dmc_.current_address == 0x0000) {
		dmc_.current_address = 0x8000;
	}

	dmc_.bytes_remaining--;

	// Handle loop or IRQ when sample completes
	if (dmc_.bytes_remaining == 0) {
		if (dmc_.loop_flag) {
			dmc_.start_sample(); // Restart sample
		} else if (dmc_.irq_enabled) {
			dmc_irq_flag_ = true;
		}
	}
}

uint8_t APU::DMCChannel::get_output() {
	if (!enabled) {
		return 0;
	}
	return output_level;
}

// =============================================================================
// Save State Serialization
// =============================================================================

void APU::serialize_state(std::vector<uint8_t> &buffer) const {
	// Frame counter
	buffer.push_back(static_cast<uint8_t>(frame_counter_.divider & 0xFF));
	buffer.push_back(static_cast<uint8_t>((frame_counter_.divider >> 8) & 0xFF));
	buffer.push_back(frame_counter_.step);
	buffer.push_back(frame_counter_.mode ? 1 : 0);
	buffer.push_back(frame_counter_.irq_inhibit ? 1 : 0);
	buffer.push_back(frame_counter_.reset_delay);

	// Pulse 1
	buffer.push_back(static_cast<uint8_t>(pulse1_.timer & 0xFF));
	buffer.push_back(static_cast<uint8_t>((pulse1_.timer >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(pulse1_.timer_period & 0xFF));
	buffer.push_back(static_cast<uint8_t>((pulse1_.timer_period >> 8) & 0xFF));
	buffer.push_back(pulse1_.timer_sequence_pos);
	buffer.push_back(pulse1_.sequencer_trigger ? 1 : 0);
	buffer.push_back(pulse1_.length_counter);
	buffer.push_back(pulse1_.length_enabled ? 1 : 0);
	buffer.push_back(pulse1_.envelope_volume);
	buffer.push_back(pulse1_.envelope_divider);
	buffer.push_back(pulse1_.envelope_decay_level);
	buffer.push_back(pulse1_.envelope_start ? 1 : 0);
	buffer.push_back(pulse1_.constant_volume ? 1 : 0);
	buffer.push_back(pulse1_.sweep_enabled ? 1 : 0);
	buffer.push_back(pulse1_.sweep_divider);
	buffer.push_back(pulse1_.sweep_period);
	buffer.push_back(pulse1_.sweep_negate ? 1 : 0);
	buffer.push_back(pulse1_.sweep_shift);
	buffer.push_back(pulse1_.sweep_reload ? 1 : 0);
	buffer.push_back(pulse1_.duty);
	buffer.push_back(pulse1_.duty_sequence_pos);
	buffer.push_back(pulse1_.enabled ? 1 : 0);

	// Pulse 2 (same structure)
	buffer.push_back(static_cast<uint8_t>(pulse2_.timer & 0xFF));
	buffer.push_back(static_cast<uint8_t>((pulse2_.timer >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(pulse2_.timer_period & 0xFF));
	buffer.push_back(static_cast<uint8_t>((pulse2_.timer_period >> 8) & 0xFF));
	buffer.push_back(pulse2_.timer_sequence_pos);
	buffer.push_back(pulse2_.sequencer_trigger ? 1 : 0);
	buffer.push_back(pulse2_.length_counter);
	buffer.push_back(pulse2_.length_enabled ? 1 : 0);
	buffer.push_back(pulse2_.envelope_volume);
	buffer.push_back(pulse2_.envelope_divider);
	buffer.push_back(pulse2_.envelope_decay_level);
	buffer.push_back(pulse2_.envelope_start ? 1 : 0);
	buffer.push_back(pulse2_.constant_volume ? 1 : 0);
	buffer.push_back(pulse2_.sweep_enabled ? 1 : 0);
	buffer.push_back(pulse2_.sweep_divider);
	buffer.push_back(pulse2_.sweep_period);
	buffer.push_back(pulse2_.sweep_negate ? 1 : 0);
	buffer.push_back(pulse2_.sweep_shift);
	buffer.push_back(pulse2_.sweep_reload ? 1 : 0);
	buffer.push_back(pulse2_.duty);
	buffer.push_back(pulse2_.duty_sequence_pos);
	buffer.push_back(pulse2_.enabled ? 1 : 0);

	// Triangle
	buffer.push_back(static_cast<uint8_t>(triangle_.timer & 0xFF));
	buffer.push_back(static_cast<uint8_t>((triangle_.timer >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(triangle_.timer_period & 0xFF));
	buffer.push_back(static_cast<uint8_t>((triangle_.timer_period >> 8) & 0xFF));
	buffer.push_back(triangle_.sequence_pos);
	buffer.push_back(triangle_.length_counter);
	buffer.push_back(triangle_.linear_counter);
	buffer.push_back(triangle_.linear_counter_period);
	buffer.push_back(triangle_.linear_counter_reload ? 1 : 0);
	buffer.push_back(triangle_.control_flag ? 1 : 0);
	buffer.push_back(triangle_.enabled ? 1 : 0);

	// Noise
	buffer.push_back(static_cast<uint8_t>(noise_.timer & 0xFF));
	buffer.push_back(static_cast<uint8_t>((noise_.timer >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(noise_.timer_period & 0xFF));
	buffer.push_back(static_cast<uint8_t>((noise_.timer_period >> 8) & 0xFF));
	buffer.push_back(noise_.sequencer_trigger ? 1 : 0);
	buffer.push_back(noise_.length_counter);
	buffer.push_back(noise_.length_enabled ? 1 : 0);
	buffer.push_back(noise_.envelope_volume);
	buffer.push_back(noise_.envelope_divider);
	buffer.push_back(noise_.envelope_decay_level);
	buffer.push_back(noise_.envelope_start ? 1 : 0);
	buffer.push_back(noise_.constant_volume ? 1 : 0);
	buffer.push_back(noise_.mode ? 1 : 0);
	buffer.push_back(static_cast<uint8_t>(noise_.shift_register & 0xFF));
	buffer.push_back(static_cast<uint8_t>((noise_.shift_register >> 8) & 0xFF));
	buffer.push_back(noise_.enabled ? 1 : 0);

	// DMC
	buffer.push_back(static_cast<uint8_t>(dmc_.timer & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.timer >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(dmc_.timer_period & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.timer_period >> 8) & 0xFF));
	buffer.push_back(dmc_.output_level);
	buffer.push_back(static_cast<uint8_t>(dmc_.sample_address & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.sample_address >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(dmc_.sample_length & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.sample_length >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(dmc_.current_address & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.current_address >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>(dmc_.bytes_remaining & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_.bytes_remaining >> 8) & 0xFF));
	buffer.push_back(dmc_.shift_register);
	buffer.push_back(dmc_.bits_remaining);
	buffer.push_back(dmc_.sample_buffer);
	buffer.push_back(dmc_.sample_buffer_empty ? 1 : 0);
	buffer.push_back(dmc_.silence ? 1 : 0);
	buffer.push_back(dmc_.irq_enabled ? 1 : 0);
	buffer.push_back(dmc_.loop_flag ? 1 : 0);
	buffer.push_back(dmc_.enabled ? 1 : 0);

	// Status flags
	buffer.push_back(frame_irq_flag_ ? 1 : 0);
	buffer.push_back(dmc_irq_flag_ ? 1 : 0);
	buffer.push_back(prev_irq_line_state_ ? 1 : 0);

	// DMC DMA tracking
	buffer.push_back(dmc_dma_pending_ ? 1 : 0);
	buffer.push_back(static_cast<uint8_t>(dmc_dma_address_ & 0xFF));
	buffer.push_back(static_cast<uint8_t>((dmc_dma_address_ >> 8) & 0xFF));

	// Cycle counter (64-bit)
	for (int i = 0; i < 8; ++i) {
		buffer.push_back(static_cast<uint8_t>((cycle_count_ >> (i * 8)) & 0xFF));
	}
}

void APU::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// Frame counter
	frame_counter_.divider = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	frame_counter_.step = buffer[offset++];
	frame_counter_.mode = buffer[offset++] != 0;
	frame_counter_.irq_inhibit = buffer[offset++] != 0;
	frame_counter_.reset_delay = buffer[offset++];

	// Pulse 1
	pulse1_.timer = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	pulse1_.timer_period = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	pulse1_.timer_sequence_pos = buffer[offset++];
	pulse1_.sequencer_trigger = buffer[offset++] != 0;
	pulse1_.length_counter = buffer[offset++];
	pulse1_.length_enabled = buffer[offset++] != 0;
	pulse1_.envelope_volume = buffer[offset++];
	pulse1_.envelope_divider = buffer[offset++];
	pulse1_.envelope_decay_level = buffer[offset++];
	pulse1_.envelope_start = buffer[offset++] != 0;
	pulse1_.constant_volume = buffer[offset++] != 0;
	pulse1_.sweep_enabled = buffer[offset++] != 0;
	pulse1_.sweep_divider = buffer[offset++];
	pulse1_.sweep_period = buffer[offset++];
	pulse1_.sweep_negate = buffer[offset++] != 0;
	pulse1_.sweep_shift = buffer[offset++];
	pulse1_.sweep_reload = buffer[offset++] != 0;
	pulse1_.duty = buffer[offset++];
	pulse1_.duty_sequence_pos = buffer[offset++];
	pulse1_.enabled = buffer[offset++] != 0;

	// Pulse 2 (same structure)
	pulse2_.timer = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	pulse2_.timer_period = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	pulse2_.timer_sequence_pos = buffer[offset++];
	pulse2_.sequencer_trigger = buffer[offset++] != 0;
	pulse2_.length_counter = buffer[offset++];
	pulse2_.length_enabled = buffer[offset++] != 0;
	pulse2_.envelope_volume = buffer[offset++];
	pulse2_.envelope_divider = buffer[offset++];
	pulse2_.envelope_decay_level = buffer[offset++];
	pulse2_.envelope_start = buffer[offset++] != 0;
	pulse2_.constant_volume = buffer[offset++] != 0;
	pulse2_.sweep_enabled = buffer[offset++] != 0;
	pulse2_.sweep_divider = buffer[offset++];
	pulse2_.sweep_period = buffer[offset++];
	pulse2_.sweep_negate = buffer[offset++] != 0;
	pulse2_.sweep_shift = buffer[offset++];
	pulse2_.sweep_reload = buffer[offset++] != 0;
	pulse2_.duty = buffer[offset++];
	pulse2_.duty_sequence_pos = buffer[offset++];
	pulse2_.enabled = buffer[offset++] != 0;

	// Triangle
	triangle_.timer = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	triangle_.timer_period = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	triangle_.sequence_pos = buffer[offset++];
	triangle_.length_counter = buffer[offset++];
	triangle_.linear_counter = buffer[offset++];
	triangle_.linear_counter_period = buffer[offset++];
	triangle_.linear_counter_reload = buffer[offset++] != 0;
	triangle_.control_flag = buffer[offset++] != 0;
	triangle_.enabled = buffer[offset++] != 0;

	// Noise
	noise_.timer = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	noise_.timer_period = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	noise_.sequencer_trigger = buffer[offset++] != 0;
	noise_.length_counter = buffer[offset++];
	noise_.length_enabled = buffer[offset++] != 0;
	noise_.envelope_volume = buffer[offset++];
	noise_.envelope_divider = buffer[offset++];
	noise_.envelope_decay_level = buffer[offset++];
	noise_.envelope_start = buffer[offset++] != 0;
	noise_.constant_volume = buffer[offset++] != 0;
	noise_.mode = buffer[offset++] != 0;
	noise_.shift_register = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	noise_.enabled = buffer[offset++] != 0;

	// DMC
	dmc_.timer = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.timer_period = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.output_level = buffer[offset++];
	dmc_.sample_address = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.sample_length = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.current_address = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.bytes_remaining = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	dmc_.shift_register = buffer[offset++];
	dmc_.bits_remaining = buffer[offset++];
	dmc_.sample_buffer = buffer[offset++];
	dmc_.sample_buffer_empty = buffer[offset++] != 0;
	dmc_.silence = buffer[offset++] != 0;
	dmc_.irq_enabled = buffer[offset++] != 0;
	dmc_.loop_flag = buffer[offset++] != 0;
	dmc_.enabled = buffer[offset++] != 0;

	// Status flags
	frame_irq_flag_ = buffer[offset++] != 0;
	dmc_irq_flag_ = buffer[offset++] != 0;
	prev_irq_line_state_ = buffer[offset++] != 0;

	// DMC DMA tracking
	dmc_dma_pending_ = buffer[offset++] != 0;
	dmc_dma_address_ = buffer[offset++];
	dmc_dma_address_ |= static_cast<uint16_t>(buffer[offset++]) << 8;

	// Cycle counter (64-bit)
	cycle_count_ = 0;
	for (int i = 0; i < 8; ++i) {
		cycle_count_ |= static_cast<uint64_t>(buffer[offset++]) << (i * 8);
	}
}

} // namespace nes
