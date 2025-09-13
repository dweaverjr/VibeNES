# NES APU Implementation Guide

## Overview

This document provides step-by-step instructions for implementing a cycle-accurate NES APU (Audio Processing Unit) for the VibeNES emulator. The implementation prioritizes timing accuracy over audio quality initially, as the APU's frame counter is critical for game timing.

## Problem Statement

Super Mario Bros and many other NES games are stuck in infinite loops during startup because they depend on APU frame counter IRQs for timing. The current APU stub lacks this critical functionality.

## Implementation Phases

### Phase 1: Core APU Header Structure

Create `include/apu/apu.hpp`:

```cpp
#pragma once

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
    const char* get_name() const noexcept override { return "APU"; }

    // Register access
    void write(uint16_t address, uint8_t value);
    uint8_t read(uint16_t address);

    // IRQ handling
    bool is_frame_irq_pending() const { return frame_irq_flag_; }
    bool is_dmc_irq_pending() const { return dmc_irq_flag_; }
    void acknowledge_frame_irq() { frame_irq_flag_ = false; }
    void acknowledge_dmc_irq() { dmc_irq_flag_ = false; }

    // Audio output (future implementation)
    float get_audio_sample();

    // System connections
    void connect_cpu(CPU6502* cpu) { cpu_ = cpu; }

private:
    // Frame Counter - CRITICAL for timing
    struct FrameCounter {
        uint16_t divider;           // Divides CPU clock
        uint8_t step;               // Current step (0-3 or 0-4)
        bool mode;                  // 0 = 4-step, 1 = 5-step
        bool irq_inhibit;           // IRQ disable flag
        uint8_t reset_delay;        // Delay after $4017 write

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
        uint8_t envelope_volume;
        uint8_t envelope_divider;
        uint8_t envelope_decay_level;
        bool envelope_start;
        bool constant_volume;

        bool mode;              // 0 = normal, 1 = short
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

    // Cycle counter
    uint64_t cycle_count_;

    // External connections
    CPU6502* cpu_;

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
```

### Phase 2: Core APU Implementation

Create `src/apu/apu.cpp`:

```cpp
#include "apu/apu.hpp"
#include "cpu/cpu_6502.hpp"
#include <algorithm>
#include <cstring>

namespace nes {

// Length counter lookup table
const uint8_t APU::LENGTH_TABLE[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// Duty cycle sequences
const uint8_t APU::DUTY_TABLE[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}  // 25% negated
};

// Triangle wave sequence
const uint8_t APU::TRIANGLE_SEQUENCE[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// Noise period table (NTSC)
const uint16_t APU::NOISE_PERIOD_TABLE[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// DMC rate table (NTSC)
const uint16_t APU::DMC_RATE_TABLE[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

APU::APU()
    : frame_counter_{}, pulse1_{}, pulse2_{}, triangle_{}, noise_{}, dmc_{},
      frame_irq_flag_(false), dmc_irq_flag_(false), cycle_count_(0), cpu_(nullptr) {
}

void APU::power_on() {
    reset();
}

void APU::reset() {
    // Reset frame counter
    frame_counter_ = {};
    frame_counter_.irq_inhibit = true;  // IRQs disabled on reset

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
    for (int i = 0; i < cycles; i++) {
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

        if (frame_counter_.mode == 0) {  // 4-step mode
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
        } else {  // 5-step mode
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
    pulse1_.clock_sweep(true);   // Pulse 1
    pulse2_.clock_length();
    pulse2_.clock_sweep(false);  // Pulse 2
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

        // Pulse 2 (similar to Pulse 1)
        case 0x4004:
            pulse2_.duty = (value >> 6) & 3;
            pulse2_.length_enabled = !(value & 0x20);
            pulse2_.constant_volume = !(value & 0x10);
            pulse2_.envelope_volume = value & 0x0F;
            break;
        // ... (similar pattern for 0x4005-0x4007)

        // Status register
        case 0x4015:
            pulse1_.enabled = (value & 0x01) != 0;
            pulse2_.enabled = (value & 0x02) != 0;
            triangle_.enabled = (value & 0x04) != 0;
            noise_.enabled = (value & 0x08) != 0;
            dmc_.enabled = (value & 0x10) != 0;

            // Clear length counters if disabled
            if (!pulse1_.enabled) pulse1_.length_counter = 0;
            if (!pulse2_.enabled) pulse2_.length_counter = 0;
            if (!triangle_.enabled) triangle_.length_counter = 0;
            if (!noise_.enabled) noise_.length_counter = 0;
            if (!dmc_.enabled) {
                dmc_.bytes_remaining = 0;
            } else if (dmc_.bytes_remaining == 0) {
                dmc_.start_sample();
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
    // Placeholder - implement later
    return 0.0f;
}

} // namespace nes
```

