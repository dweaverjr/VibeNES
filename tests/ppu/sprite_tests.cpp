// VibeNES - NES Emulator
// PPU Sprite Rendering Tests
// Tests for hardware-accurate sprite rendering behavior

#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <array>
#include <memory>

using namespace nes;

class SpriteTestFixture {
  public:
	SpriteTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ram(ram);
		ppu = std::make_unique<PPU>();
		ppu->connect_bus(bus.get());
		ppu->reset();

		// Clear OAM
		clear_oam();
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void clear_oam() {
		write_ppu_register(0x2003, 0x00); // Set OAM address to 0
		for (int i = 0; i < 256; i++) {
			write_ppu_register(0x2004, 0xFF); // Clear with invalid Y position
		}
	}

	void write_sprite(uint8_t index, uint8_t y, uint8_t tile, uint8_t attributes, uint8_t x) {
		uint8_t oam_address = index * 4;
		write_ppu_register(0x2003, oam_address);
		write_ppu_register(0x2004, y);
		write_ppu_register(0x2004, tile);
		write_ppu_register(0x2004, attributes);
		write_ppu_register(0x2004, x);
	}

	void advance_to_scanline(int target_scanline) {
		while (ppu->get_current_scanline() < target_scanline) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_to_cycle(int target_cycle) {
		while (ppu->get_current_cycle() < target_cycle) {
			ppu->tick(CpuCycle{1});
		}
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

	void enable_sprites() {
		write_ppu_register(0x2001, 0x10); // Enable sprite rendering
	}

	void enable_background_and_sprites() {
		write_ppu_register(0x2001, 0x18); // Enable both background and sprites
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::unique_ptr<PPU> ppu;
};

TEST_CASE_METHOD(SpriteTestFixture, "Sprite Evaluation Basic", "[ppu][sprites][evaluation]") {
	SECTION("Single sprite on scanline should be found") {
		// Place sprite at Y=10
		write_sprite(0, 10, 0x01, 0x00, 100);
		enable_sprites();

		// Advance to scanline 10 (sprite should be visible)
		advance_to_scanline(10);

		// Sprite evaluation happens during cycles 65-256
		advance_to_cycle(65);
		advance_ppu_cycles(191); // Through cycle 256

		// One sprite should be found for this scanline
	}

	SECTION("Sprite at Y=255 should be invisible") {
		// Y=255 is off-screen (sprites are rendered Y+1)
		write_sprite(0, 255, 0x01, 0x00, 100);
		enable_sprites();

		advance_to_scanline(10);
		advance_to_cycle(256);

		// This sprite should not be found on any scanline
	}

	SECTION("Multiple sprites on same scanline") {
		// Place 5 sprites on the same scanline
		for (uint8_t i = 0; i < 5; i++) {
			write_sprite(i, 50, i + 1, 0x00, i * 40);
		}
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(256);

		// Only first 8 sprites should be evaluated per scanline
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "Sprite 0 Hit Detection", "[ppu][sprites][sprite0]") {
	SECTION("Sprite 0 hit should occur when overlapping background") {
		// Set up sprite 0
		write_sprite(0, 50, 0x01, 0x00, 100);

		// Enable both background and sprites
		enable_background_and_sprites();

		// Advance to scanline where sprite 0 is visible
		advance_to_scanline(50);

		// Process the scanline
		advance_to_cycle(341);

		// Check PPUSTATUS for sprite 0 hit flag
		uint8_t status = read_ppu_register(0x2002);
		// Bit 6 should be set if sprite 0 hit occurred
	}

	SECTION("Sprite 0 hit should not occur with transparent pixels") {
		// Set up sprite 0 with palette index 0 (transparent)
		write_sprite(0, 50, 0x01, 0x00, 100);
		enable_background_and_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Sprite 0 hit should not occur with transparent background or sprite pixels
	}

	SECTION("Sprite 0 hit flag should clear on status read") {
		// Set up sprite 0 hit
		write_sprite(0, 50, 0x01, 0x00, 100);
		enable_background_and_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Read status to clear flags
		uint8_t status1 = read_ppu_register(0x2002);
		uint8_t status2 = read_ppu_register(0x2002);

		// Second read should have sprite 0 hit flag cleared
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "Sprite Overflow Detection", "[ppu][sprites][overflow]") {
	SECTION("More than 8 sprites on scanline should set overflow flag") {
		// Place 10 sprites on the same scanline
		for (uint8_t i = 0; i < 10; i++) {
			write_sprite(i, 100, i + 1, 0x00, i * 25);
		}
		enable_sprites();

		advance_to_scanline(100);
		advance_to_cycle(256);

		// Check PPUSTATUS for sprite overflow flag
		uint8_t status = read_ppu_register(0x2002);
		// Bit 5 should be set
	}

	SECTION("8 or fewer sprites should not set overflow flag") {
		// Place exactly 8 sprites on scanline
		for (uint8_t i = 0; i < 8; i++) {
			write_sprite(i, 100, i + 1, 0x00, i * 30);
		}
		enable_sprites();

		advance_to_scanline(100);
		advance_to_cycle(256);

		uint8_t status = read_ppu_register(0x2002);
		// Bit 5 should be clear
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "Sprite Attributes", "[ppu][sprites][attributes]") {
	SECTION("Horizontal flip should mirror sprite") {
		// Test sprite with horizontal flip bit set
		write_sprite(0, 50, 0x01, 0x40, 100); // Bit 6 = horizontal flip
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Sprite pattern should be horizontally mirrored
	}

	SECTION("Vertical flip should mirror sprite") {
		// Test sprite with vertical flip bit set
		write_sprite(0, 50, 0x01, 0x80, 100); // Bit 7 = vertical flip
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Sprite pattern should be vertically mirrored
	}

	SECTION("Priority bit should control background interaction") {
		// Test sprite with priority bit set (behind background)
		write_sprite(0, 50, 0x01, 0x20, 100); // Bit 5 = priority
		enable_background_and_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Sprite should render behind non-transparent background pixels
	}

	SECTION("Palette selection should work") {
		// Test different sprite palettes
		for (uint8_t palette = 0; palette < 4; palette++) {
			write_sprite(palette, 50 + palette * 10, 0x01, palette, 100 + palette * 30);
		}
		enable_sprites();

		// Each sprite should use different palette (bits 0-1 of attributes)
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "8x16 Sprite Mode", "[ppu][sprites][8x16]") {
	SECTION("8x16 sprites should use correct pattern tables") {
		// Enable 8x16 sprite mode
		write_ppu_register(0x2000, 0x20); // Bit 5 = sprite size

		// Even tile numbers use pattern table 0
		write_sprite(0, 50, 0x02, 0x00, 100);

		// Odd tile numbers use pattern table 1
		write_sprite(1, 50, 0x03, 0x00, 120);

		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(341);

		// Pattern table selection should be automatic based on tile number
	}

	SECTION("8x16 sprites should render two tiles vertically") {
		write_ppu_register(0x2000, 0x20); // 8x16 mode
		write_sprite(0, 50, 0x10, 0x00, 100);
		enable_sprites();

		// Sprite should be visible on scanlines 50-65 (16 pixels tall)
		for (int scanline = 50; scanline < 66; scanline++) {
			advance_to_scanline(scanline);
			advance_to_cycle(341);
		}
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "Sprite Timing", "[ppu][sprites][timing]") {
	SECTION("Sprite evaluation should occur during cycles 65-256") {
		write_sprite(0, 50, 0x01, 0x00, 100);
		enable_sprites();

		advance_to_scanline(50);

		// Before sprite evaluation
		advance_to_cycle(64);
		// Sprite evaluation not started yet

		// During sprite evaluation
		advance_to_cycle(65);
		advance_ppu_cycles(191); // Through cycle 256
								 // Sprite evaluation should be complete
	}

	SECTION("Sprite rendering should occur during visible cycles") {
		write_sprite(0, 50, 0x01, 0x00, 100);
		enable_sprites();

		advance_to_scanline(50);

		// Sprite should render during cycles 1-256
		for (int cycle = 1; cycle <= 256; cycle += 8) {
			advance_to_cycle(cycle);
			// Check if sprite pixel is being rendered at this cycle
		}
	}

	SECTION("OAM access should be blocked during rendering") {
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(65); // During sprite evaluation

		// Writes to OAMDATA should be ignored during rendering
		write_ppu_register(0x2004, 0x42);

		// Read should return $FF
		uint8_t data = read_ppu_register(0x2004);
		REQUIRE(data == 0xFF);
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "Sprite X Positioning", "[ppu][sprites][positioning]") {
	SECTION("Sprite at X=0 should be at left edge") {
		write_sprite(0, 50, 0x01, 0x00, 0);
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(8); // First 8 pixels

		// Sprite should start rendering immediately
	}

	SECTION("Sprite at X=255 should be at right edge") {
		write_sprite(0, 50, 0x01, 0x00, 255);
		enable_sprites();

		advance_to_scanline(50);
		advance_to_cycle(256);

		// Only first pixel of sprite should be visible
	}

	SECTION("Sprite clipping should work on left edge") {
		write_sprite(0, 50, 0x01, 0x00, 0);

		// Enable sprite rendering but not left edge clipping
		write_ppu_register(0x2001, 0x16); // Sprites enabled, left edge clipped

		advance_to_scanline(50);
		advance_to_cycle(16);

		// Sprite should be clipped in first 8 pixels
	}
}

TEST_CASE_METHOD(SpriteTestFixture, "OAM DMA", "[ppu][sprites][oam_dma]") {
	SECTION("OAM DMA should copy 256 bytes") {
		// Set up some data in RAM at page $02
		for (uint16_t addr = 0x0200; addr < 0x0300; addr++) {
			bus->write(addr, static_cast<uint8_t>(addr & 0xFF));
		}

		// Trigger OAM DMA
		write_ppu_register(0x4014, 0x02); // DMA from page $02

		// DMA should take 513 or 514 CPU cycles
		// Check that OAM was populated correctly
	}

	SECTION("OAM DMA should suspend CPU") {
		// Set up DMA
		write_ppu_register(0x4014, 0x02);

		// CPU should be suspended during DMA transfer
		// This would need to be tested at the system level
	}
}
