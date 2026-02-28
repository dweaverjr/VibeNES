#include <catch2/catch_all.hpp>

#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include "ppu/test_chr_data.hpp"

#include <cstdint>
#include <memory>

using namespace nes;

namespace {

// Test fixture for viewport and scrolling tests
class ViewportScrollingFixture {
  public:
	ViewportScrollingFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());
		ppu = std::make_shared<PPU>();

		// Create test cartridge with CHR ROM and VERTICAL mirroring for horizontal scrolling
		// (Like Super Mario Bros. - left/right nametables are distinct)
		RomData rom_data;
		rom_data.mapper_id = 0;				// NROM
		rom_data.prg_rom_pages = 2;			// 32KB PRG ROM
		rom_data.chr_rom_pages = 1;			// 8KB CHR ROM
		rom_data.vertical_mirroring = true; // VERTICAL mirroring (horizontal scrolling)
		rom_data.battery_backed_ram = false;
		rom_data.trainer_present = false;
		rom_data.four_screen_vram = false;
		rom_data.prg_rom.resize(32768, 0xEA); // Fill with NOP
		rom_data.chr_rom.resize(8192, 0x00);  // Empty CHR ROM
		rom_data.chr_rom[0x10] = 0xFF;		  // Tile 1 = solid
		rom_data.chr_rom[0x11] = 0xFF;
		rom_data.chr_rom[0x12] = 0xFF;
		rom_data.chr_rom[0x13] = 0xFF;
		rom_data.chr_rom[0x14] = 0xFF;
		rom_data.chr_rom[0x15] = 0xFF;
		rom_data.chr_rom[0x16] = 0xFF;
		rom_data.chr_rom[0x17] = 0xFF;
		rom_data.filename = "test_horizontal_scroll.nes";
		rom_data.valid = true;

		cartridge = std::make_shared<Cartridge>();
		cartridge->load_from_rom_data(rom_data);

		// Connect components
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);
		bus->connect_cpu(cpu);

		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);
		ppu->connect_cartridge(cartridge);
		ppu->connect_cpu(cpu.get());

		// Power on
		bus->power_on();
		ppu->power_on();

		// Process reset interrupt for CPU
		cpu->tick(CpuCycle{10});
	}

	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	void set_scroll(uint8_t x, uint8_t y) {
		// Read status to reset address latch
		read_ppu_register(0x2002);
		write_ppu_register(0x2005, x);
		write_ppu_register(0x2005, y);
	}

	void write_vram(uint16_t address, uint8_t value) {
		// Read status to reset address latch
		read_ppu_register(0x2002);
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
		write_ppu_register(0x2007, value);
	}

	uint8_t read_vram(uint16_t address) {
		// Read status to reset address latch
		read_ppu_register(0x2002);
		write_ppu_register(0x2006, static_cast<uint8_t>(address >> 8));
		write_ppu_register(0x2006, static_cast<uint8_t>(address & 0xFF));
		// Dummy read for non-palette addresses
		read_ppu_register(0x2007);
		return read_ppu_register(0x2007);
	}

	void advance_to_scanline(int target_scanline) {
		int safety = 100000;
		int current = ppu->get_current_scanline();

		// If already at target, return immediately
		if (current == target_scanline) {
			return;
		}

		// Handle wraparound: if we're past the target (e.g., at 261, want 0),
		// we need to wrap through the next frame
		if (current > target_scanline) {
			// First advance to end of frame (scanline wraps back to 0)
			while (ppu->get_current_scanline() > target_scanline && safety-- > 0) {
				ppu->tick(CpuCycle{1});
			}
		}

		// Now advance to target scanline
		while (ppu->get_current_scanline() < target_scanline && safety-- > 0) {
			ppu->tick(CpuCycle{1});
		}

		if (safety <= 0) {
			throw std::runtime_error("advance_to_scanline safety timeout");
		}
	}

	void advance_to_cycle(int target_cycle) {
		int safety = 100000;
		while (ppu->get_current_cycle() < target_cycle && safety-- > 0) {
			ppu->tick(CpuCycle{1});
		}
		if (safety <= 0) {
			throw std::runtime_error("advance_to_cycle safety timeout");
		}
	}

	void advance_frames(int frame_count) {
		uint64_t start_frame = ppu->get_frame_count();
		int safety = 1000000;
		while (ppu->get_frame_count() < start_frame + frame_count && safety-- > 0) {
			ppu->tick(CpuCycle{1});
		}
		if (safety <= 0) {
			throw std::runtime_error("advance_frames safety timeout");
		}
	}

	// Fill nametable with a pattern so we can verify which nametable is being read
	void fill_nametable(uint16_t base_address, uint8_t tile_value) {
		for (uint16_t offset = 0; offset < 960; ++offset) { // 32x30 tiles
			write_vram(base_address + offset, tile_value);
		}
	}

	// Fill a specific tile column in a nametable
	void fill_nametable_column(uint16_t base_address, uint8_t column, uint8_t tile_value) {
		for (uint8_t row = 0; row < 30; ++row) {
			write_vram(base_address + row * 32 + column, tile_value);
		}
	}

	// Get the tile ID that the PPU is currently fetching
	uint8_t get_current_tile_id() {
		auto debug_state = ppu->get_debug_state();
		return debug_state.current_tile_id;
	}

	// Get the next tile ID in the pipeline
	uint8_t get_next_tile_id() {
		auto debug_state = ppu->get_debug_state();
		return debug_state.next_tile_id;
	}

	// Initialize rendering - advances through pre-render to apply scroll settings
	void init_rendering() {
		// Advance to pre-render scanline to let scroll registers take effect
		advance_to_scanline(261);
		advance_to_cycle(280); // After vertical/horizontal scroll copy

		// CRITICAL: Must advance past the prefetch cycles (321-337)
		// The pre-render HBLANK fetches the first 2 tiles and loads shift registers
		// If we skip these cycles, the shift registers will be empty!
		advance_to_cycle(340); // After all prefetch cycles complete

		// Now advance to start of next visible frame
		advance_to_scanline(0);
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<Cartridge> cartridge;
	std::shared_ptr<APU> apu;
	std::shared_ptr<CPU6502> cpu;
	std::shared_ptr<PPU> ppu;
};

} // namespace

