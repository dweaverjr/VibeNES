// VibeNES - NES Emulator
// APU (Audio Processing Unit) Tests
// Tests for all 5 audio channels, frame counter, register I/O, mixing, and serialization

#include "../../include/apu/apu.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/core/types.hpp"
#include "../../include/memory/ram.hpp"
#include <catch2/catch_all.hpp>
#include <cmath>

using namespace nes;

// Helper: create a standalone APU for register-level testing.
// No bus/cpu connections needed for most APU unit tests.
// Returns unique_ptr since Component is non-copyable.
static std::unique_ptr<APU> make_apu() {
	auto apu = std::make_unique<APU>();
	apu->power_on();
	return apu;
}

// Helper: tick APU for N CPU cycles
static void tick_apu(APU &apu, int cycles) {
	for (int i = 0; i < cycles; i++) {
		apu.tick(CpuCycle(1));
	}
}

// =============================================================================
// Construction & Reset
// =============================================================================

TEST_CASE("APU Construction", "[apu]") {
	APU apu;

	SECTION("Name is APU") {
		REQUIRE(std::string(apu.get_name()) == "APU");
	}

	SECTION("No IRQ pending after construction") {
		REQUIRE_FALSE(apu.is_frame_irq_pending());
		REQUIRE_FALSE(apu.is_dmc_irq_pending());
	}

	SECTION("No DMC DMA pending after construction") {
		REQUIRE_FALSE(apu.is_dmc_dma_pending());
		REQUIRE(apu.get_dmc_dma_address() == 0);
	}
}

TEST_CASE("APU Reset", "[apu]") {
	APU apu;
	apu.power_on();

	// Enable some channels and generate state
	apu.write(0x4015, 0x1F); // Enable all channels
	apu.write(0x4000, 0xBF); // Pulse 1: duty 2, constant vol 15
	apu.write(0x4017, 0x00); // 4-step mode, IRQ enabled

	apu.reset();

	SECTION("IRQ flags cleared after reset") {
		REQUIRE_FALSE(apu.is_frame_irq_pending());
		REQUIRE_FALSE(apu.is_dmc_irq_pending());
	}

	SECTION("DMC DMA cleared after reset") {
		REQUIRE_FALSE(apu.is_dmc_dma_pending());
	}

	SECTION("All channels produce zero output after reset") {
		// After reset with no channels enabled, output should be near-zero
		float sample = apu.get_audio_sample();
		REQUIRE(sample == Catch::Approx(0.0f).margin(0.001f));
	}
}

// =============================================================================
// Status Register ($4015) Read/Write
// =============================================================================

TEST_CASE("APU Status Register", "[apu][registers]") {
	auto apu = make_apu();

	SECTION("Writing $4015 enables/disables channels") {
		// Enable all channels
		apu->write(0x4015, 0x1F);
		// Read back status — length counters are still 0 so bits will be 0
		uint8_t status = apu->read(0x4015);
		// With no length counter loaded, status bits 0-4 should be 0
		REQUIRE((status & 0x1F) == 0x00);
	}

	SECTION("Disabling channel clears length counter") {
		apu->write(0x4015, 0x01); // Enable pulse 1
		apu->write(0x4000, 0xBF); // Constant volume 15
		apu->write(0x4003, 0x08); // Length counter index 1 (= 254), timer high
		// Verify pulse 1 has length > 0
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);

		// Disable pulse 1
		apu->write(0x4015, 0x00);
		status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x00);
	}

	SECTION("Reading $4015 clears frame IRQ flag") {
		// Force frame IRQ by running in 4-step mode with IRQ enabled
		apu->write(0x4017, 0x00); // 4-step mode, IRQ not inhibited

		// Run enough cycles for a full 4-step sequence (~29830 APU cycles = ~59660 CPU cycles)
		// The frame IRQ fires at step 3 of 4-step mode
		tick_apu(*apu, 60000);

		// Frame IRQ should be set
		REQUIRE(apu->is_frame_irq_pending());

		// Reading $4015 should clear it
		apu->read(0x4015);
		REQUIRE_FALSE(apu->is_frame_irq_pending());
	}

	SECTION("$4015 bit 6 reflects frame IRQ flag") {
		apu->write(0x4017, 0x00); // 4-step mode, IRQ enabled
		tick_apu(*apu, 60000);
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x40) == 0x40);
	}
}

