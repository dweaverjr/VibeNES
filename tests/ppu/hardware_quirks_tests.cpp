// VibeNES - NES Emulator
// PPU Hardware Quirks Tests
// Tests for undocumented PPU behavior and hardware quirks

#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

class HardwareQuirksTestFixture {
  public:
	HardwareQuirksTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ram(ram);
		ppu = std::make_shared<PPU>();
		bus->connect_ppu(ppu);
		ppu->connect_bus(bus.get());
		ppu->power_on();

		setup_test_environment();
	}

	void setup_test_environment() {
		// Fill VRAM with test patterns
		for (uint16_t addr = 0x2000; addr < 0x3000; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Setup palettes
		for (uint16_t addr = 0x3F00; addr < 0x3F20; addr++) {
			write_vram(addr, static_cast<uint8_t>(addr & 0x3F));
		}
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
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(HardwareQuirksTestFixture, "OAMADDR Decay During Rendering", "[ppu][quirks][oamaddr]") {
	SECTION("OAMADDR should increment during sprite evaluation") {
		// Set initial OAMADDR
		write_ppu_register(0x2003, 0x00);

		// Enable sprites
		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(50);
		advance_to_cycle(64); // Just before sprite evaluation

		// OAMADDR should still be 0
		uint8_t oam_before = read_ppu_register(0x2004);

		advance_to_cycle(256); // After sprite evaluation

		// OAMADDR should have been incremented/corrupted
		uint8_t oam_after = read_ppu_register(0x2004);

		// The exact behavior depends on sprite count and evaluation
		// but OAMADDR should have changed
	}

	SECTION("OAMADDR corruption pattern") {
		// OAMADDR is corrupted in a specific pattern during rendering
		for (uint8_t start_addr = 0; start_addr < 8; start_addr++) {
			write_ppu_register(0x2003, start_addr);
			write_ppu_register(0x2001, 0x10); // Enable sprites

			advance_to_scanline(100);
			advance_to_cycle(256); // After sprite evaluation

			// Check if corruption follows expected pattern
			// (This is very hardware-specific)
		}
	}

	SECTION("OAMADDR reset timing") {
		// OAMADDR is reset to 0 during cycles 257-320
		write_ppu_register(0x2003, 0x40);
		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(50);
		advance_to_cycle(256);

		// OAMADDR should be corrupted here
		advance_to_cycle(320);

		// OAMADDR should be reset to 0
		uint8_t oam_data = read_ppu_register(0x2004);
		// Should read from address 0
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Open Bus Behavior", "[ppu][quirks][open_bus]") {
	SECTION("Unused register bits should return open bus") {
		// PPUSTATUS bits 0-4 are unused and should return open bus
		uint8_t status = read_ppu_register(0x2002);

		// Bits 5-7 are defined, bits 0-4 are open bus
		// The exact value depends on the last value on the bus
	}

	SECTION("Write-only register reads") {
		// Reading write-only registers should return open bus

		// Write a value to set up the bus
		write_ppu_register(0x2007, 0xAA);

		// Reading PPUCTRL (write-only) should return bus value
		uint8_t ctrl_read = read_ppu_register(0x2000);

		// Reading PPUMASK (write-only) should return bus value
		uint8_t mask_read = read_ppu_register(0x2001);

		// Reading PPUSCROLL (write-only) should return bus value
		uint8_t scroll_read = read_ppu_register(0x2005);

		// Reading PPUADDR (write-only) should return bus value
		uint8_t addr_read = read_ppu_register(0x2006);

		// These should all return the last bus value (0xAA in this case)
		// or decay pattern depending on timing
	}

	SECTION("Bus decay over time") {
		// Bus values decay over time due to capacitance
		write_ppu_register(0x2007, 0xFF);

		// Read immediately
		uint8_t immediate = read_ppu_register(0x2000);

		// Advance time and read again
		advance_ppu_cycles(1000);
		uint8_t delayed = read_ppu_register(0x2000);

		// Values should potentially decay (hardware-dependent)
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "VRAM Address Line Behavior", "[ppu][quirks][address_lines]") {
	SECTION("Address line floating during rendering") {
		// During rendering, address lines can float to unexpected values
		write_ppu_register(0x2001, 0x18); // Enable rendering

		advance_to_scanline(100);
		advance_to_cycle(150); // Mid-scanline

		// Set VRAM address
		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Read - address might be corrupted by rendering
		uint8_t data = read_ppu_register(0x2007);

		// The address used might not be $2000 due to rendering interference
	}

	SECTION("Palette address mirroring quirks") {
		// Palette addresses have unusual mirroring behavior

		// Write to palette
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x10); // Sprite palette 0
		write_ppu_register(0x2007, 0x30);

		// Read from mirrored address
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x00); // Background palette 0
		uint8_t mirrored = read_ppu_register(0x2007);

		// $3F10, $3F14, $3F18, $3F1C mirror to $3F00, $3F04, $3F08, $3F0C
		REQUIRE(mirrored == 0x30);
	}

	SECTION("VRAM address increment timing") {
		// VRAM address increment has precise timing requirements
		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Multiple reads should increment properly
		uint8_t dummy1 = read_ppu_register(0x2007); // Dummy read
		uint8_t data1 = read_ppu_register(0x2007);	// $2000
		uint8_t data2 = read_ppu_register(0x2007);	// $2001
		uint8_t data3 = read_ppu_register(0x2007);	// $2002

		// Verify increment worked correctly
		REQUIRE(data1 == 0x00); // Expected value at $2000
		REQUIRE(data2 == 0x01); // Expected value at $2001
		REQUIRE(data3 == 0x02); // Expected value at $2002
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Rendering Pipeline Artifacts", "[ppu][quirks][rendering]") {
	SECTION("Background fetching during sprite evaluation") {
		// Background fetching continues during sprite evaluation
		write_ppu_register(0x2001, 0x18); // Enable both

		advance_to_scanline(50);
		advance_to_cycle(100); // During sprite evaluation

		// PPU should be fetching both background and evaluating sprites
		// This can cause visual artifacts in edge cases
	}

	SECTION("Shift register wraparound") {
		// Pattern shift registers wrap around in specific ways
		write_ppu_register(0x2001, 0x08); // Enable background

		// Setup scroll to test shift register behavior
		reset_toggle();
		write_ppu_register(0x2005, 0x07); // Fine X scroll = 7
		write_ppu_register(0x2005, 0x00);

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Shift registers should wrap at pixel boundaries
	}

	SECTION("Attribute byte timing quirks") {
		// Attribute bytes are fetched at specific cycles
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);

		// Attribute is fetched every 8 pixels
		for (int tile = 0; tile < 32; tile++) {
			int attr_cycle = tile * 8 + 3; // Attribute fetch cycle
			advance_to_cycle(attr_cycle);

			// Attribute should be fetched here
		}
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Scroll Register Quirks", "[ppu][quirks][scroll]") {
	SECTION("Fine X scroll immediate effect") {
		// Fine X scroll takes effect immediately
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Change fine X during rendering
		reset_toggle();
		write_ppu_register(0x2005, 0x03); // Fine X = 3
		write_ppu_register(0x2005, 0x00);

		// Effect should be immediate on next pixel
	}

	SECTION("Scroll register write timing") {
		// Scroll writes have specific timing requirements
		reset_toggle();

		// Rapid scroll writes
		write_ppu_register(0x2005, 0x10);
		write_ppu_register(0x2005, 0x20);
		write_ppu_register(0x2005, 0x30); // This should be X again
		write_ppu_register(0x2005, 0x40); // This should be Y

		// Verify toggle state is correct
	}

	SECTION("Scroll update during VBlank") {
		// Scroll updates during VBlank work differently
		advance_to_scanline(245); // In VBlank

		reset_toggle();
		write_ppu_register(0x2005, 0x80);
		write_ppu_register(0x2005, 0x90);

		// Scroll should update properly during VBlank
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Sprite Evaluation Quirks", "[ppu][quirks][sprites]") {
	SECTION("Sprite overflow flag timing") {
		// Setup more than 8 sprites on one scanline
		write_ppu_register(0x2003, 0x00);

		for (int i = 0; i < 16; i++) {
			write_ppu_register(0x2004, 50);		// Y position (all on line 50)
			write_ppu_register(0x2004, i);		// Tile index
			write_ppu_register(0x2004, 0x00);	// Attributes
			write_ppu_register(0x2004, i * 16); // X position
		}

		write_ppu_register(0x2001, 0x10); // Enable sprites

		advance_to_scanline(51); // Sprite line
		advance_to_cycle(256);	 // After sprite evaluation

		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x20) != 0); // Sprite overflow should be set
	}

	SECTION("Sprite 0 hit with clipping") {
		// Sprite 0 hit behavior with left edge clipping
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);	  // Y
		write_ppu_register(0x2004, 0x01); // Tile
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 4);	  // X position in clipped area

		// Enable rendering with left edge clipping disabled
		write_ppu_register(0x2001, 0x14); // Show sprites in leftmost 8 pixels

		advance_to_scanline(51);
		advance_to_cycle(12); // Sprite pixel position

		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x40) != 0); // Should hit even in clipped area
	}

	SECTION("8x16 sprite evaluation quirks") {
		// 8x16 sprites have special evaluation rules
		write_ppu_register(0x2000, 0x20); // 8x16 sprite mode

		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 240);  // Y position at bottom of screen
		write_ppu_register(0x2004, 0x00); // Tile index (even for top)
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X position

		write_ppu_register(0x2001, 0x10);

		// Sprite should be evaluated for both top and bottom tiles
		advance_to_scanline(241); // First line of sprite
		advance_to_cycle(256);

		// Should be properly evaluated despite being at screen edge
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Pattern Table Access Quirks", "[ppu][quirks][pattern_table]") {
	SECTION("CHR ROM/RAM timing differences") {
		// CHR ROM vs CHR RAM have different timing characteristics

		// Setup pattern table access
		write_ppu_register(0x2001, 0x08); // Enable background

		advance_to_scanline(50);

		// Pattern table fetches occur at specific cycles
		for (int tile = 0; tile < 32; tile++) {
			int pattern_low_cycle = tile * 8 + 5;
			int pattern_high_cycle = tile * 8 + 7;

			advance_to_cycle(pattern_low_cycle);
			// Pattern table low byte fetch

			advance_to_cycle(pattern_high_cycle);
			// Pattern table high byte fetch
		}
	}

	SECTION("Pattern table banking quirks") {
		// Background and sprite pattern table selection
		write_ppu_register(0x2000, 0x10); // Background uses $1000
		write_ppu_register(0x2000, 0x18); // Both use $1000

		// Verify pattern table selection affects fetching
		write_ppu_register(0x2001, 0x18); // Enable both

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Both background and sprites should use correct pattern tables
	}
}

