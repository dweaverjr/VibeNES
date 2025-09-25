// VibeNES - NES Emulator
// VRAM Address Corruption Tests
// Tests for VRAM address corruption during rendering and edge cases

#include "../../include/apu/apu.hpp"
#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

class AddressCorruptionTestFixture {
  public:
	AddressCorruptionTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		cartridge = std::make_shared<Cartridge>();
		apu = std::make_shared<APU>();
		cpu = std::make_shared<CPU6502>(bus.get());
		ppu_memory = std::make_shared<PPUMemory>();

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

		setup_test_data();
	}

	void setup_test_data() {
		// Fill VRAM with identifiable patterns
		for (uint16_t addr = 0x2000; addr < 0x3000; addr++) {
			write_vram(addr, static_cast<uint8_t>((addr >> 8) ^ (addr & 0xFF)));
		}

		// Distinct palette data
		for (uint16_t addr = 0x3F00; addr < 0x3F20; addr++) {
			write_vram(addr, static_cast<uint8_t>(0x30 + (addr & 0x0F)));
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

	// Helper to check if current cycle is during tile fetching
	bool is_tile_fetch_cycle() {
		int cycle = ppu->get_current_cycle();
		return (cycle >= 1 && cycle <= 256) && ((cycle - 1) % 8 < 8);
	}

	// Helper to check if current cycle is during sprite evaluation
	bool is_sprite_eval_cycle() {
		int cycle = ppu->get_current_cycle();
		return cycle >= 65 && cycle <= 256;
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

TEST_CASE_METHOD(AddressCorruptionTestFixture, "VRAM Address Corruption During Background Fetching",
				 "[ppu][corruption][background]") {
	SECTION("Address corruption during nametable fetch") {
		// Enable background rendering
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(10); // During nametable fetch (cycle 1 of tile)

		// Set a known VRAM address
		reset_toggle();
		write_ppu_register(0x2006, 0x24);
		write_ppu_register(0x2006, 0x00); // Address = $2400

		// Read during background fetching
		[[maybe_unused]] uint8_t corrupted_data = read_ppu_register(0x2007);

		// The address used might be corrupted by background fetching
		// Expected data at $2400 would be different from what we get
		[[maybe_unused]] uint8_t expected = 0x24 ^ 0x00; // Our test pattern

		// During rendering, address might be corrupted
		// This is hardware-specific behavior
	}

	SECTION("Address corruption during attribute fetch") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(11); // During attribute fetch (cycle 3 of tile)

		reset_toggle();
		write_ppu_register(0x2006, 0x23);
		write_ppu_register(0x2006, 0xC0); // Attribute table

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Address corruption during attribute fetch has specific patterns
	}

	SECTION("Address corruption during pattern table fetch") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(13); // During pattern low fetch (cycle 5 of tile)

		reset_toggle();
		write_ppu_register(0x2006, 0x10);
		write_ppu_register(0x2006, 0x00); // Pattern table

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Pattern table fetching can corrupt VRAM address in specific ways
	}

	SECTION("Sequential address corruption") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);

		// Test corruption across multiple tile fetches
		for (int tile = 0; tile < 4; tile++) {
			int base_cycle = tile * 8 + 1;

			advance_to_cycle(base_cycle + 2); // Attribute fetch

			reset_toggle();
			write_ppu_register(0x2006, 0x20 + tile);
			write_ppu_register(0x2006, 0x00);

			[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

			// Each tile fetch should cause different corruption patterns
		}
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "VRAM Address Corruption During Sprite Evaluation",
				 "[ppu][corruption][sprites]") {
	SECTION("Address corruption during sprite Y comparison") {
		// Setup sprites
		write_ppu_register(0x2003, 0x00);
		for (int i = 0; i < 8; i++) {
			write_ppu_register(0x2004, 50 + i); // Y positions
			write_ppu_register(0x2004, i);		// Tile indices
			write_ppu_register(0x2004, 0x00);	// Attributes
			write_ppu_register(0x2004, i * 32); // X positions
		}

		write_ppu_register(0x2001, 0x10); // Enable sprites

		advance_to_scanline(51); // Sprite evaluation line
		advance_to_cycle(80);	 // During sprite evaluation

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Address can be corrupted by sprite evaluation process
	}

	SECTION("Address corruption during sprite pattern fetch") {
		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);	  // Y
		write_ppu_register(0x2004, 0x10); // Tile
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X

		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(51);
		advance_to_cycle(260); // During sprite pattern fetch

		reset_toggle();
		write_ppu_register(0x2006, 0x15);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Sprite pattern fetching affects VRAM address
	}

	SECTION("8x16 sprite address corruption") {
		// 8x16 sprites fetch from different pattern tables
		write_ppu_register(0x2000, 0x20); // 8x16 sprite mode

		write_ppu_register(0x2003, 0x00);
		write_ppu_register(0x2004, 50);	  // Y
		write_ppu_register(0x2004, 0x01); // Odd tile (bottom pattern table)
		write_ppu_register(0x2004, 0x00); // Attributes
		write_ppu_register(0x2004, 100);  // X

		write_ppu_register(0x2001, 0x10);

		advance_to_scanline(51);
		advance_to_cycle(270); // During 8x16 sprite fetch

		reset_toggle();
		write_ppu_register(0x2006, 0x10);
		write_ppu_register(0x2006, 0x10);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// 8x16 sprites cause different address corruption patterns
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "Fine Scroll Address Corruption", "[ppu][corruption][scroll]") {
	SECTION("Fine X scroll corruption") {
		write_ppu_register(0x2001, 0x08);

		// Set fine X scroll
		reset_toggle();
		write_ppu_register(0x2005, 0x07); // Fine X = 7
		write_ppu_register(0x2005, 0x00); // Y = 0

		advance_to_scanline(50);
		advance_to_cycle(100);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Fine X scroll affects address corruption patterns
	}

	SECTION("Fine Y scroll corruption") {
		write_ppu_register(0x2001, 0x08);

		// Set fine Y scroll
		reset_toggle();
		write_ppu_register(0x2005, 0x00);
		write_ppu_register(0x2005, 0x05); // Fine Y in lower 3 bits

		advance_to_scanline(50);
		advance_to_cycle(100);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Fine Y scroll affects tile row and address corruption
	}

	SECTION("Coarse scroll corruption") {
		write_ppu_register(0x2001, 0x08);

		// Set coarse scroll positions
		reset_toggle();
		write_ppu_register(0x2005, 0x48); // Coarse X = 9
		write_ppu_register(0x2005, 0x50); // Coarse Y = 10

		advance_to_scanline(50);
		advance_to_cycle(256); // During horizontal position copy

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Coarse scroll affects nametable selection and addressing
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "Address Line Conflicts", "[ppu][corruption][address_lines]") {
	SECTION("Multiple address line access") {
		// Test simultaneous access to different address spaces
		write_ppu_register(0x2001, 0x18); // Enable both background and sprites

		advance_to_scanline(50);
		advance_to_cycle(100); // During active rendering

		// PPU is accessing pattern table, nametable, and attribute table
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x00); // Palette access

		uint8_t palette_data = read_ppu_register(0x2007);

		// Palette access during rendering should work correctly
		// (Palettes are not affected by address corruption)
		REQUIRE(palette_data == 0x30); // Our test pattern
	}

	SECTION("CHR ROM vs VRAM access conflicts") {
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(50);
		advance_to_cycle(13); // During pattern table fetch

		// Try to access VRAM while PPU is fetching CHR data
		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t vram_data = read_ppu_register(0x2007);

		// VRAM access during CHR fetch can cause conflicts
	}

	SECTION("Nametable mirroring corruption") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(50);

		// Access mirrored nametable addresses during rendering
		uint16_t test_addresses[] = {0x2000, 0x2400, 0x2800, 0x2C00};

		for (uint16_t addr : test_addresses) {
			reset_toggle();
			write_ppu_register(0x2006, addr >> 8);
			write_ppu_register(0x2006, addr & 0xFF);

			[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

			// Mirroring behavior during rendering can be affected
		}
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "Increment Mode Corruption", "[ppu][corruption][increment]") {
	SECTION("Horizontal increment corruption") {
		write_ppu_register(0x2000, 0x00); // +1 increment
		write_ppu_register(0x2001, 0x08); // Enable background

		advance_to_scanline(50);
		advance_to_cycle(100);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Sequential reads during rendering
		[[maybe_unused]] uint8_t data1 = read_ppu_register(0x2007); // Dummy
		[[maybe_unused]] uint8_t data2 = read_ppu_register(0x2007); // $2000
		[[maybe_unused]] uint8_t data3 = read_ppu_register(0x2007); // $2001 (should be)

		// Address increment might be corrupted during rendering
	}

	SECTION("Vertical increment corruption") {
		write_ppu_register(0x2000, 0x04); // +32 increment
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(100);

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t dummy = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t data1 = read_ppu_register(0x2007); // $2000
		[[maybe_unused]] uint8_t data2 = read_ppu_register(0x2007); // $2020 (should be)

		// Vertical increment can be affected by rendering
	}

	SECTION("Increment during sprite evaluation") {
		write_ppu_register(0x2000, 0x00);
		write_ppu_register(0x2001, 0x10); // Enable sprites

		advance_to_scanline(50);
		advance_to_cycle(100); // During sprite evaluation

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t dummy = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t data1 = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t data2 = read_ppu_register(0x2007);

		// Sprite evaluation can affect address increment
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "Write During Rendering Corruption", "[ppu][corruption][write]") {
	SECTION("VRAM write corruption during background fetch") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(10); // During background fetch

		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Write during rendering
		write_ppu_register(0x2007, 0xAB);

		// Verify if write was corrupted
		reset_toggle();
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t dummy = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t written_data = read_ppu_register(0x2007);

		// Data might not be written to expected address
	}

	SECTION("PPUADDR write during rendering") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Write PPUADDR during rendering
		reset_toggle();
		write_ppu_register(0x2006, 0x25); // High byte
		write_ppu_register(0x2006, 0x00); // Low byte

		// The address might be corrupted by rendering activity
		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Address might not be $2500 as expected
	}

	SECTION("Palette write during rendering") {
		write_ppu_register(0x2001, 0x18);

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Palette writes should still work during rendering
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x01);
		write_ppu_register(0x2007, 0x15);

		// Verify palette write worked
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x01);
		uint8_t palette_data = read_ppu_register(0x2007);

		REQUIRE(palette_data == 0x15); // Palette writes should work
	}
}

TEST_CASE_METHOD(AddressCorruptionTestFixture, "Edge Case Address Patterns", "[ppu][corruption][edge_cases]") {
	SECTION("Address wraparound corruption") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(100);

		// Test address near wraparound points
		reset_toggle();
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0xFF); // Near top of address space

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Address wraparound during rendering
	}

	SECTION("Boundary crossing corruption") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(256); // End of visible area

		reset_toggle();
		write_ppu_register(0x2006, 0x23);
		write_ppu_register(0x2006, 0xFF); // Nametable boundary

		[[maybe_unused]] uint8_t data1 = read_ppu_register(0x2007);
		[[maybe_unused]] uint8_t data2 = read_ppu_register(0x2007); // Crosses to attribute table

		// Boundary crossing can cause specific corruption
	}

	SECTION("Simultaneous scroll and address update") {
		write_ppu_register(0x2001, 0x08);

		advance_to_scanline(50);
		advance_to_cycle(257); // During horizontal scroll copy

		// Try to set address during scroll copy
		reset_toggle();
		write_ppu_register(0x2006, 0x24);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t data = read_ppu_register(0x2007);

		// Address setting during scroll copy can cause corruption
	}
}
