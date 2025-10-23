#pragma once

#include "audio/audio_backend.hpp"
#include "audio/sample_rate_converter.hpp"
#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <memory>

namespace nes {

// Forward declarations
class SystemBus;
class CPU6502;

/**
 * NES APU (Audio Processing Unit) 2A03
 * Handles audio generation and timing-critical frame counter IRQs
 */
class APU : public Component {
  public:
	APU();
	~APU() override = default;

	// Component interface
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	const char *get_name() const noexcept override {
		return "APU";
	}

	// Register access
	void write(uint16_t address, uint8_t value);
	uint8_t read(uint16_t address);

	// IRQ handling
	bool is_frame_irq_pending() const {
		return frame_irq_flag_;
	}
	bool is_dmc_irq_pending() const {
		return dmc_irq_flag_;
	}
	void acknowledge_frame_irq() {
		frame_irq_flag_ = false;
	}
	void acknowledge_dmc_irq() {
		dmc_irq_flag_ = false;
	}

	// DMC DMA cycle stealing
	bool is_dmc_dma_active() const {
		return dmc_dma_in_progress_;
	}
	uint8_t get_dmc_stall_cycles() const {
		return dmc_stall_cycles_;
	}
	void clear_dmc_stall() {
		dmc_dma_in_progress_ = false;
		dmc_stall_cycles_ = 0;
	}

	// Audio output (future implementation)
	float get_audio_sample();

	// System connections
	void connect_cpu(CPU6502 *cpu) {
		cpu_ = cpu;
	}
	void connect_bus(SystemBus *bus) {
		bus_ = bus;
	}
	void connect_audio_backend(AudioBackend *audio_backend) {
		audio_backend_ = audio_backend;
	}

	// Audio control
	void enable_audio(bool enabled) {
		audio_enabled_ = enabled;
	}
	bool is_audio_enabled() const {
		return audio_enabled_;
	}

	// Update sample rate converter output rate (called when audio backend initializes)
	void set_output_sample_rate(float sample_rate) {
		sample_rate_converter_ = SampleRateConverter(static_cast<float>(CPU_CLOCK_NTSC), sample_rate);
	}

  private:
	// Frame Counter - CRITICAL for timing
	struct FrameCounter {
		uint16_t divider;	 // Divides CPU clock
		uint8_t step;		 // Current step (0-3 or 0-4)
		bool mode;			 // 0 = 4-step, 1 = 5-step
		bool irq_inhibit;	 // IRQ disable flag
		uint8_t reset_delay; // Delay after $4017 write

		// Frame counter timing constants
		static constexpr uint16_t STEP_CYCLES_4[4] = {7457, 7456, 7458, 7457};
		static constexpr uint16_t STEP_CYCLES_5[5] = {7457, 7456, 7458, 7457, 7452};
	};

	// Pulse Channel (2 instances)
	struct PulseChannel {
		// Timer
		uint16_t timer;
		uint16_t timer_period;
		uint8_t timer_sequence_pos;

		// Length counter
		uint8_t length_counter;
		bool length_enabled;

		// Volume envelope
		uint8_t envelope_volume;
		uint8_t envelope_divider;
		uint8_t envelope_decay_level;
		bool envelope_start;
		bool constant_volume;

		// Sweep unit (Pulse 1 and 2 differ slightly)
		bool sweep_enabled;
		uint8_t sweep_divider;
		uint8_t sweep_period;
		bool sweep_negate;
		uint8_t sweep_shift;
		bool sweep_reload;

		// Duty cycle
		uint8_t duty;
		uint8_t duty_sequence_pos;

		// Channel enable
		bool enabled;

		void clock_timer();
		void clock_length();
		void clock_sweep(bool is_pulse1);
		void clock_envelope();
		uint8_t get_output();
	};

	// Triangle Channel
	struct TriangleChannel {
		uint16_t timer;
		uint16_t timer_period;
		uint8_t sequence_pos;

		uint8_t length_counter;
		uint8_t linear_counter;
		uint8_t linear_counter_period;
		bool linear_counter_reload;
		bool control_flag;
		bool enabled;

		void clock_timer();
		void clock_length();
		void clock_linear();
		uint8_t get_output();
	};

	// Noise Channel
	struct NoiseChannel {
		uint16_t timer;
		uint16_t timer_period;

		uint8_t length_counter;
		bool length_enabled;
		uint8_t envelope_volume;
		uint8_t envelope_divider;
		uint8_t envelope_decay_level;
		bool envelope_start;
		bool constant_volume;

		bool mode; // 0 = normal, 1 = short
		uint16_t shift_register;
		bool enabled;

		void clock_timer();
		void clock_length();
		void clock_envelope();
		uint8_t get_output();
	};

	// DMC Channel
	struct DMCChannel {
		uint16_t timer;
		uint16_t timer_period;
		uint8_t output_level;

		uint16_t sample_address;
		uint16_t sample_length;
		uint16_t current_address;
		uint16_t bytes_remaining;

		uint8_t shift_register;
		uint8_t bits_remaining;
		uint8_t sample_buffer;	  // Current sample byte being processed
		bool sample_buffer_empty; // True when we need to load next byte
		bool silence;
		bool irq_enabled;
		bool loop_flag;
		bool enabled;

		void clock_timer(SystemBus *bus);
		void start_sample();
		void load_sample_byte(SystemBus *bus);
		uint8_t get_output();
	};

	// APU Components
	FrameCounter frame_counter_;
	PulseChannel pulse1_;
	PulseChannel pulse2_;
	TriangleChannel triangle_;
	NoiseChannel noise_;
	DMCChannel dmc_;

	// Status flags
	bool frame_irq_flag_;
	bool dmc_irq_flag_;

	// DMC DMA tracking
	bool dmc_dma_in_progress_;
	uint8_t dmc_stall_cycles_;

	// Cycle counter
	uint64_t cycle_count_;

	// External connections
	CPU6502 *cpu_;
	SystemBus *bus_;
	AudioBackend *audio_backend_;

	// Audio output
	SampleRateConverter sample_rate_converter_;
	bool audio_enabled_;

	// High-pass filter for DC blocking (removes DC bias from APU output)
	// This simulates the AC coupling that occurs in real hardware
	float hp_filter_prev_input_;					   // Previous input sample
	float hp_filter_prev_output_;					   // Previous output sample
	static constexpr float HP_FILTER_POLE = 0.999835f; // Pole position (~90Hz cutoff at 44.1kHz)

	// Internal methods
	void clock_frame_counter();
	void clock_quarter_frame();
	void clock_half_frame();
	void update_irq_line();

	// Lookup tables
	static const uint8_t LENGTH_TABLE[32];
	static const uint8_t DUTY_TABLE[4][8];
	static const uint16_t NOISE_PERIOD_TABLE[16];
	static const uint16_t DMC_RATE_TABLE[16];
	static const uint8_t TRIANGLE_SEQUENCE[32];
};

} // namespace nes
