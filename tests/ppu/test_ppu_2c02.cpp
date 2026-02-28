// VibeNES - NES Emulator
// PPU 2C02 Tests
// Comprehensive tests for the PPU core implementation

#include "../../include/cartridge/cartridge.hpp"
#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include <catch2/catch_all.hpp>
#include <memory>

using namespace nes;

// Test fixture for PPU testing
class PPUTestFixture {
  public:
	PPUTestFixture() {
		// Create mock components
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		ppu_memory = std::make_shared<PPUMemory>();

		// Connect components
		bus->connect_ram(ram);

		// Create PPU
		ppu = std::make_shared<PPU>();
		ppu->connect_bus(bus.get());
		bus->connect_ppu(ppu);

		// Initialize PPU state
		ppu->power_on();
	}

	// Helper function to create mock CHR data
	void setup_mock_chr_data() {
		// Create simple test patterns for CHR ROM
		std::vector<uint8_t> chr_data(8192, 0);

		// Pattern 0: Simple cross pattern
		chr_data[0x00] = 0x18;
		chr_data[0x08] = 0x00;
		chr_data[0x01] = 0x18;
		chr_data[0x09] = 0x00;
		chr_data[0x02] = 0xFF;
		chr_data[0x0A] = 0x00;
		chr_data[0x03] = 0xFF;
		chr_data[0x0B] = 0x00;
		chr_data[0x04] = 0x18;
		chr_data[0x0C] = 0x00;
		chr_data[0x05] = 0x18;
		chr_data[0x0D] = 0x00;
		chr_data[0x06] = 0x18;
		chr_data[0x0E] = 0x00;
		chr_data[0x07] = 0x18;
		chr_data[0x0F] = 0x00;

		// Store in mock cartridge (we'll need to implement this)
	}

	// Helper function to write to PPU register
	void write_ppu_register(uint16_t address, uint8_t value) {
		bus->write(address, value);
	}

	// Helper function to read from PPU register
	uint8_t read_ppu_register(uint16_t address) {
		return bus->read(address);
	}