### Phase 3: Channel Implementation Stubs

Add basic implementations for each channel's methods:

```cpp
// Add to apu.cpp

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

    uint8_t duty_output = DUTY_TABLE[duty][duty_sequence_pos];
    if (duty_output == 0) {
        return 0;
    }

    return constant_volume ? envelope_volume : envelope_decay_level;
}

// Similar implementations for Triangle, Noise, and DMC channels...
```

### Phase 4: Integration with Existing System

Update `include/apu/apu_stub.hpp` to include new APU or replace entirely:

```cpp
#pragma once

// Include the new APU implementation
#include "apu/apu.hpp"

// For backward compatibility, typedef the new APU as APUStub
namespace nes {
    using APUStub = APU;
}
```

Update `src/core/bus.cpp` to use the new APU:

```cpp
// In SystemBus constructor, replace APUStub with new APU
apu_ = std::make_unique<APU>();

// In initialization, connect CPU to APU
void SystemBus::connect_cpu(std::shared_ptr<CPU6502> cpu) {
    cpu_ = cpu;
    apu_->connect_cpu(cpu.get());  // Add this line
}
```

Update CPU to check for APU IRQs:

```cpp
// In cpu_6502.cpp, in execute_instruction():
void CPU6502::execute_instruction() {
    // ... existing code ...

    // After instruction execution, check for APU IRQs
    if (bus_->get_apu()->is_frame_irq_pending() && !get_interrupt_flag()) {
        trigger_irq();
        bus_->get_apu()->acknowledge_frame_irq();
    }

    if (bus_->get_apu()->is_dmc_irq_pending() && !get_interrupt_flag()) {
        trigger_irq();
        bus_->get_apu()->acknowledge_dmc_irq();
    }
}
```

### Phase 5: Testing and Validation

1. **Add Debug Output**:
```cpp
// In APU::clock_frame_counter(), add debug output
if (frame_counter_.step == 3 && frame_counter_.mode == 0 && !frame_counter_.irq_inhibit) {
    printf("APU: Frame IRQ generated at cycle %llu\n", cycle_count_);
}
```

2. **Test with Super Mario Bros**:
   - Load the ROM
   - Use step buttons to advance through initialization
   - Look for "APU: Frame IRQ generated" messages
   - Verify that sprite 0 eventually moves from (0,0)

3. **Verify Register Access**:
   - Add debug output for $4015 and $4017 writes
   - Ensure games are writing expected values

### Phase 6: Optimization and Refinement

1. **Cycle Accuracy**: Fine-tune the frame counter timing
2. **IRQ Timing**: Ensure IRQs fire at exactly the right CPU cycle
3. **Register Behavior**: Implement all edge cases for register reads/writes
4. **Audio Preparation**: Prepare channel implementations for future audio output

## Expected Results

After implementation:
- Super Mario Bros should progress past the infinite sprite 0 hit loop
- Frame counter IRQs should fire at ~240Hz
- Games should advance through initialization properly
- Debug output should show APU activity

## Critical Success Factors

1. **Frame Counter Timing**: Must be cycle-accurate
2. **IRQ Generation**: Must connect properly to CPU
3. **Register Implementation**: $4015 and $4017 are most critical
4. **Integration**: APU must be ticked every CPU cycle

This implementation prioritizes timing accuracy over audio quality, focusing on the components needed to fix the Super Mario Bros initialization issue.