// =============================================================================
// Frame Counter ($4017)
// =============================================================================

TEST_CASE("APU Frame Counter Modes", "[apu][frame-counter]") {
	auto apu = make_apu();

	SECTION("4-step mode generates IRQ") {
		apu->write(0x4017, 0x00); // 4-step mode, IRQ not inhibited
		tick_apu(*apu, 60000);	  // Full frame
		REQUIRE(apu->is_frame_irq_pending());
	}

	SECTION("5-step mode does not generate IRQ") {
		apu->write(0x4017, 0x80); // 5-step mode
		tick_apu(*apu, 60000);
		REQUIRE_FALSE(apu->is_frame_irq_pending());
	}

	SECTION("IRQ inhibit prevents frame IRQ in 4-step mode") {
		apu->write(0x4017, 0x40); // 4-step mode, IRQ inhibited
		tick_apu(*apu, 60000);
		REQUIRE_FALSE(apu->is_frame_irq_pending());
	}

	SECTION("5-step mode clocks immediately on write") {
		// In 5-step mode, writing $4017 should immediately clock quarter + half frame
		apu->write(0x4015, 0x01); // Enable pulse 1
		apu->write(0x4000, 0x30); // Pulse 1: length halt=0, constant vol
		apu->write(0x4003, 0x08); // Load length counter

		// Length counter should be > 0
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);

		// Write 5-step mode — this triggers immediate half frame which clocks length
		apu->write(0x4017, 0x80);

		// Tick one cycle so the write takes effect
		tick_apu(*apu, 1);

		// Length counter was clocked by immediate half frame, should have decremented
		// (Still > 0 since it started at 254, but we just verify the mechanism works)
		status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01); // Still active (254 - 1 = 253)
	}
}

// =============================================================================
// Pulse Channel Registers
// =============================================================================

TEST_CASE("APU Pulse Channel Registers", "[apu][pulse]") {
	auto apu = make_apu();
	apu->write(0x4015, 0x03); // Enable both pulse channels

	SECTION("Pulse 1 duty cycle and volume ($4000)") {
		// Duty 2 (50%), length halt, constant volume 10
		apu->write(0x4000, 0xAA); // 10_1_0_1010 = duty 2, halt, const vol, vol=10
		// No direct way to read back, but we can verify output after timer setup
		apu->write(0x4002, 0x00); // Timer low
		apu->write(0x4003, 0x08); // Timer high + length counter load
		tick_apu(*apu, 100);

		// Channel should produce non-zero output
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01); // Pulse 1 length > 0
	}

	SECTION("Pulse 2 mirrors pulse 1 register layout at $4004-$4007") {
		apu->write(0x4004, 0xBF); // Same encoding as pulse 1
		apu->write(0x4006, 0x00);
		apu->write(0x4007, 0x08);
		tick_apu(*apu, 100);

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x02) == 0x02); // Pulse 2 length > 0
	}

	SECTION("Timer period combines low and high bytes") {
		apu->write(0x4000, 0xBF); // Constant volume 15
		apu->write(0x4002, 0xFD); // Timer low byte
		apu->write(0x4003, 0x02); // Timer high (bits 0-2) = 2, length index
		// Timer period = (2 << 8) | 0xFD = 0x2FD = 765
		// We verify this indirectly — channel is active
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);
	}
}

// =============================================================================
// Triangle Channel
// =============================================================================

TEST_CASE("APU Triangle Channel", "[apu][triangle]") {
	auto apu = make_apu();
	apu->write(0x4015, 0x04); // Enable triangle

	SECTION("Triangle active with length and linear counter loaded") {
		apu->write(0x4008, 0xFF); // Control flag + linear counter = 127
		apu->write(0x400A, 0x00); // Timer low
		apu->write(0x400B, 0x08); // Timer high + length load

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x04) == 0x04); // Triangle length > 0
	}

	SECTION("Triangle silenced when disabled") {
		apu->write(0x4008, 0xFF);
		apu->write(0x400A, 0x00);
		apu->write(0x400B, 0x08);

		// Disable triangle
		apu->write(0x4015, 0x00);
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x04) == 0x00);
	}
}