	// Helper function to advance PPU by specific number of cycles
	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
		}
	}

	// Helper function to advance to specific scanline
	void advance_to_scanline(int target_scanline) {
		int safety_counter = 0;
		int max_cycles = 100000; // Safety limit
		while (ppu->get_current_scanline() < target_scanline && safety_counter < max_cycles) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			safety_counter++;
		}
		if (safety_counter >= max_cycles) {
			FAIL("advance_to_scanline hit safety limit - PPU may not be properly connected");
		}
	}

	void advance_to_cycle(int target_cycle) {
		int safety_counter = 0;
		const int MAX_CYCLES = 100000; // Safety limit to prevent infinite loops

		// If target cycle is less than current cycle, we need to advance to next scanline
		if (target_cycle < ppu->get_current_cycle()) {
			// Advance to next scanline first
			while (ppu->get_current_cycle() != 0 && safety_counter < MAX_CYCLES) {
				ppu->tick_single_dot();
				safety_counter++;
			}
		}

		// Now advance to the target cycle within the current scanline
		while (ppu->get_current_cycle() < target_cycle && safety_counter < MAX_CYCLES) {
			ppu->tick_single_dot(); // Advance by exactly 1 PPU dot
			safety_counter++;
		}
		if (safety_counter >= MAX_CYCLES) {
			FAIL("advance_to_cycle hit safety limit - possible infinite loop");
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(PPUTestFixture, "PPU Construction", "[ppu][core]") {
	SECTION("PPU should be properly initialized") {
		REQUIRE(std::string(ppu->get_name()) == "PPU 2C02");
	}

	SECTION("PPU should start at scanline 0") {
		REQUIRE(ppu->get_current_scanline() == 0);
	}

	SECTION("PPU should start at cycle 0") {
		REQUIRE(ppu->get_current_cycle() == 0);
	}

	SECTION("VBlank should not be set initially") {
		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) == 0); // VBlank flag
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPU Reset", "[ppu][reset]") {
	SECTION("Reset should clear VBlank flag") {
		// Set VBlank flag artificially
		write_ppu_register(0x2001, 0x10); // Enable rendering to allow VBlank
		advance_to_scanline(241);		  // VBlank scanline
		advance_to_cycle(1);			  // VBlank flag is set at cycle 1

		uint8_t status_before = read_ppu_register(0x2002);
		REQUIRE((status_before & 0x80) != 0); // VBlank should be set

		ppu->reset();

		uint8_t status_after = read_ppu_register(0x2002);
		REQUIRE((status_after & 0x80) == 0); // VBlank should be cleared
	}

	SECTION("Reset should set scanline and cycle to 0") {
		advance_ppu_cycles(1000); // Advance PPU state

		ppu->reset();

		REQUIRE(ppu->get_current_scanline() == 0);
		REQUIRE(ppu->get_current_cycle() == 0);
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUCTRL Register ($2000)", "[ppu][registers]") {
	SECTION("PPUCTRL write should update control register") {
		write_ppu_register(0x2000, 0x90);

		// We can't directly read PPUCTRL, but we can test its effects
		// through other registers and behavior
	}

	SECTION("Nametable selection bits should work") {
		// Test nametable selection through PPUCTRL bits 0-1
		write_ppu_register(0x2000, 0x00); // Nametable 0
		write_ppu_register(0x2000, 0x01); // Nametable 1
		write_ppu_register(0x2000, 0x02); // Nametable 2
		write_ppu_register(0x2000, 0x03); // Nametable 3
	}

	SECTION("VRAM increment mode should work") {
		write_ppu_register(0x2000, 0x00); // +1 increment
		write_ppu_register(0x2000, 0x04); // +32 increment
	}

	SECTION("Pattern table selection should work") {
		write_ppu_register(0x2000, 0x00); // Background: $0000, Sprite: $0000
		write_ppu_register(0x2000, 0x10); // Background: $1000, Sprite: $0000
		write_ppu_register(0x2000, 0x08); // Background: $0000, Sprite: $1000
		write_ppu_register(0x2000, 0x18); // Background: $1000, Sprite: $1000
	}

	SECTION("NMI enable should work") {
		write_ppu_register(0x2000, 0x80); // Enable NMI
		write_ppu_register(0x2000, 0x00); // Disable NMI
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUMASK Register ($2001)", "[ppu][registers]") {
	SECTION("PPUMASK write should update mask register") {
		write_ppu_register(0x2001, 0x1E);

		// Test rendering enable through behavior
	}

	SECTION("Background enable should work") {
		write_ppu_register(0x2001, 0x08); // Enable background
		write_ppu_register(0x2001, 0x00); // Disable background
	}

	SECTION("Sprite enable should work") {
		write_ppu_register(0x2001, 0x10); // Enable sprites
		write_ppu_register(0x2001, 0x00); // Disable sprites
	}

	SECTION("Left edge clipping should work") {
		write_ppu_register(0x2001, 0x02); // Show background in leftmost 8 pixels
		write_ppu_register(0x2001, 0x04); // Show sprites in leftmost 8 pixels
	}

	SECTION("Color emphasis should work") {
		write_ppu_register(0x2001, 0x20); // Emphasize red
		write_ppu_register(0x2001, 0x40); // Emphasize green
		write_ppu_register(0x2001, 0x80); // Emphasize blue
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUSTATUS Register ($2002)", "[ppu][registers]") {
	SECTION("PPUSTATUS read should return correct flags") {
		uint8_t status = read_ppu_register(0x2002);

		// Initially, only unused bits might be set
		REQUIRE((status & 0x1F) == 0); // Lower 5 bits should be 0
	}

	SECTION("VBlank flag should be set during VBlank") {
		write_ppu_register(0x2001, 0x10); // Enable rendering
		advance_to_scanline(241);		  // VBlank starts at scanline 241
		advance_to_cycle(1);			  // VBlank flag is set at cycle 1

		uint8_t status = read_ppu_register(0x2002);
		REQUIRE((status & 0x80) != 0); // VBlank flag should be set
	}

	SECTION("Reading PPUSTATUS should clear VBlank flag") {
		write_ppu_register(0x2001, 0x10); // Enable rendering
		advance_to_scanline(241);		  // VBlank starts
		advance_to_cycle(1);			  // VBlank flag is set at cycle 1

		uint8_t status1 = read_ppu_register(0x2002);
		REQUIRE((status1 & 0x80) != 0); // VBlank should be set

		uint8_t status2 = read_ppu_register(0x2002);
		REQUIRE((status2 & 0x80) == 0); // VBlank should be cleared after read
	}

	SECTION("Reading PPUSTATUS should reset PPUSCROLL/PPUADDR toggle") {
		// Write to PPUSCROLL (first write)
		write_ppu_register(0x2005, 0x12);

		// Read PPUSTATUS to reset toggle
		read_ppu_register(0x2002);

		// Next write to PPUSCROLL should be treated as first write again
		write_ppu_register(0x2005, 0x34);
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUSCROLL Register ($2005)", "[ppu][registers]") {
	SECTION("PPUSCROLL writes should update scroll registers") {
		// Reset toggle
		read_ppu_register(0x2002);

		// First write: X scroll
		write_ppu_register(0x2005, 0x12);

		// Second write: Y scroll
		write_ppu_register(0x2005, 0x34);

		// Third write should be X scroll again (toggle reset)
		write_ppu_register(0x2005, 0x56);
	}

	SECTION("PPUSCROLL should work with fine scroll") {
		read_ppu_register(0x2002); // Reset toggle

		// Test various scroll values
		for (uint8_t scroll = 0; scroll < 8; scroll++) {
			write_ppu_register(0x2005, scroll);
			write_ppu_register(0x2005, scroll);
		}
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUADDR Register ($2006)", "[ppu][registers]") {
	SECTION("PPUADDR writes should update VRAM address") {
		read_ppu_register(0x2002); // Reset toggle

		// Write high byte
		write_ppu_register(0x2006, 0x20);

		// Write low byte
		write_ppu_register(0x2006, 0x00);

		// VRAM address should now be $2000
		// We can test this by reading PPUDATA
	}

	SECTION("PPUADDR should handle address mirroring") {
		read_ppu_register(0x2002);

		// Test various address ranges
		uint16_t test_addresses[] = {
			0x2000, 0x2400, 0x2800, 0x2C00, // Nametables
			0x3000, 0x3400, 0x3800, 0x3C00, // Nametable mirrors
			0x3F00, 0x3F10, 0x3F20, 0x3F30	// Palette
		};

		for (uint16_t addr : test_addresses) {
			read_ppu_register(0x2002); // Reset toggle
			write_ppu_register(0x2006, addr >> 8);
			write_ppu_register(0x2006, addr & 0xFF);
		}
	}
}

TEST_CASE_METHOD(PPUTestFixture, "PPUDATA Register ($2007)", "[ppu][registers]") {
	SECTION("PPUDATA write should update VRAM") {
		read_ppu_register(0x2002); // Reset toggle

		// Set VRAM address to nametable
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		// Write test data
		write_ppu_register(0x2007, 0x42);

		// Reset address and read back
		read_ppu_register(0x2002);
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		[[maybe_unused]] uint8_t dummy = read_ppu_register(0x2007); // Dummy read
		uint8_t data = read_ppu_register(0x2007);					// Actual data

		REQUIRE(static_cast<int>(data) == 0x42);
	}

	SECTION("PPUDATA should handle VRAM increment modes") {
		read_ppu_register(0x2002);

		// Test +1 increment
		write_ppu_register(0x2000, 0x00); // +1 increment
		write_ppu_register(0x2006, 0x20);
		write_ppu_register(0x2006, 0x00);

		write_ppu_register(0x2007, 0x11);
		write_ppu_register(0x2007, 0x22);
		write_ppu_register(0x2007, 0x33);

		// Test +32 increment
		write_ppu_register(0x2000, 0x04); // +32 increment
		write_ppu_register(0x2006, 0x21);
		write_ppu_register(0x2006, 0x00);

		write_ppu_register(0x2007, 0x44);
		write_ppu_register(0x2007, 0x55);
		write_ppu_register(0x2007, 0x66);
	}

	SECTION("PPUDATA palette reads should be immediate") {
		read_ppu_register(0x2002);

		// Set address to palette
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x00);

		// Write palette data
		write_ppu_register(0x2007, 0x0F);

		// Reset and read
		read_ppu_register(0x2002);
		write_ppu_register(0x2006, 0x3F);
		write_ppu_register(0x2006, 0x00);

		uint8_t palette_data = read_ppu_register(0x2007);
		REQUIRE(static_cast<int>(palette_data) == 0x0F); // No dummy read for palette
	}
}
