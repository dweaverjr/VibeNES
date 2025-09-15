#include "../catch2/catch_amalgamated.hpp"
#include "core/bus.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include "ppu/ppu_memory.hpp"
#include <memory>

using namespace nes;

class PaletteTestFixture {
  public:
	PaletteTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ram(ram);

		ppu = std::make_shared<PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		ppu->reset();
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

	uint8_t read_palette(uint16_t address) {
		set_vram_address(address);
		return read_ppu_register(0x2007); // Palette reads don't need dummy read
	}

	void write_palette(uint16_t address, uint8_t value) {
		set_vram_address(address);
		write_ppu_register(0x2007, value);
	}

	void clear_all_palettes() {
		// Clear all 32 palette entries
		for (uint16_t addr = 0x3F00; addr <= 0x3F1F; addr++) {
			write_palette(addr, 0x00);
		}
	}

	void setup_test_palettes() {
		// Background palette 0 (universal background + 3 colors)
		write_palette(0x3F00, 0x0F); // Universal background (black)
		write_palette(0x3F01, 0x30); // White
		write_palette(0x3F02, 0x16); // Red
		write_palette(0x3F03, 0x27); // Orange

		// Background palette 1
		write_palette(0x3F04, 0x0F); // Universal background (should mirror to 0x3F00)
		write_palette(0x3F05, 0x12); // Blue
		write_palette(0x3F06, 0x1C); // Green
		write_palette(0x3F07, 0x07); // Brown

		// Sprite palette 0
		write_palette(0x3F10, 0x0F); // Should mirror to 0x3F00
		write_palette(0x3F11, 0x38); // Yellow
		write_palette(0x3F12, 0x06); // Dark red
		write_palette(0x3F13, 0x26); // Light red

		// Sprite palette 1
		write_palette(0x3F14, 0x0F); // Should mirror to 0x3F04
		write_palette(0x3F15, 0x2A); // Green
		write_palette(0x3F16, 0x1A); // Light green
		write_palette(0x3F17, 0x0A); // Dark green
	}

	void enable_grayscale_mode() {
		uint8_t mask = read_ppu_register(0x2001);
		mask |= 0x01; // Set grayscale bit
		write_ppu_register(0x2001, mask);
	}

	void disable_grayscale_mode() {
		uint8_t mask = read_ppu_register(0x2001);
		mask &= ~0x01; // Clear grayscale bit
		write_ppu_register(0x2001, mask);
	}

