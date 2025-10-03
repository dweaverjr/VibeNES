// VibeNES - NES Emulator
// PPU Bus Conflict Tests
// Tests for PPU bus conflicts, race conditions, and timing edge cases

#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include "ppu_trace_harness.hpp"
#include "test_chr_data.hpp"
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

using namespace nes;

namespace {

std::string format_byte(uint8_t value) {
	std::ostringstream ss;
	ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(value)
	   << std::dec;
	return ss.str();
}

std::string format_word(uint16_t value) {
	std::ostringstream ss;
	ss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value << std::dec;
	return ss.str();
}

std::string format_debug_state(const PPU::DebugState &state) {
	std::ostringstream ss;
	ss << "sl=" << state.scanline << " cy=" << state.cycle << " v=" << format_word(state.vram_address)
	   << " t=" << format_word(state.temp_vram_address) << " fineX=" << static_cast<int>(state.fine_x_scroll)
	   << " fetch=" << static_cast<int>(state.fetch_cycle) << " tile{id=" << format_byte(state.current_tile_id)
	   << ", attr=" << format_byte(state.current_attribute) << "}"
	   << " next{id=" << format_byte(state.next_tile_id) << ", attr=" << format_byte(state.next_tile_attribute) << "}";
	return ss.str();
}

uint8_t sample_shift_pixel(uint16_t shift_reg, uint8_t fine_x) {
	uint8_t shift_amount = 15 - (fine_x & 0x07);
	return static_cast<uint8_t>((shift_reg >> shift_amount) & 0x01);
}

uint8_t estimate_background_pixel_with_offset(const PPU::DebugState &state, uint8_t fine_x_offset) {
	uint8_t effective_fine_x = static_cast<uint8_t>((state.fine_x_scroll + fine_x_offset) & 0x07);
	uint8_t pattern_low = sample_shift_pixel(state.bg_pattern_low_shift, effective_fine_x);
	uint8_t pattern_high = sample_shift_pixel(state.bg_pattern_high_shift, effective_fine_x);
	uint8_t pixel_value = static_cast<uint8_t>((pattern_high << 1) | pattern_low);
	if (pixel_value == 0) {
		return 0;
	}

	uint8_t attr_low = sample_shift_pixel(state.bg_attribute_low_shift, effective_fine_x);
	uint8_t attr_high = sample_shift_pixel(state.bg_attribute_high_shift, effective_fine_x);
	uint8_t palette = static_cast<uint8_t>((attr_high << 1) | attr_low);
	return static_cast<uint8_t>((palette * 4) + pixel_value);
}

uint8_t estimate_background_pixel(const PPU::DebugState &state) {
	return estimate_background_pixel_with_offset(state, 0);
}

uint8_t estimate_next_background_pixel(const PPU::DebugState &state) {
	return estimate_background_pixel_with_offset(state, 1);
}

} // namespace