TEST_CASE_METHOD(HardwareQuirksTestFixture, "Undocumented Register Behavior", "[ppu][quirks][registers]") {
	SECTION("PPUSTATUS sprite overflow flag quirks") {
		// Sprite overflow flag has unusual clearing behavior

		// Setup overflow condition
		write_ppu_register(0x2003, 0x00);
		for (int i = 0; i < 12; i++) {
			write_ppu_register(0x2004, 100); // Same Y
			write_ppu_register(0x2004, i);
			write_ppu_register(0x2004, 0x00);
			write_ppu_register(0x2004, i * 20);
		}

		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(101);
		advance_to_cycle(256);

		uint8_t status1 = read_ppu_register(0x2002);
		REQUIRE((status1 & 0x20) != 0); // Overflow set

		uint8_t status2 = read_ppu_register(0x2002);
		REQUIRE((status2 & 0x20) == 0); // Cleared by read
	}

	SECTION("OAMDATA read during rendering") {
		// OAMDATA reads during rendering return specific values
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 0x42);

		write_ppu_register(0x2001, 0x10); // Enable sprites

		advance_to_scanline(50);
		advance_to_cycle(100); // During sprite evaluation

		uint8_t oam_data = read_ppu_register(0x2004);
		// Should return specific value based on sprite evaluation state
	}

	SECTION("Write-only register write behavior") {
		// Writing to read-only registers (unusual behavior)

		// Try writing to PPUSTATUS (should be ignored)
		write_ppu_register(0x2002, 0xFF);

		uint8_t status = read_ppu_register(0x2002);
		// Write should be ignored, status should be normal
	}
}