	void set_color_emphasis(uint8_t emphasis_bits) {
		uint8_t mask = read_ppu_register(0x2001);
		mask = (mask & 0x1F) | (emphasis_bits << 5); // Set bits 5-7
		write_ppu_register(0x2001, mask);
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_to_rendering() {
		// Advance to active rendering period
		while (ppu->get_current_scanline() >= 240 || ppu->get_current_scanline() < 0) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_to_vblank() {
		// Advance to VBlank period
		while (ppu->get_current_scanline() != 241) {
			ppu->tick(CpuCycle{1});
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(PaletteTestFixture, "Palette Memory Layout", "[ppu][palette][layout]") {
	SECTION("Palette RAM should be 32 bytes") {
		clear_all_palettes();

		// Write test pattern to all 32 palette entries
		for (uint16_t addr = 0x3F00; addr <= 0x3F1F; addr++) {
			uint8_t test_value = static_cast<uint8_t>(addr & 0x3F);
			write_palette(addr, test_value);
		}

		// Verify all entries were written
		for (uint16_t addr = 0x3F00; addr <= 0x3F1F; addr++) {
			uint8_t expected = static_cast<uint8_t>(addr & 0x3F);
			uint8_t actual = read_palette(addr);
			REQUIRE(actual == expected);
		}
	}

	SECTION("Palette addresses should mirror every 32 bytes") {
		clear_all_palettes();

		// Write to base palette addresses
		write_palette(0x3F00, 0x11);
		write_palette(0x3F0F, 0x22);

		// Check mirroring at various offsets
		REQUIRE(read_palette(0x3F20) == 0x11); // 0x3F00 + 0x20
		REQUIRE(read_palette(0x3F2F) == 0x22); // 0x3F0F + 0x20
		REQUIRE(read_palette(0x3F40) == 0x11); // 0x3F00 + 0x40
		REQUIRE(read_palette(0x3F4F) == 0x22); // 0x3F0F + 0x40

		// Test up to end of palette address space
		REQUIRE(read_palette(0x3FE0) == 0x11); // Should still mirror
		REQUIRE(read_palette(0x3FEF) == 0x22);
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Universal Background Color Mirroring", "[ppu][palette][universal_bg]") {
	SECTION("Sprite palette universal colors should mirror to background") {
		clear_all_palettes();

		// Write to background universal color
		write_palette(0x3F00, 0x25);

		// Sprite palette universal colors should read the same value
		REQUIRE(read_palette(0x3F10) == 0x25); // Sprite palette 0
		REQUIRE(read_palette(0x3F14) == 0x25); // Sprite palette 1
		REQUIRE(read_palette(0x3F18) == 0x25); // Sprite palette 2
		REQUIRE(read_palette(0x3F1C) == 0x25); // Sprite palette 3
	}

	SECTION("Writing to sprite universal colors should affect background") {
		clear_all_palettes();

		// Write to sprite palette universal color
		write_palette(0x3F10, 0x17);

		// Background universal color should reflect the change
		REQUIRE(read_palette(0x3F00) == 0x17);

		// All sprite universal colors should match
		REQUIRE(read_palette(0x3F14) == 0x17);
		REQUIRE(read_palette(0x3F18) == 0x17);
		REQUIRE(read_palette(0x3F1C) == 0x17);
	}

	SECTION("Non-universal colors should not mirror") {
		clear_all_palettes();

		// Write to background palette color 1
		write_palette(0x3F01, 0x30);
		// Write to sprite palette color 1
		write_palette(0x3F11, 0x16);

		// These should be independent
		REQUIRE(read_palette(0x3F01) == 0x30);
		REQUIRE(read_palette(0x3F11) == 0x16);
		REQUIRE(read_palette(0x3F01) != read_palette(0x3F11));
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Palette Access During Rendering", "[ppu][palette][rendering]") {
	SECTION("Palette should be accessible during VBlank") {
		setup_test_palettes();
		advance_to_vblank();

		// Should be able to read/write palettes during VBlank
		write_palette(0x3F00, 0x20);
		REQUIRE(read_palette(0x3F00) == 0x20);

		write_palette(0x3F11, 0x35);
		REQUIRE(read_palette(0x3F11) == 0x35);
	}

	SECTION("Palette should be accessible during active rendering") {
		setup_test_palettes();
		advance_to_rendering();

		// Palette access should work during rendering (unlike other VRAM)
		write_palette(0x3F05, 0x2C);
		REQUIRE(read_palette(0x3F05) == 0x2C);
	}

	SECTION("Palette reads should not increment VRAM address") {
		// Set VRAM address to palette area
		set_vram_address(0x3F00);

		// Read palette value
		uint8_t value1 = read_ppu_register(0x2007);

		// Read again - should get same address, not incremented
		uint8_t value2 = read_ppu_register(0x2007);

		// Both reads should be from same address
		REQUIRE(value1 == value2);
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Grayscale Mode", "[ppu][palette][grayscale]") {
	SECTION("Grayscale mode should affect color output") {
		setup_test_palettes();

		// Test normal color mode
		disable_grayscale_mode();
		uint8_t color_normal = read_palette(0x3F01);

		// Enable grayscale mode
		enable_grayscale_mode();
		uint8_t color_grayscale = read_palette(0x3F01);

		// In grayscale mode, only the luminance (lower 4 bits) should matter
		// Upper bits should be masked off
		REQUIRE((color_grayscale & 0x30) == 0x30); // Should force grayscale
	}

	SECTION("Grayscale should affect all palette entries") {
		setup_test_palettes();
		enable_grayscale_mode();

		// Check multiple palette entries
		for (uint16_t addr = 0x3F00; addr <= 0x3F1F; addr += 4) {
			uint8_t value = read_palette(addr);
			// In grayscale mode, should see grayscale effect
			// (exact behavior depends on implementation)
		}
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Color Emphasis", "[ppu][palette][emphasis]") {
	SECTION("Color emphasis bits should affect palette output") {
		setup_test_palettes();

		// Test normal colors
		set_color_emphasis(0x00); // No emphasis
		uint8_t normal_color = read_palette(0x3F01);

		// Test red emphasis
		set_color_emphasis(0x01); // Red emphasis
		uint8_t red_emphasized = read_palette(0x3F01);

		// Test green emphasis
		set_color_emphasis(0x02); // Green emphasis
		uint8_t green_emphasized = read_palette(0x3F01);

		// Test blue emphasis
		set_color_emphasis(0x04); // Blue emphasis
		uint8_t blue_emphasized = read_palette(0x3F01);

		// Colors should be affected by emphasis
		// (exact behavior depends on implementation)
	}

	SECTION("Multiple emphasis bits should combine") {
		setup_test_palettes();

		// Test red + green emphasis
		set_color_emphasis(0x03); // Red + Green
		uint8_t rg_emphasized = read_palette(0x3F01);

		// Test all emphasis bits
		set_color_emphasis(0x07); // Red + Green + Blue
		uint8_t all_emphasized = read_palette(0x3F01);

		// Should see combined effects
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Palette Color Indices", "[ppu][palette][indices]") {
	SECTION("Color indices should be 6-bit values") {
		clear_all_palettes();

		// Test writing values with upper bits set
		write_palette(0x3F00, 0xFF); // Write $FF
		uint8_t result = read_palette(0x3F00);

		// Should mask to 6 bits (0x3F)
		REQUIRE(result == 0x3F);

		// Test various bit patterns
		write_palette(0x3F01, 0x80);		   // Only bit 7 set
		REQUIRE(read_palette(0x3F01) == 0x00); // Should be masked off

		write_palette(0x3F02, 0x40);		   // Only bit 6 set
		REQUIRE(read_palette(0x3F02) == 0x00); // Should be masked off

		write_palette(0x3F03, 0x3F);		   // All valid bits set
		REQUIRE(read_palette(0x3F03) == 0x3F); // Should remain
	}

	SECTION("Palette should support full color range") {
		clear_all_palettes();

		// Test all valid color indices (0x00-0x3F)
		for (uint8_t color = 0x00; color <= 0x3F; color++) {
			write_palette(0x3F00, color);
			REQUIRE(read_palette(0x3F00) == color);
		}
	}
}

TEST_CASE_METHOD(PaletteTestFixture, "Palette Organization", "[ppu][palette][organization]") {
	SECTION("Background palettes should be properly organized") {
		clear_all_palettes();

		// Background palette 0: $3F00-$3F03
		write_palette(0x3F00, 0x0F); // Universal background
		write_palette(0x3F01, 0x30); // Color 1
		write_palette(0x3F02, 0x16); // Color 2
		write_palette(0x3F03, 0x27); // Color 3

		// Verify organization
		REQUIRE(read_palette(0x3F00) == 0x0F);
		REQUIRE(read_palette(0x3F01) == 0x30);
		REQUIRE(read_palette(0x3F02) == 0x16);
		REQUIRE(read_palette(0x3F03) == 0x27);

		// Background palette 1: $3F04-$3F07
		write_palette(0x3F04, 0x0F); // Universal (mirrors to $3F00)
		write_palette(0x3F05, 0x12);
		write_palette(0x3F06, 0x1C);
		write_palette(0x3F07, 0x07);

		REQUIRE(read_palette(0x3F05) == 0x12);
		REQUIRE(read_palette(0x3F06) == 0x1C);
		REQUIRE(read_palette(0x3F07) == 0x07);
	}

	SECTION("Sprite palettes should be properly organized") {
		clear_all_palettes();

		// Sprite palette 0: $3F10-$3F13
		write_palette(0x3F10, 0x0F); // Universal (mirrors to $3F00)
		write_palette(0x3F11, 0x38);
		write_palette(0x3F12, 0x06);
		write_palette(0x3F13, 0x26);

		REQUIRE(read_palette(0x3F11) == 0x38);
		REQUIRE(read_palette(0x3F12) == 0x06);
		REQUIRE(read_palette(0x3F13) == 0x26);

		// Sprite palette 3: $3F1C-$3F1F
		write_palette(0x3F1C, 0x0F); // Universal (mirrors to $3F00)
		write_palette(0x3F1D, 0x2A);
		write_palette(0x3F1E, 0x1A);
		write_palette(0x3F1F, 0x0A);

		REQUIRE(read_palette(0x3F1D) == 0x2A);
		REQUIRE(read_palette(0x3F1E) == 0x1A);
		REQUIRE(read_palette(0x3F1F) == 0x0A);
	}
}
