#include "apu/apu.hpp"
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
	  dmc_irq_flag_(false), cycle_count_(0), cpu_(nullptr) {
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

		// Clock frame counter every CPU cycle
		clock_frame_counter();

		// Clock triangle at CPU rate
		if (triangle_.enabled && triangle_.length_counter > 0 && triangle_.linear_counter > 0) {
			triangle_.clock_timer();
		}

		// Clock other channels at half CPU rate
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
				dmc_.clock_timer();
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
					std::cout << "APU: Frame IRQ generated at cycle " << cycle_count_ << std::endl;
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
		}

		// Clear DMC IRQ
		dmc_irq_flag_ = false;

		std::cout << "APU: Status register write $" << std::hex << (int)value << std::dec << std::endl;
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

		std::cout << "APU: Frame counter write $" << std::hex << (int)value
				  << " mode=" << (frame_counter_.mode ? "5-step" : "4-step")
				  << " irq_inhibit=" << frame_counter_.irq_inhibit << std::dec << std::endl;
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
	// Placeholder - implement later
	return 0.0f;
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
	// Simplified sweep implementation
	(void)is_pulse1; // TODO: Implement pulse 1 vs pulse 2 sweep differences
	if (sweep_reload) {
		sweep_divider = sweep_period;
		sweep_reload = false;
	} else if (sweep_divider == 0) {
		sweep_divider = sweep_period;
		// TODO: Implement sweep logic
	} else {
		sweep_divider--;
	}
}

uint8_t APU::PulseChannel::get_output() {
	if (!enabled || length_counter == 0 || timer_period < 8) {
		return 0;
	}

	uint8_t duty_output = APU::DUTY_TABLE[duty][duty_sequence_pos];
	if (duty_output == 0) {
		return 0;
	}

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
void APU::DMCChannel::clock_timer() {
	if (timer == 0) {
		timer = timer_period;

		if (!silence) {
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

		shift_register >>= 1;
		bits_remaining--;

		if (bits_remaining == 0) {
			bits_remaining = 8;
			// TODO: Load next sample byte
			silence = true;
		}
	} else {
		timer--;
	}
}

void APU::DMCChannel::start_sample() {
	current_address = sample_address;
	bytes_remaining = sample_length;
}

uint8_t APU::DMCChannel::get_output() {
	return output_level;
}

} // namespace nes