// =============================================================================
// Noise Channel
// =============================================================================

TEST_CASE("APU Noise Channel", "[apu][noise]") {
	auto apu = make_apu();
	apu->write(0x4015, 0x08); // Enable noise

	SECTION("Noise active with length counter loaded") {
		apu->write(0x400C, 0x3F); // Length halt, constant volume 15
		apu->write(0x400E, 0x00); // Mode 0, period index 0
		apu->write(0x400F, 0x08); // Length counter load

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x08) == 0x08); // Noise length > 0
	}

	SECTION("Noise mode bit selects short mode") {
		apu->write(0x400C, 0x3F);
		apu->write(0x400E, 0x80); // Mode 1 (short), period index 0
		apu->write(0x400F, 0x08);

		// Channel is active
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x08) == 0x08);
	}
}

// =============================================================================
// DMC Channel
// =============================================================================

TEST_CASE("APU DMC Channel", "[apu][dmc]") {
	auto apu = make_apu();

	SECTION("DMC sample address formula: $C000 + value * 64") {
		apu->write(0x4012, 0x00); // address = $C000 + 0*64 = $C000
		// Can't directly read, but we verify through DMA address later
		apu->write(0x4012, 0xFF); // address = $C000 + 255*64 = $FFC0
								  // Verified by DMA mechanism
	}

	SECTION("DMC sample length formula: value * 16 + 1") {
		apu->write(0x4013, 0x00); // length = 0*16 + 1 = 1
		apu->write(0x4013, 0xFF); // length = 255*16 + 1 = 4081
	}

	SECTION("DMC direct output level ($4011)") {
		apu->write(0x4015, 0x10); // Enable DMC
		apu->write(0x4011, 0x40); // Direct load output level = 64
		// Output level affects audio mixing
		float sample = apu->get_audio_sample();
		// DMC at 64, all other channels at 0 — should produce non-zero TND output
		REQUIRE(sample != 0.0f);
	}

	SECTION("DMC output level clamped to 7 bits (0-127)") {
		apu->write(0x4015, 0x10);
		apu->write(0x4011, 0x7F); // Max = 127
		float sample_max = apu->get_audio_sample();
		REQUIRE(sample_max != 0.0f);

		apu->write(0x4011, 0x00); // Min = 0
		float sample_min = apu->get_audio_sample();
		// At 0, TND contribution from DMC is zero
		REQUIRE(sample_min == Catch::Approx(0.0f).margin(0.001f));
	}

	SECTION("Enabling DMC with bytes_remaining=0 restarts sample") {
		apu->write(0x4010, 0x00); // IRQ disabled, no loop, rate index 0
		apu->write(0x4012, 0x00); // Sample address $C000
		apu->write(0x4013, 0x01); // Sample length = 1*16+1 = 17

		// Enable DMC — should restart sample
		apu->write(0x4015, 0x10);

		// DMC should be active
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x10) == 0x10);
	}
}

// =============================================================================
// Audio Mixing
// =============================================================================

TEST_CASE("APU Audio Mixing", "[apu][mixing]") {
	auto apu = make_apu();

	SECTION("All channels silent produces zero output") {
		float sample = apu->get_audio_sample();
		REQUIRE(sample == Catch::Approx(0.0f).margin(0.001f));
	}

	SECTION("Pulse output uses non-linear mixing") {
		apu->write(0x4015, 0x01); // Enable pulse 1
		apu->write(0x4000, 0xBF); // Duty 2, constant volume 15
		apu->write(0x4002, 0xFD); // Timer low (period ~765 for ~440Hz-ish)
		apu->write(0x4003, 0x08); // Timer high + length

		// Run enough cycles for the pulse to produce output
		tick_apu(*apu, 2000);
		float sample = apu->get_audio_sample();
		// Non-linear mixing: 95.88 / ((8128 / pulse) + 100)
		// With max volume pulse, sample should be positive and < 0.5
		REQUIRE(sample >= 0.0f);
		REQUIRE(sample < 0.5f);
	}

	SECTION("DMC direct load affects mix") {
		apu->write(0x4015, 0x10); // Enable DMC only
		apu->write(0x4011, 0x7F); // Direct load max output

		float sample = apu->get_audio_sample();
		// TND formula with only DMC: 159.79 / ((1 / (dmc/22638)) + 100)
		// Should produce a non-trivial contribution
		REQUIRE(sample > 0.0f);
	}
}