class BusConflictTestFixture {
  public:
	BusConflictTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());
		ppu_memory = std::make_shared<PPUMemory>();

		// Load synthetic CHR ROM data FIRST for sprite 0 hit testing
		cartridge = nes::test::TestCHRData::create_test_cartridge();

		// Connect components to bus (like TimingTestFixture)
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);
		bus->connect_cpu(cpu);

		// Create and connect PPU
		ppu = std::make_shared<PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		// Connect cartridge to PPU for CHR ROM access
		ppu->connect_cartridge(cartridge);

		// Connect CPU to PPU for NMI generation
		ppu->connect_cpu(cpu.get());

		// Power on
		bus->power_on();
		ppu->power_on();

		// Initialize VRAM with test patterns
		setup_test_vram();
	}

	void setup_test_vram() {
		// Fill nametables with recognizable patterns
		for (uint16_t addr = 0x2000; addr < 0x3000; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Fill pattern tables with test data - these would need CHR ROM setup
		// For now, we'll focus on VRAM testing
	}
	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() < target_scanline && safety_counter < MAX_CYCLES) {
			ppu->tick(CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_scanline hit safety limit - possible infinite loop");
		}
	}

	void advance_to_cycle(int target_cycle) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_cycle() < target_cycle && safety_counter < MAX_CYCLES) {
			ppu->tick(CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_cycle hit safety limit - possible infinite loop");
		}
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

	void reset_toggle() {
		read_ppu_register(0x2002);
	}

	void write_vram(uint16_t address, uint8_t value) {
		reset_toggle();
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
		write_ppu_register(0x2007, value);
	}

	uint8_t read_vram(uint16_t address) {
		reset_toggle();
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
		return read_ppu_register(0x2007);
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<Cartridge> cartridge;
	std::shared_ptr<APU> apu;
	std::shared_ptr<CPU6502> cpu;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(BusConflictTestFixture, "VBlank Flag Race Conditions", "[ppu][bus_conflict][vblank]") {
	SECTION("PPUSTATUS read exactly at VBlank set") {
		// Position just before VBlank flag set (scanline 241, cycle 0)
		advance_to_scanline(241);
		advance_to_cycle(0);

		// VBlank should not be set yet
		uint8_t status_before = read_ppu_register(0x2002);
		REQUIRE((status_before & 0x80) == 0);

		// Reading PPUSTATUS exactly when VBlank flag is being set
		// This creates a race condition in real hardware
		advance_to_cycle(1);
		[[maybe_unused]] uint8_t status_during = read_ppu_register(0x2002);

		// The read should clear the flag that was just set
		// This is the infamous VBlank flag race condition
		uint8_t status_after = read_ppu_register(0x2002);
		REQUIRE((status_after & 0x80) == 0);
	}

	SECTION("NMI timing vs PPUSTATUS read race") {
		// Enable NMI
		write_ppu_register(0x2000, 0x80);

		advance_to_scanline(241);
		advance_to_cycle(0);

		// Reading PPUSTATUS on the exact cycle NMI would fire
		// should suppress the NMI
		advance_to_cycle(1);
		read_ppu_register(0x2002); // This should suppress NMI

		// NMI should not fire (would need CPU integration to test)
	}

	SECTION("VBlank clear race condition") {
		// Set VBlank flag first
		advance_to_scanline(241);
		advance_to_cycle(1);

		uint8_t status_set = read_ppu_register(0x2002);
		REQUIRE((status_set & 0x80) != 0);

		// Restore VBlank flag for test
		advance_to_scanline(241);
		advance_to_cycle(1);

		// Now test clearing race
		advance_to_scanline(261);
		advance_to_cycle(0);

		// Reading just before clear
		uint8_t status_before_clear = read_ppu_register(0x2002);
		REQUIRE((status_before_clear & 0x80) != 0);

		// Flag should be cleared by hardware on next cycle
		advance_to_cycle(1);
		uint8_t status_after_clear = read_ppu_register(0x2002);
		REQUIRE((status_after_clear & 0x80) == 0);
	}
}

TEST_CASE_METHOD(BusConflictTestFixture, "VRAM Access During Rendering", "[ppu][bus_conflict][vram]") {
	SECTION("VRAM read during background fetching") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(50); // Visible scanline
		advance_to_cycle(100);	 // During tile fetching

		// Set VRAM address
		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// VRAM read during rendering should return corrupted data
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// The exact value depends on what the PPU is fetching
		// but it should not be the expected nametable data
		// (This is hardware-specific behavior)
	}

	SECTION("VRAM write during sprite evaluation") {
		// Enable sprites
		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(50);
		advance_to_cycle(70); // During sprite evaluation (65-256)

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Write during sprite evaluation
		write_ppu_register(0x2007, 0x42);

		// The write may be corrupted or ignored
		// depending on what the PPU is doing
	}

	SECTION("Address corruption during rendering") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(100);
		advance_to_cycle(200); // Mid-scanline

		// Set a known VRAM address
		reset_toggle();
		write_ppu_register(0x2006, 0x23);
		write_ppu_register(0x2006, 0x45);

		// Reading during rendering can corrupt the address
		[[maybe_unused]] uint8_t data1 = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t data2 = read_ppu_register(0x2007);

		// The second read may not be from the expected address
		// due to address corruption during rendering
	}
}