TEST_CASE_METHOD(ViewportScrollingFixture, "VRAM Write/Read Verification", "[ppu][viewport][debug]") {
	SECTION("VRAM writes should be readable") {
		// Write a known value
		write_vram(0x2000, 0xAA);

		// Read it back
		uint8_t value = read_vram(0x2000);
		REQUIRE(value == 0xAA);
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Horizontal Scrolling - Single Nametable", "[ppu][viewport][scrolling]") {
	SECTION("Scroll X = 0 (leftmost position)") {
		// Fill nametable 0 with tile 0xAA
		fill_nametable(0x2000, 0xAA);

		// Set scroll to 0,0
		set_scroll(0, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18); // Show background and sprites

		// Let the PPU process through pre-render to initialize scroll properly
		advance_to_scanline(261); // Pre-render scanline
		advance_to_cycle(280);	  // After vertical scroll copy

		// Now advance to visible scanline 0 of the NEXT frame
		advance_to_scanline(0);
		advance_to_cycle(2); // During first tile fetch (cycle 1-7)

		// Should be reading from nametable 0, column 0
		uint8_t tile_id = get_current_tile_id();
		INFO("Tile ID at cycle 2: " << static_cast<int>(tile_id));
		REQUIRE(tile_id == 0xAA);
	}

	SECTION("Scroll X = 128 (middle of nametable)") {
		// Fill nametable 0 with tile 0xBB
		fill_nametable(0x2000, 0xBB);

		// Set scroll to middle of screen (128 pixels = 16 tiles)
		set_scroll(128, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Initialize rendering (advances through pre-render)
		init_rendering();
		advance_to_cycle(2); // During first tile fetch

		// Should be reading from nametable 0, column 16 (scroll offset)
		uint8_t tile_id = get_current_tile_id();
		REQUIRE(tile_id == 0xBB);
	}

	SECTION("Scroll X = 255 (near right edge of nametable 0)") {
		// Fill nametable 0 with tile 0xCC
		fill_nametable(0x2000, 0xCC);
		// Also fill nametable 1 with 0xCC since scroll will wrap into it
		// (vertical mirroring means NT1 is independent from NT0)
		fill_nametable(0x2400, 0xCC);

		// Set scroll to near right edge (255 pixels = 31 tiles + 7 pixels)
		set_scroll(255, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Initialize rendering
		init_rendering();
		advance_to_cycle(2);

		// At scroll X=255, the viewport shows pixels 255-510
		// Pre-render fetched tiles at columns 31 (NT0) and 0 (NT1 after wrap)
		// By cycle 2 of scanline 0, we're fetching the 3rd tile (column 1 of NT1)
		// Since both nametables are filled with 0xCC, this should return 0xCC
		uint8_t tile_id = get_current_tile_id();
		REQUIRE(tile_id == 0xCC);
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Horizontal Scrolling - Nametable Wraparound",
				 "[ppu][viewport][scrolling][wraparound]") {
	SECTION("Scroll X = 0, should read from left nametable only") {
		// Fill nametable 0 (left) with 0xAA
		fill_nametable(0x2000, 0xAA);
		// Fill nametable 1 (right) with 0xBB
		fill_nametable(0x2400, 0xBB);

		// Reset nametable select after VRAM operations (real NES hardware behavior)
		write_ppu_register(0x2000, 0x00);

		// Set scroll to 0,0 (show left nametable)
		set_scroll(0, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Initialize rendering
		init_rendering();

		// Check first tile (column 0) - but vram_address has been incremented by
		// pre-render HBLANK fetches, so we're actually at column 2
		// This is BEFORE cycle 257 horizontal scroll reset
		advance_to_cycle(2);
		// At cycle 2, we're fetching tile for column 2 (due to prefetch offset)
		REQUIRE(get_current_tile_id() == 0xAA);

		// After cycle 257, horizontal scroll is reset
		advance_to_cycle(260);
		// Now we should be fetching from the correct scroll position
		// But we're at the end of the scanline, so check on next scanline
		advance_to_scanline(1);
		advance_to_cycle(2);
		// Now we should be at column 0 again (after scroll reset)
		REQUIRE(get_current_tile_id() == 0xAA);
	}

	SECTION("Scroll past nametable boundary - should wrap to right nametable") {
		// Fill nametable 0 (left, $2000) with 0xAA
		fill_nametable(0x2000, 0xAA);
		// Fill nametable 1 (right, $2400) with 0xBB
		fill_nametable(0x2400, 0xBB);

		// CRITICAL: Write PPUCTRL after VRAM operations to set correct base nametable
		write_ppu_register(0x2000, 0x00); // PPUCTRL: nametable 0

		// Scroll so that we're at tile 30 of nametable 0
		// This means first visible tile is column 30, and after 2 tiles we wrap to nametable 1
		set_scroll(240, 0); // 240 pixels = 30 tiles

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Initialize rendering
		init_rendering();

		// At cycle 2 of scanline 0, we're fetching tile at scroll position + 2 (due to prefetch)
		// scroll=30, so we're at column 32 which wraps to nametable 1, column 0
		// Actually: column 30 + 2 (prefetch) = 32, which wraps to 0 and toggles nametable
		advance_to_cycle(2);
		// Should be reading from nametable 1 now
		REQUIRE(get_current_tile_id() == 0xBB);

		// After scrolling through scanline and hitting cycle 257 reset,
		// check on next scanline that we're back at the correct position
		advance_to_scanline(1);
		advance_to_cycle(2);
		// Should still be at correct scroll position after reset
		REQUIRE(get_current_tile_id() == 0xBB);
	}

	SECTION("Left edge of viewport should NOT show right nametable (bug test)") {
		// This is the CRITICAL test for the bug the user reported!
		// When scrolling horizontally, the LEFT edge of the viewport should
		// NOT show tiles from the RIGHT nametable

		// Fill nametable 0 (left, $2000) with 0xAA everywhere
		fill_nametable(0x2000, 0xAA);
		// Fill nametable 1 (right, $2400) with 0xBB everywhere
		fill_nametable(0x2400, 0xBB);

		// CRITICAL: After PPUADDR/PPUDATA writes, must write PPUCTRL to set correct nametable
		// This is what real NES games do after updating VRAM during VBlank
		write_ppu_register(0x2000, 0x00); // PPUCTRL: select nametable 0

		// Set scroll to show left nametable
		set_scroll(0, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render to apply scroll
		init_rendering();

		// At cycle 2, we have prefetch offset but should still be in nametable 0
		// With scroll X=0, coarse_x after prefetch is 2, still in nametable 0
		advance_to_cycle(2);
		uint8_t tile_id = get_current_tile_id();
		REQUIRE(tile_id == 0xAA); // Must be from left nametable
		REQUIRE(tile_id != 0xBB); // NOT from right nametable
	}

	SECTION("Coarse X increment at tile 31 should toggle horizontal nametable") {
		// Fill both nametables with different patterns
		fill_nametable(0x2000, 0xAA); // Left nametable
		fill_nametable(0x2400, 0xBB); // Right nametable

		// CRITICAL: Write PPUCTRL after VRAM operations
		write_ppu_register(0x2000, 0x00); // PPUCTRL: nametable 0

		// Start at column 31 of left nametable (scroll X = 248)
		set_scroll(248, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render to apply scroll
		init_rendering();

		// After pre-render, vram_address coarse_x = 31 + 2 (prefetch) = 33
		// But coarse_x only has 5 bits (0-31), so 33 wraps to 1 and toggles horizontal nametable
		// So we're now in nametable 1, column 1
		advance_to_cycle(2);
		REQUIRE(get_current_tile_id() == 0xBB); // Should be in nametable 1 after wraparound

		// After 8 pixels (1 tile fetch), coarse_x increments from 1 to 2 (still in nametable 1)
		advance_to_cycle(10);					// Next tile fetch
		REQUIRE(get_current_tile_id() == 0xBB); // Still in nametable 1
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Horizontal Scrolling - Fine X Offset",
				 "[ppu][viewport][scrolling][fine_x]") {
	SECTION("Fine X scroll affects pixel position within tile") {
		// Fill nametable with known tile
		fill_nametable(0x2000, 0x01); // Tile 1 is solid pattern (0xFF)

		// Set scroll with fine X offset
		set_scroll(5, 0); // 5 pixels = 0 tiles + 5 pixel offset

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render to apply scroll
		init_rendering();
		advance_to_cycle(2);

		// Should still be reading tile 0, but with 5-pixel fine X offset
		auto debug_state = ppu->get_debug_state();
		REQUIRE(debug_state.fine_x_scroll == 5);
		REQUIRE(get_current_tile_id() == 0x01);
	}

	SECTION("Fine X wraps at 8 pixels") {
		// Set scroll to 15 pixels = 1 tile + 7 pixel offset
		set_scroll(15, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render to apply scroll
		init_rendering();
		advance_to_cycle(2);

		// Should be reading tile 1 with fine X = 7
		auto debug_state = ppu->get_debug_state();
		REQUIRE(debug_state.fine_x_scroll == 7);
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Horizontal Scrolling - Pre-render Scanline Setup",
				 "[ppu][viewport][scrolling][prerender]") {
	SECTION("Pre-render scanline prepares first two tiles for next frame") {
		// Fill nametables with patterns
		fill_nametable(0x2000, 0xAA);

		// Set scroll
		set_scroll(0, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance to pre-render scanline (261)
		advance_to_scanline(261);

		// During cycles 320-335, the PPU fetches the first two tiles for the next frame
		advance_to_cycle(330);

		// The pre-fetch should be loading tiles from nametable 0
		uint8_t tile_id = get_current_tile_id();
		REQUIRE(tile_id == 0xAA);
	}

	SECTION("Horizontal scroll reset at cycle 257 for each scanline") {
		// Fill nametables
		fill_nametable(0x2000, 0xAA);
		fill_nametable(0x2400, 0xBB);

		// Reset nametable select after VRAM operations (real NES hardware behavior)
		write_ppu_register(0x2000, 0x00);

		// Set initial scroll in middle of screen
		// scroll X = 128 means coarse_x = 16 (still in nametable 0)
		set_scroll(128, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render to apply scroll
		init_rendering();

		// After pre-render HBLANK, vram_address coarse_x = 16 + 2 (prefetch) = 18
		// Still in nametable 0 (only 32 columns per nametable)
		advance_to_cycle(2);
		uint8_t tile_before_257 = get_current_tile_id();
		REQUIRE(tile_before_257 == 0xAA); // coarse_x = 18, nametable 0

		// Advance to next scanline - cycle 257 will reset horizontal scroll
		advance_to_scanline(1);
		advance_to_cycle(2);

		// After cycle 257 reset, coarse_x should be back to 16 (original scroll)
		// But we also have the prefetch offset, so coarse_x = 16 + 2 = 18
		uint8_t tile_after_reset = get_current_tile_id();
		REQUIRE(tile_after_reset == 0xAA); // coarse_x = 18, nametable 0 (same as before)
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Nametable Mirroring - Vertical Mirroring", "[ppu][viewport][mirroring]") {
	SECTION("Vertical mirroring - left/right nametables distinct") {
		// With vertical mirroring (like Super Mario Bros.):
		// Nametable 0 ($2000) and Nametable 2 ($2800) are the same (left screen)
		// Nametable 1 ($2400) and Nametable 3 ($2C00) are the same (right screen)

		// Write to nametable 0
		write_vram(0x2000, 0xAA);
		// Write to nametable 1
		write_vram(0x2400, 0xBB);

		// Read from nametable 2 (should mirror nametable 0)
		uint8_t value_nt2 = read_vram(0x2800);
		REQUIRE(value_nt2 == 0xAA);

		// Read from nametable 3 (should mirror nametable 1)
		uint8_t value_nt3 = read_vram(0x2C00);
		REQUIRE(value_nt3 == 0xBB);
	}
}

TEST_CASE_METHOD(ViewportScrollingFixture, "Viewport Rendering - No Bleed Between Nametables",
				 "[ppu][viewport][rendering]") {
	SECTION("Rendering left nametable should not show right nametable pixels") {
		// Fill left and right nametables with distinct checkerboard patterns
		for (uint16_t i = 0; i < 960; ++i) {
			// Left nametable: even tiles = 0x00, odd tiles = 0x01
			write_vram(0x2000 + i, (i & 1) ? 0x01 : 0x00);
			// Right nametable: even tiles = 0x02, odd tiles = 0x03
			write_vram(0x2400 + i, (i & 1) ? 0x03 : 0x02);
		}

		// Reset nametable select after VRAM operations (real NES hardware behavior)
		write_ppu_register(0x2000, 0x00);

		// Set scroll to show only left nametable
		set_scroll(0, 0);

		// Enable rendering
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render
		init_rendering();

		// After pre-render, vram_address coarse_x = 0 + 2 (prefetch) = 2
		// Verify that tile fetching only reads from left nametable throughout scanline
		// Start at cycle 2 (first tile after prefetch) through visible area
		// Note: Stop at cycle 234 before PPU starts fetching from next nametable (tile 32)
		// The PPU fetches 2 tiles ahead, so by cycle 242 it's fetching tile 32 from NT1
		for (int cycle = 2; cycle < 234; cycle += 8) {
			advance_to_cycle(cycle);
			uint8_t tile_id = get_current_tile_id();
			// All tiles should be from left nametable (0x00 or 0x01)
			REQUIRE((tile_id == 0x00 || tile_id == 0x01));
			// NEVER from right nametable (0x02 or 0x03)
			REQUIRE(tile_id != 0x02);
			REQUIRE(tile_id != 0x03);
		}
	}

	SECTION("Scrolling creates smooth transition between nametables") {
		// Fill nametables with sequential tile IDs for easy tracking
		for (uint16_t i = 0; i < 960; ++i) {
			write_vram(0x2000 + i, static_cast<uint8_t>(i % 256));
			write_vram(0x2400 + i, static_cast<uint8_t>((i + 128) % 256));
		}

		// Reset nametable select after VRAM operations (real NES hardware behavior)
		write_ppu_register(0x2000, 0x00);

		// Start at scroll 0
		set_scroll(0, 0);
		write_ppu_register(0x2001, 0x18);

		// Advance through pre-render
		init_rendering();

		// After pre-render, vram_address coarse_x = 0 + 2 (prefetch) = 2
		// Tile at nametable 0 position 2 should be value 2
		advance_to_cycle(2);
		uint8_t first_tile = get_current_tile_id();
		REQUIRE(first_tile == 0x02); // Nametable 0, position 2 (due to prefetch)

		// Now scroll to the right nametable by setting PPUCTRL bit 0
		// This toggles horizontal nametable select
		write_ppu_register(0x2000, 0x00); // Reset control
		set_scroll(0, 0);				  // Scroll X = 0

		// Manually set nametable select by writing to PPUCTRL
		write_ppu_register(0x2000, 0x01); // Bit 0 = horizontal nametable select

		// Advance to next scanline to apply new scroll
		advance_to_scanline(1);
		advance_to_cycle(2);

		// Now we're in nametable 1, coarse_x = 0 + 2 (prefetch) = 2
		// Tile at nametable 1 position 2 = (2 + 128) % 256 = 130
		uint8_t right_nt_tile = get_current_tile_id();
		REQUIRE(right_nt_tile == 130); // Nametable 1, position 2 (offset pattern)
	}
}
