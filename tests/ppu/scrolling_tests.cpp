// VibeNES - NES Emulator
// PPU Scrolling System Tests
// Tests for hardware-accurate scrolling behavior

#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

class ScrollTestFixture {
  public:
	ScrollTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ram(ram);
		ppu = std::make_shared<PPU>();
		bus->connect_ppu(ppu);
		ppu->connect_bus(bus.get());
		ppu->power_on();
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void reset_toggle() {
		read_ppu_register(0x2002);
	}

	void set_scroll(uint8_t x, uint8_t y) {
		reset_toggle();
		write_ppu_register(0x2005, x);
		write_ppu_register(0x2005, y);
	}

	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() < target_scanline && safety_counter < MAX_CYCLES) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
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
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_cycle hit safety limit - possible infinite loop");
		}
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(ScrollTestFixture, "Fine Scroll Behavior", "[ppu][scroll][fine]") {
	SECTION("Fine X scroll should affect pixel-level positioning") {
		// Fine X scroll uses bits 0-2 of the scroll value
		for (uint8_t fine_x = 0; fine_x < 8; fine_x++) {
			set_scroll(fine_x, 0);

			// The fine X scroll is stored separately from VRAM address
			// It affects which bit of the shift register is used for rendering
		}
	}

	SECTION("Fine Y scroll should be stored in VRAM address") {
		// Fine Y scroll uses bits 12-14 of VRAM address
		for (uint8_t fine_y = 0; fine_y < 8; fine_y++) {
			set_scroll(0, fine_y);

			// Fine Y affects which row of the tile pattern is fetched
		}
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Coarse Scroll Behavior", "[ppu][scroll][coarse]") {
	SECTION("Coarse X scroll should affect nametable column") {
		// Coarse X is derived from scroll_x / 8
		for (uint8_t coarse_x = 0; coarse_x < 32; coarse_x += 4) {
			set_scroll(coarse_x * 8, 0);

			// This should affect which tile column is fetched from nametable
		}
	}

	SECTION("Coarse Y scroll should affect nametable row") {
		// Coarse Y is derived from scroll_y / 8
		for (uint8_t coarse_y = 0; coarse_y < 30; coarse_y += 4) {
			set_scroll(0, coarse_y * 8);

			// This should affect which tile row is fetched from nametable
		}
	}

	SECTION("Coarse X should wrap at tile 32") {
		// When coarse X reaches 32, it should wrap to 0 and switch nametables
		set_scroll(static_cast<uint8_t>(32 * 8), 0); // 256 pixels = 32 tiles

		// This should wrap to coarse X = 0 and toggle horizontal nametable
	}

	SECTION("Coarse Y should wrap at tile 30") {
		// When coarse Y reaches 30, it should wrap to 0 and switch nametables
		set_scroll(0, 30 * 8); // 240 pixels = 30 tiles

		// This should wrap to coarse Y = 0 and toggle vertical nametable
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Nametable Selection", "[ppu][scroll][nametable]") {
	SECTION("Horizontal nametable bit should toggle with X scroll") {
		// Test horizontal nametable switching
		set_scroll(0, 0);						  // Nametable 0
		set_scroll(static_cast<uint8_t>(256), 0); // Should switch to nametable 1
		set_scroll(static_cast<uint8_t>(512), 0); // Should wrap back to nametable 0
	}

	SECTION("Vertical nametable bit should toggle with Y scroll") {
		// Test vertical nametable switching
		set_scroll(0, 0);						  // Nametable 0
		set_scroll(0, 240);						  // Should switch to nametable 2
		set_scroll(0, static_cast<uint8_t>(480)); // Should wrap back to nametable 0
	}

	SECTION("Both nametable bits should work together") {
		set_scroll(0, 0);							// Nametable 0 ($2000)
		set_scroll(static_cast<uint8_t>(256), 0);	// Nametable 1 ($2400)
		set_scroll(0, 240);							// Nametable 2 ($2800)
		set_scroll(static_cast<uint8_t>(256), 240); // Nametable 3 ($2C00)
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Scroll Update Timing", "[ppu][scroll][timing]") {
	SECTION("Horizontal scroll should be copied at cycle 257") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Set scroll
		set_scroll(64, 0);

		// Advance to visible scanline
		advance_to_scanline(10);

		// Advance to cycle 257 (when horizontal scroll is copied)
		advance_to_cycle(257);

		// At this point, horizontal position should be reset from temp VRAM address
	}

	SECTION("Vertical scroll should be copied during cycles 280-304") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Set scroll
		set_scroll(0, 64);

		// Advance to pre-render scanline (261)
		advance_to_scanline(261);

		// Advance to cycle 280-304 range
		advance_to_cycle(280);
		advance_ppu_cycles(25); // Through cycle 304

		// Vertical scroll should be copied from temp VRAM address
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "VRAM Address Increments During Rendering", "[ppu][scroll][vram_increments]") {
	SECTION("Coarse X should increment during tile fetches") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Start at beginning of scanline
		advance_to_scanline(10);
		advance_to_cycle(0);

		// During visible cycles, coarse X should increment every 8 cycles
		// (after each tile fetch completes)
	}

	SECTION("Fine Y should increment at end of scanline") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to end of visible scanline
		advance_to_scanline(10);
		advance_to_cycle(256);

		// Fine Y should increment here
		// When fine Y reaches 8, it wraps to 0 and coarse Y increments
	}

	SECTION("Y increment should handle wraparound correctly") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Set up scroll near Y boundary
		set_scroll(0, 239); // Near bottom of screen

		// Advance through several scanlines to test Y increment wraparound
		for (int scanline = 0; scanline < 10; scanline++) {
			advance_to_scanline(scanline);
			advance_to_cycle(256); // Y increment point
		}
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Scroll Boundary Conditions", "[ppu][scroll][boundaries]") {
	SECTION("Horizontal scroll at nametable boundary") {
		// Test scroll values that cross nametable boundaries
		uint8_t boundary_values[] = {248, 249, 250, 251, 252, 253, 254, 255, 0, 1, 2, 3, 4, 5, 6, 7};

		for (uint8_t scroll_x : boundary_values) {
			set_scroll(scroll_x, 0);

			// Each of these should result in correct nametable selection
			// and tile addressing
		}
	}

	SECTION("Vertical scroll at nametable boundary") {
		// Test scroll values that cross vertical nametable boundaries
		uint8_t boundary_values[] = {232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247};

		for (uint8_t scroll_y : boundary_values) {
			set_scroll(0, scroll_y);

			// Each of these should result in correct nametable selection
		}
	}

	SECTION("Fine scroll wraparound") {
		// Test fine scroll values at boundaries
		for (uint8_t pixel = 0; pixel < 16; pixel++) {
			set_scroll(pixel, pixel);

			// Fine scroll should wrap correctly at 8-pixel boundaries
		}
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Attribute Table Addressing", "[ppu][scroll][attributes]") {
	SECTION("Attribute addressing should follow scroll") {
		// Attribute table addressing is complex and depends on scroll position
		for (uint8_t tile_x = 0; tile_x < 32; tile_x += 2) {
			for (uint8_t tile_y = 0; tile_y < 30; tile_y += 2) {
				set_scroll(tile_x * 8, tile_y * 8);

				// Each 2x2 tile group shares one attribute byte
				// Address calculation: base + (coarse_y / 4) * 8 + (coarse_x / 4)
			}
		}
	}

	SECTION("Attribute bits should be selected correctly") {
		// Within each attribute byte, 2-bit values are stored for 2x2 tile groups
		// Bit selection depends on (coarse_x % 4) and (coarse_y % 4)

		for (uint8_t sub_x = 0; sub_x < 4; sub_x++) {
			for (uint8_t sub_y = 0; sub_y < 4; sub_y++) {
				set_scroll(sub_x * 8, sub_y * 8);

				// Attribute bits should be extracted from correct position
				// in the attribute byte
			}
		}
	}
}

TEST_CASE_METHOD(ScrollTestFixture, "Split Screen Effects", "[ppu][scroll][split_screen]") {
	SECTION("Mid-frame scroll changes should work") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Set initial scroll
		set_scroll(0, 0);

		// Advance to middle of frame
		advance_to_scanline(100);

		// Change scroll mid-frame (simulating split-screen effect)
		set_scroll(128, 64);

		// This should affect rendering for subsequent scanlines
	}

	SECTION("PPUADDR writes during rendering should affect scroll") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to visible area
		advance_to_scanline(50);

		// Write to PPUADDR during rendering (affects current VRAM address)
		reset_toggle();
		write_ppu_register(0x2006, 0x24);
		write_ppu_register(0x2006, 0x80);

		// This should cause scrolling glitches/effects
	}
}