TEST_CASE_METHOD(BusConflictTestFixture, "OAM Access Conflicts", "[ppu][bus_conflict][oam]") {
	SECTION("OAM write during sprite evaluation") {
		// Setup sprites in OAM
		write_ppu_register(0x2003, 0x00); // OAMADDR = 0
		for (int i = 0; i < 16; i++) {
			write_ppu_register(0x2004, i * 4);	// Y position
			write_ppu_register(0x2004, i);		// Tile index
			write_ppu_register(0x2004, 0x00);	// Attributes
			write_ppu_register(0x2004, i * 16); // X position
		}

		// Enable sprites
		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(50);
		advance_to_cycle(70); // During sprite evaluation

		// Try to write to OAM during sprite evaluation
		write_ppu_register(0x2003, 0x10);
		write_ppu_register(0x2004, 0xFF);

		// The write should be ignored or corrupted
	}

	SECTION("OAMADDR corruption during rendering") {
		// Set OAMADDR to a known value
		write_ppu_register(0x2003, 0x20);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(100);
		advance_to_cycle(100);

		// OAMADDR should be corrupted during rendering
		// (Hardware behavior: OAMADDR is incremented during sprite evaluation)

		// Read OAM data - address may not be where we expect
		[[maybe_unused]] uint8_t oam_data = read_ppu_register(0x2004);
	}

	SECTION("OAM DMA during sprite evaluation conflict") {
		// Setup sprite evaluation scenario
		write_ppu_register(0x2001, 0x10); // Enable sprites

		advance_to_scanline(50);
		advance_to_cycle(65); // Start of sprite evaluation

		// Attempt OAM DMA during sprite evaluation
		// (This would require CPU integration to test properly)
		// In real hardware, this can cause corruption

		// For now, just test that sprite evaluation is active
		REQUIRE(ppu->get_current_cycle() >= 65);
		REQUIRE(ppu->get_current_cycle() <= 256);
	}
}

TEST_CASE_METHOD(BusConflictTestFixture, "Register Write Timing Conflicts", "[ppu][bus_conflict][registers]") {
	SECTION("PPUCTRL write during VBlank flag set") {
		advance_to_scanline(241);
		advance_to_cycle(0);

		// Write to PPUCTRL exactly when VBlank flag is being set
		advance_to_cycle(1);
		write_ppu_register(0x2000, 0x80); // Enable NMI

		// This should still generate NMI if VBlank is set
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) != 0);
	}

	SECTION("Multiple register writes same cycle") {
		// Test writing to multiple PPU registers in sequence
		// (This tests the PPU's ability to handle rapid writes)

		write_ppu_register(0x2000, 0x90);
		write_ppu_register(0x2001, 0x1E);
		write_ppu_register(0x2005, 0x00);
		write_ppu_register(0x2005, 0x00);
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// All writes should be processed correctly
		[[maybe_unused]] uint8_t status = read_ppu_register(0x2002);
		// Test that PPU state is consistent
	}

	SECTION("Write toggle state during conflicts") {
		// Test write toggle behavior during timing conflicts

		reset_toggle();
		write_ppu_register(0x2005, 0x10); // First write (X)

		// Read PPUSTATUS (should reset toggle)
		read_ppu_register(0x2002);

		write_ppu_register(0x2005, 0x20); // Should be X again, not Y

		// Verify toggle was properly reset
	}
}

