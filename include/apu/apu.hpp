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

	// DMC DMA cycle stealing — when the DMC sample buffer empties, the APU
	// requests a DMA fetch.  The CPU must stall for ~4 cycles and perform
	// the read, then deliver the byte via complete_dmc_dma().
	[[nodiscard]] bool is_dmc_dma_pending() const noexcept {
		return dmc_dma_pending_;
	}
	[[nodiscard]] uint16_t get_dmc_dma_address() const noexcept {
		return dmc_dma_address_;
	}
	/// Called by the CPU after it performs the DMA read.  Delivers the
	/// fetched byte into the DMC sample buffer.
	void complete_dmc_dma(uint8_t data);

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

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset);

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
		bool sequencer_trigger; // Flag set when timer reaches 0

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
		void clock_sequencer();
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
		bool sequencer_trigger; // Flag set when timer reaches 0

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
		void clock_sequencer();
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

		void clock_timer();
		void start_sample();
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
	bool prev_irq_line_state_; // Track previous IRQ line state for edge detection

	// DMC DMA tracking — the APU requests a fetch; the CPU fulfils it.
	bool dmc_dma_pending_ = false; // true while waiting for CPU to deliver byte
	uint16_t dmc_dma_address_ = 0; // address the CPU should read

	// Cycle counter
	uint64_t cycle_count_;

	// External connections
	CPU6502 *cpu_;
	SystemBus *bus_;
	AudioBackend *audio_backend_;

	// Audio output
	SampleRateConverter sample_rate_converter_;
	bool audio_enabled_;

	// Dynamic rate control: periodically nudge the resampling ratio based on
	// audio buffer fill level.  Keeps the buffer near a target level, preventing
	// drift between emulation clock and audio device clock (which causes
	// underrun/overflow clicks).
	int rate_adjust_counter_ = 0;
	static constexpr int RATE_ADJUST_INTERVAL = 512;		// check every 512 output samples (~11.6 ms)
	static constexpr std::size_t RATE_ADJUST_TARGET = 3072; // target buffer fill (stereo sample pairs)

	// NES hardware analog output filter chain.
	// Models the analog circuitry between the DAC and the audio output jack:
	//   1. First-order high-pass ~90 Hz  (mixer DC removal)
	//   2. First-order high-pass ~440 Hz (amplifier AC coupling capacitor)
	//   3. First-order low-pass  ~14 kHz (DAC output impedance + capacitance)
	// Applied at the full CPU rate (1.789773 MHz) before downsampling.
	// The low-pass filter is critical for the characteristic warm NES sound
	// and also serves as an anti-aliasing pre-filter.
	struct OutputFilter {
		// High-pass: y[n] = α * (y[n-1] + x[n] - x[n-1])
		// α = RC / (RC + dt), RC = 1/(2π·fc), dt = 1/1789773
		struct HighPass {
			float prev_input = 0.0f;
			float prev_output = 0.0f;
			float alpha = 0.0f;

			float apply(float sample) {
				prev_output = alpha * (prev_output + sample - prev_input);
				prev_input = sample;
				return prev_output;
			}

			void reset() {
				prev_input = 0.0f;
				prev_output = 0.0f;
			}
		};

		// Low-pass: y[n] = α * x[n] + (1 - α) * y[n-1]
		// α = dt / (RC + dt)
		struct LowPass {
			float prev_output = 0.0f;
			float alpha = 0.0f;

			float apply(float sample) {
				prev_output += alpha * (sample - prev_output);
				return prev_output;
			}

			void reset() {
				prev_output = 0.0f;
			}
		};

		HighPass hp_90;	 // ~90 Hz high-pass (DC removal from mixer)
		HighPass hp_440; // ~440 Hz high-pass (AC coupling capacitor)
		LowPass lp_14k;	 // ~14 kHz low-pass (DAC output filtering)

		void initialize() {
			// Coefficients for filters running at CPU clock rate (1,789,773 Hz)
			constexpr float dt = 1.0f / 1789773.0f;

			// HP 90 Hz: RC = 1/(2π×90) ≈ 0.001768
			constexpr float rc_90 = 1.0f / (6.2831853f * 90.0f);
			hp_90.alpha = rc_90 / (rc_90 + dt);

			// HP 440 Hz: RC = 1/(2π×440) ≈ 3.617e-4
			constexpr float rc_440 = 1.0f / (6.2831853f * 440.0f);
			hp_440.alpha = rc_440 / (rc_440 + dt);

			// LP 14000 Hz: RC = 1/(2π×14000) ≈ 1.137e-5
			constexpr float rc_14k = 1.0f / (6.2831853f * 14000.0f);
			lp_14k.alpha = dt / (rc_14k + dt);
		}

		float apply(float sample) {
			sample = hp_90.apply(sample);
			sample = hp_440.apply(sample);
			sample = lp_14k.apply(sample);
			return sample;
		}

		void reset() {
			hp_90.reset();
			hp_440.reset();
			lp_14k.reset();
		}
	};
	OutputFilter output_filter_;

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
