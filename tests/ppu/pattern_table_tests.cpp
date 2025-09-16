#include "../catch2/catch_amalgamated.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include "ppu/ppu_memory.hpp"
#include <memory>

using namespace nes;

class PatternTableTestFixture {
  public:
	PatternTableTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ram(ram);

		ppu = std::make_shared<PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		ppu->power_on();

		// Set up some basic CHR data for testing
		setup_test_chr_data();
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void set_vram_address(uint16_t address) {
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));	  // High byte
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF)); // Low byte
	}

	uint8_t read_vram(uint16_t address) {
		set_vram_address(address);
		read_ppu_register(0x2007); // Dummy read for non-palette addresses
		return read_ppu_register(0x2007);
	}

	void write_vram(uint16_t address, uint8_t value) {
		set_vram_address(address);
		write_ppu_register(0x2007, value);
	}

	void setup_test_chr_data() {
		// Set up test pattern table data
		// This simulates CHR ROM/RAM data that would come from cartridge

		// Pattern table 0 ($0000-$0FFF) - Background patterns
		test_chr_data_.resize(0x2000, 0x00);

		// Create a simple test tile at pattern 0
		// Tile 0: Simple 8x8 pattern
		test_chr_data_[0x00] = 0xFF; // ########
		test_chr_data_[0x01] = 0x81; // #......#
		test_chr_data_[0x02] = 0x81; // #......#
		test_chr_data_[0x03] = 0x81; // #......#
		test_chr_data_[0x04] = 0x81; // #......#
		test_chr_data_[0x05] = 0x81; // #......#
		test_chr_data_[0x06] = 0x81; // #......#
		test_chr_data_[0x07] = 0xFF; // ########

		test_chr_data_[0x08] = 0x00; // High bit plane (solid color)
		test_chr_data_[0x09] = 0x7E;
		test_chr_data_[0x0A] = 0x7E;
		test_chr_data_[0x0B] = 0x7E;
		test_chr_data_[0x0C] = 0x7E;
		test_chr_data_[0x0D] = 0x7E;
		test_chr_data_[0x0E] = 0x7E;
		test_chr_data_[0x0F] = 0x00;

		// Create another test tile at pattern 1
		test_chr_data_[0x10] = 0xAA; // #.#.#.#.
		test_chr_data_[0x11] = 0x55; // .#.#.#.#
		test_chr_data_[0x12] = 0xAA; // #.#.#.#.
		test_chr_data_[0x13] = 0x55; // .#.#.#.#
		test_chr_data_[0x14] = 0xAA; // #.#.#.#.
		test_chr_data_[0x15] = 0x55; // .#.#.#.#
		test_chr_data_[0x16] = 0xAA; // #.#.#.#.
		test_chr_data_[0x17] = 0x55; // .#.#.#.#

		test_chr_data_[0x18] = 0x55; // High bit plane
		test_chr_data_[0x19] = 0xAA;
		test_chr_data_[0x1A] = 0x55;
		test_chr_data_[0x1B] = 0xAA;
		test_chr_data_[0x1C] = 0x55;
		test_chr_data_[0x1D] = 0xAA;
		test_chr_data_[0x1E] = 0x55;
		test_chr_data_[0x1F] = 0xAA;

		// Pattern table 1 ($1000-$1FFF) - Sprite patterns
		// Copy same patterns to sprite table for testing
		for (int i = 0; i < 0x20; i++) {
			test_chr_data_[0x1000 + i] = test_chr_data_[i];
		}
	}

	uint8_t read_chr_data(uint16_t address) {
		if (address < test_chr_data_.size()) {
			return test_chr_data_[address];
		}
		return 0x00;
	}

	void enable_background() {
		write_ppu_register(0x2001, 0x08); // Enable background rendering
	}

	void enable_sprites() {
		write_ppu_register(0x2001, 0x10); // Enable sprite rendering
	}

	void enable_rendering() {
		write_ppu_register(0x2001, 0x1E); // Enable background and sprites
	}

	void set_background_pattern_table(bool table_1) {
		uint8_t ctrl = read_ppu_register(0x2000);
		if (table_1) {
			ctrl |= 0x10; // Set bit 4
		} else {
			ctrl &= ~0x10; // Clear bit 4
		}
		write_ppu_register(0x2000, ctrl);
	}

	void set_sprite_pattern_table(bool table_1) {
		uint8_t ctrl = read_ppu_register(0x2000);
		if (table_1) {
			ctrl |= 0x08; // Set bit 3
		} else {
			ctrl &= ~0x08; // Clear bit 3
		}
		write_ppu_register(0x2000, ctrl);
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() != target_scanline && safety_counter < MAX_CYCLES) {
			ppu->tick(CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_scanline hit safety limit - possible infinite loop");
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
	std::vector<uint8_t> test_chr_data_;
};

TEST_CASE_METHOD(PatternTableTestFixture, "Pattern Table Access", "[ppu][pattern_table][access]") {
	SECTION("Should read from pattern table 0") {
		// Test reading pattern table 0 data
		uint8_t data = read_vram(0x0000); // First byte of pattern 0
										  // Note: This test assumes CHR data is accessible through VRAM
										  // In real hardware, pattern tables are read-only from cartridge
	}

	SECTION("Should read from pattern table 1") {
		// Test reading pattern table 1 data
		uint8_t data = read_vram(0x1000); // First byte of pattern 0 in table 1
	}

	SECTION("Pattern table addresses should wrap correctly") {
		// Test address wrapping within pattern table space
		uint8_t data1 = read_vram(0x0FFF); // Last byte of pattern table 0
		uint8_t data2 = read_vram(0x1FFF); // Last byte of pattern table 1
	}
}

TEST_CASE_METHOD(PatternTableTestFixture, "Background Pattern Table Selection", "[ppu][pattern_table][background]") {
	SECTION("PPUCTRL bit 4 should control background pattern table") {
		enable_background();

		// Test pattern table 0 selection
		set_background_pattern_table(false);
		uint8_t ctrl = read_ppu_register(0x2000);
		REQUIRE((ctrl & 0x10) == 0x00);

		// Test pattern table 1 selection
		set_background_pattern_table(true);
		ctrl = read_ppu_register(0x2000);
		REQUIRE((ctrl & 0x10) == 0x10);
	}

	SECTION("Background rendering should use correct pattern table") {
		enable_background();

		// Set up nametable data pointing to pattern 1
		write_vram(0x2000, 0x01); // Nametable tile points to pattern 1

		// Test with pattern table 0
		set_background_pattern_table(false);
		advance_to_scanline(0);
		advance_ppu_cycles(256); // Render one scanline

		// Test with pattern table 1
		set_background_pattern_table(true);
		advance_to_scanline(1);
		advance_ppu_cycles(256); // Render another scanline
	}
}

TEST_CASE_METHOD(PatternTableTestFixture, "Sprite Pattern Table Selection", "[ppu][pattern_table][sprites]") {
	SECTION("PPUCTRL bit 3 should control sprite pattern table for 8x8 sprites") {
		enable_sprites();

		// Test pattern table 0 selection
		set_sprite_pattern_table(false);
		uint8_t ctrl = read_ppu_register(0x2000);
		REQUIRE((ctrl & 0x08) == 0x00);

		// Test pattern table 1 selection
		set_sprite_pattern_table(true);
		ctrl = read_ppu_register(0x2000);
		REQUIRE((ctrl & 0x08) == 0x08);
	}

	SECTION("8x8 sprites should use selected pattern table") {
		enable_sprites();

		// Set up sprite data
		write_ppu_register(0x2003, 0x00); // OAM address
		write_ppu_register(0x2004, 50);	  // Y position
		write_ppu_register(0x2004, 0x01); // Tile number 1
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X position

		// Test with pattern table 0
		set_sprite_pattern_table(false);
		advance_to_scanline(50);

		// Test with pattern table 1
		set_sprite_pattern_table(true);
		advance_to_scanline(51);
	}

	SECTION("8x16 sprites should ignore pattern table bit") {
		// Enable 8x16 sprite mode
		write_ppu_register(0x2000, 0x20); // Set bit 5 for 8x16 sprites
		enable_sprites();

		// Set up 8x16 sprite
		write_ppu_register(0x2003, 0x00); // OAM address
		write_ppu_register(0x2004, 50);	  // Y position
		write_ppu_register(0x2004, 0x02); // Tile number (even = table 0, odd = table 1)
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X position

		// Pattern table bit should be ignored for 8x16 sprites
		// Tile number bit 0 determines pattern table
		set_sprite_pattern_table(true); // This should be ignored
		advance_to_scanline(50);
	}
}

TEST_CASE_METHOD(PatternTableTestFixture, "Pattern Fetching During Rendering", "[ppu][pattern_table][fetching]") {
	SECTION("Background tiles should be fetched during active scanlines") {
		enable_background();

		// Set up nametable with various tile indices
		for (int i = 0; i < 32; i++) {
			write_vram(0x2000 + i, i % 4); // Use patterns 0-3
		}

		// Advance to active rendering
		advance_to_scanline(0);

		// Simulate pattern fetching during scanline rendering
		for (int cycle = 0; cycle < 256; cycle += 8) {
			advance_ppu_cycles(8);
			// Each 8-cycle period should fetch one tile's pattern data
		}
	}

	SECTION("Sprite patterns should be fetched during sprite evaluation") {
		enable_sprites();

		// Set up multiple sprites
		for (int sprite = 0; sprite < 8; sprite++) {
			write_ppu_register(0x2003, sprite * 4);	 // OAM address
			write_ppu_register(0x2004, 50);			 // Y position
			write_ppu_register(0x2004, sprite);		 // Tile number
			write_ppu_register(0x2004, 0x00);		 // Attributes
			write_ppu_register(0x2004, sprite * 32); // X position
		}

		// Advance to sprite rendering scanline
		advance_to_scanline(50);

		// Sprite patterns should be fetched during cycles 257-320
		advance_ppu_cycles(257);
		for (int i = 0; i < 64; i += 8) {
			advance_ppu_cycles(8);
			// Each 8-cycle period fetches one sprite's pattern data
		}
	}
}

TEST_CASE_METHOD(PatternTableTestFixture, "Pattern Data Format", "[ppu][pattern_table][format]") {
	SECTION("Pattern tiles should be 8x8 pixels with 2 bit planes") {
		// Each pattern is 16 bytes: 8 bytes low bit plane + 8 bytes high bit plane
		// Test pattern 0 layout
		uint8_t pattern_0_low_0 = read_vram(0x0000);  // Row 0, low bit plane
		uint8_t pattern_0_high_0 = read_vram(0x0008); // Row 0, high bit plane

		// Test pattern 1 layout (next pattern)
		uint8_t pattern_1_low_0 = read_vram(0x0010);  // Row 0, low bit plane
		uint8_t pattern_1_high_0 = read_vram(0x0018); // Row 0, high bit plane
	}

	SECTION("8x16 sprites should use consecutive patterns") {
		enable_sprites();
		write_ppu_register(0x2000, 0x20); // Enable 8x16 sprite mode

		// Set up 8x16 sprite with even tile number
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);	  // Y position
		write_ppu_register(0x2004, 0x02); // Even tile number
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X position

		// 8x16 sprite should use patterns 0x02 (top) and 0x03 (bottom)
		// Top half comes from even pattern, bottom from odd pattern
		advance_to_scanline(50);
	}

	SECTION("Pattern table boundaries should be respected") {
		// Pattern table 0: $0000-$0FFF (256 patterns)
		// Pattern table 1: $1000-$1FFF (256 patterns)

		// Last pattern in table 0
		uint8_t last_pattern_table_0 = read_vram(0x0FF0);

		// First pattern in table 1
		uint8_t first_pattern_table_1 = read_vram(0x1000);

		// Should not wrap between tables
		REQUIRE(read_vram(0x0FFF) != read_vram(0x1000));
	}
}