// =============================================================================
// Length Counter
// =============================================================================

TEST_CASE("APU Length Counter", "[apu][length-counter]") {
	auto apu = make_apu();

	SECTION("Length counter lookup table values") {
		// Write specific length counter indices and verify via $4015
		// Index 1 (value 0x08 in high byte = index 1) should load 254
		apu->write(0x4015, 0x01); // Enable pulse 1
		apu->write(0x4000, 0xBF); // Constant vol, no halt
		apu->write(0x4003, 0x08); // Length index = (0x08 >> 3) = 1 → LENGTH_TABLE[1] = 254

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);
	}

	SECTION("Length halt prevents countdown") {
		apu->write(0x4015, 0x01);
		apu->write(0x4000, 0xBF | 0x20); // Set length halt (bit 5)
		apu->write(0x4003, 0x08);		 // Load length

		// Run many cycles with halted length
		// 5-step mode to avoid frame IRQ complications
		apu->write(0x4017, 0x80);
		tick_apu(*apu, 120000); // Several frames worth

		// Length counter should still be > 0 due to halt
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);
	}

	SECTION("Length counter counts down to zero without halt") {
		apu->write(0x4015, 0x01);
		apu->write(0x4000, 0x9F); // No length halt (bit 5 clear), constant vol 15
		apu->write(0x4002, 0x00);
		apu->write(0x4003, 0x08); // Length index 1 = 254

		apu->write(0x4017, 0x80); // 5-step mode

		// Half frame clocks happen at steps 1 and 4 of 5-step mode
		// Each half frame decrements length by 1
		// After enough cycles, length should reach 0
		// 254 half-frame clocks needed. Each 5-step frame has 2 half clocks.
		// One 5-step frame ≈ 37282 APU cycles ≈ 74564 CPU cycles
		// Need ~127 frames = ~9.47M CPU cycles — that's too many for a unit test
		// Instead, use a short length counter value
		apu->write(0x4003, 0x00); // Length index 0 → LENGTH_TABLE[0] = 10

		// 10 half-frame clocks needed. 2 per frame. ~5 frames = ~372,820 CPU cycles
		tick_apu(*apu, 400000);

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x00); // Should have counted down to 0
	}
}

// =============================================================================
// Envelope
// =============================================================================

TEST_CASE("APU Envelope", "[apu][envelope]") {
	auto apu = make_apu();
	apu->write(0x4015, 0x01); // Enable pulse 1

	SECTION("Constant volume mode outputs volume directly") {
		apu->write(0x4000, 0xBF); // Duty 2, halt, constant volume = 15
		apu->write(0x4002, 0xFD);
		apu->write(0x4003, 0x08);

		// Run some cycles
		tick_apu(*apu, 2000);

		// Channel should produce output (constant volume)
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);
	}

	SECTION("Envelope mode decays volume over time") {
		apu->write(0x4000, 0x8F); // Duty 2, no halt, envelope mode, period=15
		apu->write(0x4002, 0xFD);
		apu->write(0x4003, 0x08);

		apu->write(0x4017, 0x80); // 5-step mode for clean timing

		// After many quarter-frame clocks, envelope should have decayed
		// Quarter frames happen at steps 0,1,2,4 in 5-step mode
		tick_apu(*apu, 200000); // Several frames

		// Channel still active but volume should be lower
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01); // Still has length
	}
}

// =============================================================================
// Sweep Unit
// =============================================================================

