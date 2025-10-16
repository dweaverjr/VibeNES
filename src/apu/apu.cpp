#include "apu/apu.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include <algorithm>
#include <cstring>
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
	  dmc_irq_flag_(false), dmc_dma_in_progress_(false), dmc_stall_cycles_(0), cycle_count_(0), cpu_(nullptr),
	  bus_(nullptr), audio_backend_(nullptr), sample_rate_converter_(1789773.0f, 44100.0f), audio_enabled_(false) {
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
	cycle_count_ = 0;
}

void APU::tick(CpuCycle cycles) {
	int cycle_count = static_cast<int>(cycles.count());
	for (int i = 0; i < cycle_count; i++) {
		cycle_count_++;

		// Clock frame counter at APU rate (every other CPU cycle)
		if (cycle_count_ & 1) {
			clock_frame_counter();
		}

		// Clock triangle at CPU rate
		if (triangle_.enabled && triangle_.length_counter > 0 && triangle_.linear_counter > 0) {
			triangle_.clock_timer();
		}

		// Clock other channels at half CPU rate (APU rate)
		if (cycle_count_ & 1) {
			if (pulse1_.enabled && pulse1_.length_counter > 0) {
				pulse1_.clock_timer();
			}
			if (pulse2_.enabled && pulse2_.length_counter > 0) {
				pulse2_.clock_timer();
			}
			if (noise_.enabled && noise_.length_counter > 0) {
				noise_.clock_timer();
			}
			if (dmc_.enabled) {
				dmc_.clock_timer(bus_);
			}
		}

		// Generate audio sample every CPU cycle
		if (audio_enabled_ && audio_backend_) {
			float sample = get_audio_sample();
			sample_rate_converter_.input_sample(sample);

			// Check if we have an output sample ready (downsampled to 44.1kHz)
			if (sample_rate_converter_.has_output()) {
				audio_backend_->queue_sample(sample_rate_converter_.get_output());
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
	if (cpu_ && (frame_irq_flag_ || dmc_irq_flag_)) {
		cpu_->trigger_irq();
	}
}

// Register access implementation
void APU::write(uint16_t address, uint8_t value) {
	switch (address) {
	// Pulse 1
	case 0x4000:
		pulse1_.duty = (value >> 6) & 3;
		pulse1_.length_enabled = !(value & 0x20);
		pulse1_.constant_volume = !(value & 0x10);
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
		pulse2_.constant_volume = !(value & 0x10);
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
		noise_.constant_volume = !(value & 0x10);
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
		if (!noise_.enabled)
			noise_.length_counter = 0;
		if (!dmc_.enabled) {
			dmc_.bytes_remaining = 0;
		} else if (dmc_.bytes_remaining == 0) {
			dmc_.start_sample();
			// Load first sample byte immediately
			if (bus_) {
				dmc_.load_sample_byte(bus_);
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
	// Get raw output from each channel (0-15 range for most channels)
	uint8_t pulse1_out = pulse1_.get_output();
	uint8_t pulse2_out = pulse2_.get_output();
	uint8_t triangle_out = triangle_.get_output();
	uint8_t noise_out = noise_.get_output();
	uint8_t dmc_out = dmc_.get_output();

	// NES APU uses non-linear mixing to prevent overflow
	// Formula from NESDev wiki: https://www.nesdev.org/wiki/APU_Mixer

	// Pulse channel mixing
	float pulse_output = 0.0f;
	float pulse_sum = static_cast<float>(pulse1_out + pulse2_out);
	if (pulse_sum > 0.0f) {
		pulse_output = 95.88f / ((8128.0f / pulse_sum) + 100.0f);
	}

	// TND channel mixing (Triangle + Noise + DMC)
	float tnd_output = 0.0f;
	float tnd_sum = (static_cast<float>(triangle_out) / 8227.0f) + (static_cast<float>(noise_out) / 12241.0f) +
					(static_cast<float>(dmc_out) / 22638.0f);
	if (tnd_sum > 0.0f) {
		tnd_output = 159.79f / ((1.0f / tnd_sum) + 100.0f);
	}

	// Combine both outputs
	// Output range is approximately [0.0, 1.0], center around 0.5
	float mixed = pulse_output + tnd_output;

	// Convert to signed audio range [-1.0, 1.0]
	// Normalize and center: (mixed - 0.5) * 2.0
	float output = (mixed - 0.5f) * 2.0f;

	// Final safety clamp
	if (output > 1.0f)
		output = 1.0f;
	if (output < -1.0f)
		output = -1.0f;

	return output;
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

	// Sweep divider logic
	if (sweep_divider == 0 && sweep_enabled && sweep_shift > 0 && !muted) {
		// Update timer period when divider reaches 0
		timer_period = target_period;
	}

	if (sweep_divider == 0 || sweep_reload) {
		sweep_divider = sweep_period;
		sweep_reload = false;
	} else {
		sweep_divider--;
	}
}

uint8_t APU::PulseChannel::get_output() {
	// Muting conditions:
	// 1. Channel disabled
	// 2. Length counter reached zero
	// 3. Timer period too low (< 8) - produces ultrasonic frequencies
	// 4. Timer period too high (>= 0x800) - sweep overflow
	if (!enabled || length_counter == 0 || timer_period < 8 || timer_period >= 0x800) {
		return 0;
	}

	// Check duty cycle output
	uint8_t duty_output = APU::DUTY_TABLE[duty][duty_sequence_pos];
	if (duty_output == 0) {
		return 0;
	}

	// Return volume (either constant or from envelope)
	return constant_volume ? envelope_volume : envelope_decay_level;
}

// Triangle Channel methods
void APU::TriangleChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;
		sequence_pos = (sequence_pos + 1) & 31;
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
	if (!enabled || length_counter == 0 || linear_counter == 0) {
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
	if (!enabled || length_counter == 0 || (shift_register & 1)) {
		return 0;
	}
	return constant_volume ? envelope_volume : envelope_decay_level;
}

// DMC Channel methods
void APU::DMCChannel::clock_timer(SystemBus *bus) {
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

				// Load next sample byte from memory (if available)
				if (bus && bytes_remaining > 0) {
					load_sample_byte(bus);
				}
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

void APU::DMCChannel::load_sample_byte(SystemBus *bus) {
	if (!bus || bytes_remaining == 0) {
		return;
	}

	// Note: In real hardware, this memory read steals CPU cycles
	// The APU class handles signaling this to the CPU via DMA flags

	// Read sample byte from memory
	sample_buffer = bus->read(current_address);
	sample_buffer_empty = false;

	// Increment address with wrapping
	// DMC samples wrap from $FFFF to $8000
	current_address++;
	if (current_address == 0x0000) {
		current_address = 0x8000;
	}

	bytes_remaining--;

	// Handle loop or IRQ when sample completes
	if (bytes_remaining == 0) {
		if (loop_flag) {
			start_sample(); // Restart sample
		} else if (irq_enabled) {
			// IRQ flag set in APU class (dmc_irq_flag_)
		}
	}
}

uint8_t APU::DMCChannel::get_output() {
	return output_level;
}

} // namespace nes
