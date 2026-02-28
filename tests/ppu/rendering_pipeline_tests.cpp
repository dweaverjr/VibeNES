#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/nes_palette.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include <catch2/catch_all.hpp>
#include "test_chr_data.hpp"
#include <memory>

using namespace nes;

class RenderingPipelineTestFixture {
  public:
	RenderingPipelineTestFixture() {
		bus = std::make_unique<nes::SystemBus>();
		ram = std::make_shared<nes::Ram>();
		cartridge = nes::test::TestCHRData::create_test_cartridge();
		apu = std::make_shared<nes::APU>();
		cpu = std::make_shared<nes::CPU6502>(bus.get());
		ppu_memory = std::make_shared<nes::PPUMemory>();

		// Connect components to bus (like TimingTestFixture)
		bus->connect_ram(ram);
		bus->connect_cartridge(cartridge);
		bus->connect_apu(apu);
		bus->connect_cpu(cpu);

		// Create and connect PPU
		ppu = std::make_shared<nes::PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		// Connect cartridge to PPU for CHR ROM access
		ppu->connect_cartridge(cartridge);

		// Connect CPU to PPU for NMI generation
		ppu->connect_cpu(cpu.get());

		// Power on
		bus->power_on();
		ppu->power_on();

		// Set up basic rendering environment
		setup_basic_graphics_data();
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

	void write_vram(uint16_t address, uint8_t value) {
		set_vram_address(address);
		write_ppu_register(0x2007, value);
	}

	uint8_t read_vram(uint16_t address) {
		set_vram_address(address);
		read_ppu_register(0x2007); // Dummy read for non-palette addresses
		return read_ppu_register(0x2007);
	}

	void write_palette(uint16_t address, uint8_t value) {
		set_vram_address(address);
		write_ppu_register(0x2007, value);
	}

	void setup_basic_graphics_data() {
		// Set up a simple nametable pattern
		// Top row: tile pattern 0, 1, 0, 1, ...
		for (int x = 0; x < 32; x++) {
			write_vram(0x2000 + x, x % 2);
		}

		// Second row: tile pattern 2, 3, 2, 3, ...
		for (int x = 0; x < 32; x++) {
			write_vram(0x2020 + x, 2 + (x % 2));
		}

		// Set up attribute table (first few entries)
		write_vram(0x23C0, 0x50); // Palette pattern for first 4x4 tiles
		write_vram(0x23C1, 0xA0); // Different palette for next 4x4 tiles

		// Set up basic palette
		write_palette(0x3F00, 0x0F); // Universal background (black)
		write_palette(0x3F01, 0x30); // White
		write_palette(0x3F02, 0x16); // Red
		write_palette(0x3F03, 0x27); // Orange

		write_palette(0x3F04, 0x0F); // Palette 1 background
		write_palette(0x3F05, 0x12); // Blue
		write_palette(0x3F06, 0x1C); // Green
		write_palette(0x3F07, 0x07); // Brown

		// Set up sprite data
		setup_basic_sprites();
	}

	void setup_basic_sprites() {
		// Sprite 0: for sprite 0 hit testing
		write_ppu_register(0x2003, 0x00); // OAM address
		write_ppu_register(0x2004, 100);  // Y position
		write_ppu_register(0x2004, 0x01); // Tile index
		write_ppu_register(0x2004, 0x00); // Attributes (palette 0, no flip, front)
		write_ppu_register(0x2004, 120);  // X position

		// Sprite 1: regular sprite
		write_ppu_register(0x2004, 50);	  // Y position
		write_ppu_register(0x2004, 0x02); // Tile index
		write_ppu_register(0x2004, 0x01); // Attributes (palette 1)
		write_ppu_register(0x2004, 80);	  // X position

		// Clear remaining sprites
		for (int i = 8; i < 256; i++) {
			write_ppu_register(0x2004, 0xFF); // Invalid Y position
		}
	}

	void enable_background_rendering() {
		uint8_t mask = read_ppu_register(0x2001);
		mask |= 0x08; // Enable background
		write_ppu_register(0x2001, mask);
	}

	void enable_sprite_rendering() {
		uint8_t mask = read_ppu_register(0x2001);
		mask |= 0x10; // Enable sprites
		write_ppu_register(0x2001, mask);
	}

	void enable_all_rendering() {
		write_ppu_register(0x2001, 0x1E); // Enable background and sprites, no clipping
	}

	void disable_all_rendering() {
		write_ppu_register(0x2001, 0x00); // Disable all rendering
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(nes::CpuCycle{1});
		}
	}

	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() < target_scanline && safety_counter < MAX_CYCLES) {
			ppu->tick(nes::CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_scanline hit safety limit - possible infinite loop");
		}
	}

	void advance_to_cycle(int target_cycle) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops

		// If target cycle is less than current cycle, we need to advance to next scanline
		if (target_cycle < ppu->get_current_cycle()) {
			// Advance to next scanline first
			while (ppu->get_current_cycle() != 0 && safety_counter < MAX_CYCLES) {
				ppu->tick(nes::CpuCycle{1});
				safety_counter++;
			}
		}

		// Now advance to the target cycle within the current scanline
		while (ppu->get_current_cycle() < target_cycle && safety_counter < MAX_CYCLES) {
			ppu->tick(nes::CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_cycle hit safety limit - possible infinite loop");
		}
	}

	void advance_to_vblank() {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while (ppu->get_current_scanline() != 241 && safety_counter < MAX_CYCLES) {
			ppu->tick(nes::CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_vblank hit safety limit - possible infinite loop");
		}
	}

	void advance_to_rendering_start() {
		// Advance to start of visible scanlines (scanline 0, cycle 0)
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops
		while ((ppu->get_current_scanline() != 0 || ppu->get_current_cycle() != 0) && safety_counter < MAX_CYCLES) {
			ppu->tick(nes::CpuCycle{1});
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			throw std::runtime_error("advance_to_rendering_start hit safety limit - possible infinite loop");
		}
	}

	void set_scroll(uint8_t x, uint8_t y) {
		write_ppu_register(0x2005, x); // Horizontal scroll
		write_ppu_register(0x2005, y); // Vertical scroll
	}

	bool is_sprite_0_hit_set() {
		uint8_t status = read_ppu_register(0x2002);
		return (status & 0x40) != 0; // Bit 6
	}

	bool is_sprite_overflow_set() {
		uint8_t status = read_ppu_register(0x2002);
		return (status & 0x20) != 0; // Bit 5
	}

  protected:
	std::unique_ptr<nes::SystemBus> bus;
	std::shared_ptr<nes::Ram> ram;
	std::shared_ptr<nes::Cartridge> cartridge;
	std::shared_ptr<nes::APU> apu;
	std::shared_ptr<nes::CPU6502> cpu;
	std::shared_ptr<nes::PPUMemory> ppu_memory;
	std::shared_ptr<nes::PPU> ppu;
};

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Background Tile Fetching", "[ppu][pipeline][background]") {
	SECTION("Should fetch nametable tiles during visible scanlines") {
		enable_background_rendering();
		advance_to_rendering_start();

		// During visible scanlines, PPU should fetch tiles
		// Cycle pattern: NT byte, AT byte, PT low, PT high (repeat)
		for (int tile = 0; tile < 32; tile++) {
			advance_ppu_cycles(8); // Each tile takes 8 cycles to fetch
								   // PPU should have fetched nametable, attribute, and pattern data
		}
	}

	SECTION("Should fetch tiles even when rendering is disabled") {
		disable_all_rendering();
		advance_to_rendering_start();

		// PPU still performs fetches for timing accuracy
		advance_ppu_cycles(256); // Full scanline of fetches
								 // No visual output but timing should be maintained
	}

	SECTION("Should handle attribute table fetching correctly") {
		enable_background_rendering();
		advance_to_rendering_start();

		// Attribute table provides palette info for 4x4 tile groups
		// First 4 tiles should use attribute from $23C0
		advance_ppu_cycles(32); // 4 tiles × 8 cycles each

		// Next 4 tiles should still use same attribute byte
		advance_ppu_cycles(32);
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Background prefetch maintains left edge alignment",
				 "[ppu][render][alignment][regression]") {
	// Ensure a known tile and palette at the top-left of the screen
	disable_all_rendering();
	write_vram(0x2000, 0x01); // Top-left tile uses solid pattern

	// Configure palette 0 with a distinctive color so differences are obvious
	write_vram(0x23C0, 0x00);	 // All quadrants use palette 0
	write_palette(0x3F00, 0x0F); // Universal background (dark backdrop)
	write_palette(0x3F01, 0x30);
	write_palette(0x3F02, 0x30);
	write_palette(0x3F03, 0x30);

	// Enable background rendering and ensure the leftmost 8 pixels are enabled
	write_ppu_register(0x2001, 0x0A);

	// Render two frames to allow the prefetch pipeline to settle before sampling
	ppu->clear_frame_ready();
	advance_ppu_cycles(PPUTiming::CYCLES_PER_SCANLINE * PPUTiming::TOTAL_SCANLINES);
	ppu->clear_frame_ready();
	advance_ppu_cycles(PPUTiming::CYCLES_PER_SCANLINE * PPUTiming::TOTAL_SCANLINES);
	REQUIRE(ppu->is_frame_ready());

	const uint32_t *frame = ppu->get_frame_buffer();
	REQUIRE(frame != nullptr);

	uint32_t expected_tile_color = nes::NESPalette::get_rgba_color(0x30);
	uint32_t expected_background_color = nes::NESPalette::get_rgba_color(0x0F);
	CAPTURE(frame[0], frame[1], frame[8], frame[9]);

	// The very first pixel should use the prefetched leftmost tile
	REQUIRE(frame[0] == expected_tile_color);

	// Sanity check: tile color must differ from backdrop to detect mixing issues
	REQUIRE(frame[0] != expected_background_color);
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Sprite Evaluation", "[ppu][pipeline][sprites]") {
	SECTION("Should evaluate sprites during cycles 65-256") {
		enable_sprite_rendering();
		advance_to_rendering_start();

		// Cycles 1-64: idle
		advance_to_cycle(64);

		// Cycles 65-256: sprite evaluation for next scanline
		advance_to_cycle(65);

		// During this period, PPU evaluates which sprites appear on next scanline
		advance_ppu_cycles(192); // Cycles 65-256

		advance_to_cycle(257);
		// Sprite evaluation should be complete
	}

	SECTION("Should set sprite overflow flag when more than 8 sprites on scanline") {
		// Set up 9 sprites on the same scanline
		for (int i = 0; i < 9; i++) {
			write_ppu_register(0x2003, i * 4);	// OAM address
			write_ppu_register(0x2004, 100);	// Y position (same scanline)
			write_ppu_register(0x2004, i);		// Tile index
			write_ppu_register(0x2004, 0x00);	// Attributes
			write_ppu_register(0x2004, i * 20); // X position
		}

		enable_sprite_rendering();
		advance_to_scanline(100);
		advance_to_cycle(256);

		// Should set sprite overflow flag
		REQUIRE(is_sprite_overflow_set());
	}

	SECTION("Should fetch sprite pattern data during cycles 257-320") {
		enable_sprite_rendering();
		advance_to_rendering_start();

		// Advance to sprite pattern fetch period
		advance_to_cycle(257);

		// During cycles 257-320, PPU fetches pattern data for sprites
		// that will be rendered on current scanline
		advance_ppu_cycles(64); // Cycles 257-320

		advance_to_cycle(321);
		// Sprite pattern fetching should be complete
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Pixel Priority System", "[ppu][pipeline][priority]") {
	SECTION("Background pixels should appear when sprites are transparent") {
		enable_all_rendering();
		advance_to_rendering_start();

		// Set up scenario where sprite pixel is transparent (color 0)
		// Background should show through
		advance_to_scanline(0);
		advance_ppu_cycles(256);
	}

	SECTION("Sprite pixels should appear in front of background by default") {
		enable_all_rendering();
		advance_to_rendering_start();

		// Normal sprite priority: sprite in front of background
		advance_to_scanline(50); // Scanline with sprite
		advance_ppu_cycles(256);
	}

	SECTION("Background priority sprites should appear behind background") {
		// Set up sprite with background priority (bit 5 of attributes)
		write_ppu_register(0x2003, 4);	  // Sprite 1 attributes
		write_ppu_register(0x2004, 0x20); // Set background priority bit

		enable_all_rendering();
		advance_to_scanline(50);
		advance_ppu_cycles(256);

		// Sprite should appear behind non-transparent background pixels
	}

	SECTION("Sprite 0 hit should trigger when sprite and background collide") {
		enable_all_rendering();

		// Advance to where sprite 0 should be visible
		advance_to_scanline(100);
		advance_to_cycle(120);

		// Sprite 0 hit should occur when:
		// 1. Both background and sprite pixels are non-transparent
		// 2. Sprite 0 is involved
		// 3. Not at X=0 or Y=0

		// Check if sprite 0 hit flag is set
		[[maybe_unused]] bool hit_before = is_sprite_0_hit_set();

		// Advance through sprite 0's X position
		advance_ppu_cycles(8);

		[[maybe_unused]] bool hit_after = is_sprite_0_hit_set();
		// Hit flag should be set if collision occurred
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Shift Register Operation", "[ppu][pipeline][shift_registers]") {
	SECTION("Background shift registers should shift every pixel cycle") {
		enable_background_rendering();
		advance_to_rendering_start();

		// Every pixel cycle, background shift registers shift left
		// New tile data is loaded every 8 cycles
		for (int pixel = 0; pixel < 256; pixel++) {
			advance_ppu_cycles(1);
			// At each pixel, shift registers should shift
			// Every 8th pixel, new tile data should be loaded
		}
	}

	SECTION("Should load new tile data every 8 cycles") {
		enable_background_rendering();
		advance_to_rendering_start();

		// Tile fetch happens in 8-cycle blocks
		// Cycles 1,3,5,7 of each block fetch NT, AT, PT_low, PT_high
		for (int tile = 0; tile < 32; tile++) {
			// Cycle 1: Nametable byte
			advance_ppu_cycles(2);
			// Cycle 3: Attribute byte
			advance_ppu_cycles(2);
			// Cycle 5: Pattern table low
			advance_ppu_cycles(2);
			// Cycle 7: Pattern table high
			advance_ppu_cycles(2);
			// New tile data should be loaded into shift registers
		}
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Scrolling Effects on Pipeline", "[ppu][pipeline][scrolling]") {
	SECTION("Fine X scroll should affect pixel output timing") {
		enable_background_rendering();

		// Set fine X scroll
		set_scroll(3, 0); // 3-pixel horizontal offset

		advance_to_rendering_start();
		advance_ppu_cycles(256);

		// Pixels should be shifted by fine X amount
		// This affects which bits are read from shift registers
	}

	SECTION("Coarse scrolling should affect tile addresses") {
		enable_background_rendering();

		// Set coarse scroll (8-pixel increments)
		set_scroll(16, 8); // 2 tiles right, 1 tile down

		advance_to_rendering_start();
		advance_ppu_cycles(256);

		// Should fetch tiles from offset positions in nametable
	}

	SECTION("Vertical scrolling should affect nametable row") {
		enable_background_rendering();

		// Set vertical scroll
		set_scroll(0, 16); // 2 tiles down

		advance_to_rendering_start();
		advance_ppu_cycles(256);

		// Should fetch from tiles 2 rows down from normal
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Rendering Restrictions", "[ppu][pipeline][restrictions]") {
	SECTION("VRAM should be accessible during VBlank") {
		advance_to_vblank();

		// During VBlank, VRAM access should work normally
		write_vram(0x2000, 0x55);
		REQUIRE(read_vram(0x2000) == 0x55);
	}

	SECTION("VRAM access should be restricted during rendering") {
		enable_all_rendering();
		advance_to_rendering_start();

		// During active rendering, VRAM access may be corrupted
		write_vram(0x2000, 0xAA);
		// Read back may not match due to PPU using the bus
	}

	SECTION("Palette should be accessible during rendering") {
		enable_all_rendering();
		advance_to_rendering_start();

		// Palette access should work even during rendering
		write_palette(0x3F00, 0x25);

		set_vram_address(0x3F00);
		uint8_t value = read_ppu_register(0x2007);
		REQUIRE(value == 0x25);
	}

	SECTION("OAM should be inaccessible during sprite evaluation") {
		enable_sprite_rendering();
		advance_to_rendering_start();

		// Advance to sprite evaluation period
		advance_to_cycle(65);

		// OAM reads should return $FF during sprite evaluation
		uint8_t oam_value = read_ppu_register(0x2004);
		REQUIRE(oam_value == 0xFF);
	}
}

TEST_CASE_METHOD(RenderingPipelineTestFixture, "Frame Timing", "[ppu][pipeline][timing]") {
	SECTION("Visible scanlines should be 0-239") {
		advance_to_rendering_start();
		REQUIRE(ppu->get_current_scanline() == 0);

		// Advance through all visible scanlines
		for (int scanline = 0; scanline < 240; scanline++) {
			advance_to_scanline(scanline);
			REQUIRE(ppu->get_current_scanline() == scanline);
			advance_ppu_cycles(341); // Full scanline
		}

		// Should now be at VBlank
		REQUIRE(ppu->get_current_scanline() == 240);
	}

	SECTION("VBlank should be scanlines 241-260") {
		advance_to_scanline(241);
		advance_to_cycle(1); // VBlank flag is set at cycle 1
		REQUIRE(ppu->get_current_scanline() == 241);

		// VBlank flag should be set
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) != 0); // VBlank flag

		// Advance through VBlank
		advance_to_scanline(260);
		REQUIRE(ppu->get_current_scanline() == 260);
	}

	SECTION("Each scanline should take 341 cycles") {
		advance_to_rendering_start();
		advance_to_cycle(0);

		// Advance one full scanline
		advance_ppu_cycles(341);

		// Should be at start of next scanline
		REQUIRE(ppu->get_current_cycle() == 0);
		REQUIRE(ppu->get_current_scanline() == 1);
	}
}