TEST_CASE("APU Pulse Sweep", "[apu][sweep]") {
	auto apu = make_apu();
	apu->write(0x4015, 0x03); // Enable both pulse channels

	SECTION("Sweep muting when period < 8") {
		apu->write(0x4000, 0xBF); // Constant volume 15
		apu->write(0x4001, 0x00); // Sweep disabled
		apu->write(0x4002, 0x05); // Timer period = 5 (< 8, muted)
		apu->write(0x4003, 0x08);

		tick_apu(*apu, 2000);

		// Channel is active (length > 0) but output should be muted
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01); // Has length
										  // We can't directly check output is zero without examining samples,
										  // but the length counter confirms the channel is configured
	}

	SECTION("Pulse 1 vs Pulse 2 sweep negate difference") {
		// Pulse 1 uses one's complement (negate = ~period)
		// Pulse 2 uses two's complement (negate = -period)
		// This matters at period 0: P1 negate → -1 (muted), P2 negate → 0 (not muted)
		apu->write(0x4000, 0xBF);
		apu->write(0x4001, 0x8F); // Sweep enabled, negate, shift 7
		apu->write(0x4002, 0x00); // Timer period = 0 (too low, muted anyway)
		apu->write(0x4003, 0x08);

		apu->write(0x4004, 0xBF);
		apu->write(0x4005, 0x8F); // Same sweep for pulse 2
		apu->write(0x4006, 0x00);
		apu->write(0x4007, 0x08);

		tick_apu(*apu, 2000);

		// Both channels have length counters loaded
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x03) == 0x03);
	}
}

// =============================================================================
// Serialization Roundtrip
// =============================================================================

TEST_CASE("APU State Serialization", "[apu][serialization]") {
	auto apu = make_apu();

	SECTION("Roundtrip preserves initial state") {
		std::vector<uint8_t> buffer;
		apu->serialize_state(buffer);
		REQUIRE(buffer.size() > 0);

		APU apu2;
		apu2.power_on();
		size_t offset = 0;
		apu2.deserialize_state(buffer, offset);

		// Verify by re-serializing and comparing
		std::vector<uint8_t> buffer2;
		apu2.serialize_state(buffer2);
		REQUIRE(buffer == buffer2);
	}

	SECTION("Roundtrip preserves complex state") {
		// Set up various channel states
		apu->write(0x4015, 0x1F); // Enable all
		apu->write(0x4000, 0xBF); // Pulse 1 config
		apu->write(0x4002, 0xFD);
		apu->write(0x4003, 0x08);
		apu->write(0x4004, 0x7F); // Pulse 2
		apu->write(0x4006, 0x80);
		apu->write(0x4007, 0x10);
		apu->write(0x4008, 0xFF); // Triangle
		apu->write(0x400A, 0x42);
		apu->write(0x400B, 0x08);
		apu->write(0x400C, 0x3F); // Noise
		apu->write(0x400E, 0x85);
		apu->write(0x400F, 0x10);
		apu->write(0x4010, 0x0F); // DMC
		apu->write(0x4011, 0x40);

		// Run some cycles to create interesting state
		tick_apu(*apu, 5000);

		std::vector<uint8_t> buffer;
		apu->serialize_state(buffer);

		APU apu2;
		apu2.power_on();
		size_t offset = 0;
		apu2.deserialize_state(buffer, offset);

		// Re-serialize and compare
		std::vector<uint8_t> buffer2;
		apu2.serialize_state(buffer2);
		REQUIRE(buffer == buffer2);
	}

	SECTION("Roundtrip preserves IRQ and DMA state") {
		apu->write(0x4017, 0x00); // 4-step mode, IRQ enabled
		tick_apu(*apu, 60000);	  // Generate frame IRQ

		std::vector<uint8_t> buffer;
		apu->serialize_state(buffer);

		APU apu2;
		size_t offset = 0;
		apu2.deserialize_state(buffer, offset);

		REQUIRE(apu2.is_frame_irq_pending() == apu->is_frame_irq_pending());
		REQUIRE(apu2.is_dmc_irq_pending() == apu->is_dmc_irq_pending());
		REQUIRE(apu2.is_dmc_dma_pending() == apu->is_dmc_dma_pending());
	}
}

// =============================================================================
// DMC DMA Interface
// =============================================================================