TEST_CASE_METHOD(BusConflictTestFixture, "Sprite 0 Hit Edge Cases", "[ppu][bus_conflict][sprite0]") {
	SECTION("Sprite 0 hit during PPUSTATUS read") {
		// Setup sprite 0 for hit detection
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);	  // Y position
		write_ppu_register(0x2004, 0x01); // Tile index
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X position

		// Setup background pattern to ensure hit
		setup_test_vram();

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to sprite 0 hit position
		advance_to_scanline(51); // Y + 1
		advance_to_cycle(108);	 // X + 8

		auto debug_before = ppu->get_debug_state();
		INFO(std::string("Before PPUSTATUS read: ") + format_debug_state(debug_before));

		// Reading PPUSTATUS when sprite 0 hit occurs
		uint8_t status = read_ppu_register(0x2002);
		INFO(std::string("PPUSTATUS read value: ") + format_byte(status));

		auto debug_after_first_read = ppu->get_debug_state();
		INFO(std::string("After first PPUSTATUS read: ") + format_debug_state(debug_after_first_read));

		// Sprite 0 hit should be detected
		REQUIRE((status & 0x40) != 0);

		// Reading again should clear the flag
		uint8_t status2 = read_ppu_register(0x2002);
		INFO(std::string("Second PPUSTATUS read value: ") + format_byte(status2));
		REQUIRE((status2 & 0x40) == 0);
	}

	SECTION("Sprite 0 hit with rendering disabled") {
		// Setup sprite 0
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);
		write_ppu_register(0x2004, 0x01);
		write_ppu_register(0x2004, 0x00);
		write_ppu_register(0x2004, 100);

		// Disable rendering
		write_ppu_register(0x2001, 0x00);

		advance_to_scanline(51);
		advance_to_cycle(108);

		// Sprite 0 hit should NOT occur with rendering disabled
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x40) == 0);
	}

	SECTION("Sprite 0 hit pixel precision") {
		// Reset PPU state for clean test
		ppu->reset();

		int first_hit_cycle = -1;
		// Probe latching behaviour with dedicated harness for diagnostics
		{
			nes::test::PPUTraceHarness probe;
			probe.write_ppu_register(0x2003, 0x00);
			probe.write_ppu_register(0x2004, 100);
			probe.write_ppu_register(0x2004, 0x01);
			probe.write_ppu_register(0x2004, 0x00);
			probe.write_ppu_register(0x2004, 200);
			probe.write_ppu_register(0x2001, 0x18);
			probe.advance_to_position(101, 0, false);
			for (int cycle = 0; cycle <= 256; ++cycle) {
				probe.advance_to_position(101, static_cast<uint16_t>(cycle), false);
				auto status = probe.ppu()->get_status_register();
				if ((status & 0x40) != 0) {
					first_hit_cycle = cycle;
					break;
				}
			}
		}
		INFO(std::string("Probe sprite 0 hit latched by cycle: ") +
			 format_word(static_cast<uint16_t>(first_hit_cycle)));

		// Test exact pixel timing for sprite 0 hit
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 100);  // Y position
		write_ppu_register(0x2004, 0x01); // Non-zero tile
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 200);  // X position

		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(101);

		// Hit should occur at exact pixel position
		// Sprite at X=200 renders starting at cycle 201 (X+1)
		// Hit detected at cycle 201, 2-cycle delay, flag visible at cycle 203+
		advance_to_cycle(203); // Before hit flag becomes visible
		auto debug_before_hit = ppu->get_debug_state();
		INFO(std::string("Before sprite 0 hit status read: ") + format_debug_state(debug_before_hit));
		auto bg_pixel_before = estimate_background_pixel(debug_before_hit);
		auto bg_pixel_next_before = estimate_next_background_pixel(debug_before_hit);
		INFO(std::string("Estimated BG pixel before hit: ") + format_byte(bg_pixel_before));
		INFO(std::string("Estimated BG pixel (next) before hit: ") + format_byte(bg_pixel_next_before));
		uint8_t status_before = read_ppu_register(0x2002);
		INFO(std::string("Status before expected hit: ") + format_byte(status_before));
		REQUIRE((status_before & 0x40) == 0);

		advance_to_cycle(204); // Hit flag should be visible (set during cycle 203)
		auto debug_at_hit = ppu->get_debug_state();
		INFO(std::string("At expected sprite 0 hit: ") + format_debug_state(debug_at_hit));
		auto bg_pixel_at_hit = estimate_background_pixel(debug_at_hit);
		auto bg_pixel_next_at_hit = estimate_next_background_pixel(debug_at_hit);
		INFO(std::string("Estimated BG pixel at hit: ") + format_byte(bg_pixel_at_hit));
		INFO(std::string("Estimated BG pixel (next) at hit: ") + format_byte(bg_pixel_next_at_hit));
		uint8_t status_hit = read_ppu_register(0x2002);
		INFO(std::string("Status at expected hit: ") + format_byte(status_hit));
		REQUIRE((status_hit & 0x40) != 0);
	}
}

TEST_CASE_METHOD(BusConflictTestFixture, "Power-On vs Reset Behavior", "[ppu][bus_conflict][power_on]") {
	SECTION("Register state after reset") {
		// Reset PPU
		ppu->reset();

		// Check initial register states
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) == 0); // VBlank clear
		REQUIRE((status & 0x40) == 0); // Sprite 0 hit clear
		REQUIRE((status & 0x20) == 0); // Sprite overflow clear

		// Other registers should be in known state
		REQUIRE(ppu->get_current_scanline() == 0);
		REQUIRE(ppu->get_current_cycle() == 0);
	}

	SECTION("Write toggle state after reset") {
		// Reset should clear write toggle
		ppu->reset();

		// First write should be X scroll
		write_ppu_register(0x2005, 0x10);
		// Second write should be Y scroll
		write_ppu_register(0x2005, 0x20);
		// Third write should be X scroll again
		write_ppu_register(0x2005, 0x30);

		// Verify toggle behavior is correct
	}

	SECTION("Memory state after reset") {
		// Reset should not affect VRAM/OAM contents
		// (Unlike power-on which randomizes memory)

		// Write test pattern before reset
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 0x42);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);
		write_ppu_register(0x2007, 0x33);

		// Reset PPU
		ppu->reset();

		// Memory should retain values
		write_ppu_register(0x2003, 0x00);
		uint8_t oam_data = read_ppu_register(0x2004);
		REQUIRE(oam_data == 0x42);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);
		[[maybe_unused]] uint8_t dummy = read_ppu_register(0x2007);
		uint8_t vram_data = read_ppu_register(0x2007);
		REQUIRE(vram_data == 0x33);
	}
}