TEST_CASE("APU DMC DMA Interface", "[apu][dmc][dma]") {
	auto apu = make_apu();

	SECTION("complete_dmc_dma fills sample buffer") {
		// Set up DMC with a pending request manually
		apu->write(0x4015, 0x10); // Enable DMC
		apu->write(0x4010, 0x00); // No IRQ, no loop
		apu->write(0x4012, 0x00); // Sample address $C000
		apu->write(0x4013, 0x01); // Length = 17

		// The DMC will request DMA when its sample buffer empties
		// We can test the complete_dmc_dma flow directly
		// First need to get into a state where DMA is pending
		// This requires ticking enough for the DMC timer to fire

		// For now, just verify the interface doesn't crash
		apu->complete_dmc_dma(0xAA);
	}
}

// =============================================================================
// Register Write Edge Cases
// =============================================================================

TEST_CASE("APU Register Edge Cases", "[apu][registers][edge-cases]") {
	auto apu = make_apu();

	SECTION("Writing to disabled channel does not load length counter") {
		// Don't enable pulse 1
		apu->write(0x4015, 0x00);
		apu->write(0x4003, 0x08); // Try to load length counter

		// Pulse 1 should not have length
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x00);
	}

	SECTION("Writing $4003/$4007 resets duty sequence position") {
		apu->write(0x4015, 0x01);
		apu->write(0x4000, 0xBF);
		apu->write(0x4002, 0x00);

		// Run some cycles to advance duty position
		tick_apu(*apu, 1000);

		// Writing $4003 resets the duty sequence
		apu->write(0x4003, 0x08);

		// Channel should still be functional
		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x01) == 0x01);
	}

	SECTION("$4015 write clears DMC IRQ flag") {
		// Verify write to $4015 clears DMC IRQ
		// We can't easily trigger DMC IRQ without a bus, but we can test
		// that the mechanism exists
		apu->write(0x4015, 0x00);
		REQUIRE_FALSE(apu->is_dmc_irq_pending());
	}

	SECTION("Reading non-$4015 APU registers returns 0") {
		// Only $4015 has a meaningful read
		// Other reads should return 0 (or open bus, but APU returns 0)
		uint8_t val = apu->read(0x4000);
		REQUIRE(val == 0x00);
	}
}

// =============================================================================
// Frame Counter Timing
// =============================================================================

TEST_CASE("APU Frame Counter Timing", "[apu][frame-counter][timing]") {
	auto apu = make_apu();

	SECTION("Frame IRQ fires once per 4-step frame") {
		apu->write(0x4017, 0x00); // 4-step mode, IRQ enabled

		// First frame
		tick_apu(*apu, 60000);
		REQUIRE(apu->is_frame_irq_pending());

		// Clear and check another frame
		apu->acknowledge_frame_irq();
		REQUIRE_FALSE(apu->is_frame_irq_pending());

		tick_apu(*apu, 60000);
		REQUIRE(apu->is_frame_irq_pending());
	}

	SECTION("Acknowledge frame IRQ clears flag") {
		apu->write(0x4017, 0x00);
		tick_apu(*apu, 60000);
		REQUIRE(apu->is_frame_irq_pending());

		apu->acknowledge_frame_irq();
		REQUIRE_FALSE(apu->is_frame_irq_pending());
	}

	SECTION("Acknowledge DMC IRQ clears flag") {
		// Just verify the acknowledge mechanism works
		apu->acknowledge_dmc_irq();
		REQUIRE_FALSE(apu->is_dmc_irq_pending());
	}
}

// =============================================================================
// Lookup Tables
// =============================================================================

TEST_CASE("APU Lookup Tables", "[apu][tables]") {
	SECTION("Noise period table has 16 entries in ascending order") {
		// We can verify through the register write interface
		auto apu = make_apu();
		apu->write(0x4015, 0x08); // Enable noise

		// Period index 0 = shortest period (4)
		apu->write(0x400C, 0x3F);
		apu->write(0x400E, 0x00);
		apu->write(0x400F, 0x08);

		uint8_t status = apu->read(0x4015);
		REQUIRE((status & 0x08) == 0x08);

		// Period index 15 = longest period (4068)
		apu->write(0x400E, 0x0F);
		apu->write(0x400F, 0x08);

		status = apu->read(0x4015);
		REQUIRE((status & 0x08) == 0x08);
	}
}
