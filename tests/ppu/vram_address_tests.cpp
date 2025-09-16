// VibeNES - NES Emulator
// PPU VRAM Address Tests
// Tests for VRAM address register behavior and scrolling

#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

// Test fixture for VRAM address testing
class VRAMAddressTestFixture {
  public:
	VRAMAddressTestFixture() {
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

	void set_vram_address(uint16_t address) {
		reset_toggle();
		write_ppu_register(0x2006, address >> 8);
		write_ppu_register(0x2006, address & 0xFF);
	}

	void advance_ppu_cycles(int cycles) {
		for (int i = 0; i < cycles; i++) {
			ppu->tick(CpuCycle{1});
		}
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(VRAMAddressTestFixture, "VRAM Address Increment", "[ppu][vram][address]") {
	SECTION("VRAM address should increment by 1 in horizontal mode") {
		write_ppu_register(0x2000, 0x00); // Horizontal increment (+1)
		set_vram_address(0x2000);

		// Write sequence and check address increments
		write_ppu_register(0x2007, 0x11);
		write_ppu_register(0x2007, 0x22);
		write_ppu_register(0x2007, 0x33);

		// Read back to verify addresses were incremented correctly
		set_vram_address(0x2000);
		uint8_t dummy = read_ppu_register(0x2007); // Dummy read
		uint8_t data1 = read_ppu_register(0x2007); // $2000
		uint8_t data2 = read_ppu_register(0x2007); // $2001
		uint8_t data3 = read_ppu_register(0x2007); // $2002

		REQUIRE(data1 == 0x11);
		REQUIRE(data2 == 0x22);
		REQUIRE(data3 == 0x33);
	}

	SECTION("VRAM address should increment by 32 in vertical mode") {
		write_ppu_register(0x2000, 0x04); // Vertical increment (+32)
		set_vram_address(0x2000);

		write_ppu_register(0x2007, 0xAA);
		write_ppu_register(0x2007, 0xBB);
		write_ppu_register(0x2007, 0xCC);

		// Check data at incremented addresses
		set_vram_address(0x2000); // $2000
		uint8_t dummy1 = read_ppu_register(0x2007);
		uint8_t data1 = read_ppu_register(0x2007);

		set_vram_address(0x2020); // $2000 + 32
		uint8_t dummy2 = read_ppu_register(0x2007);
		uint8_t data2 = read_ppu_register(0x2007);

		set_vram_address(0x2040); // $2000 + 64
		uint8_t dummy3 = read_ppu_register(0x2007);
		uint8_t data3 = read_ppu_register(0x2007);

		REQUIRE(data1 == 0xAA);
		REQUIRE(data2 == 0xBB);
		REQUIRE(data3 == 0xCC);
	}
}

TEST_CASE_METHOD(VRAMAddressTestFixture, "VRAM Address Wrapping", "[ppu][vram][wrapping]") {
	SECTION("VRAM address should wrap at $4000") {
		write_ppu_register(0x2000, 0x00); // +1 increment
		set_vram_address(0x3FFF);

		write_ppu_register(0x2007, 0x42);

		// Address should wrap to $0000
		set_vram_address(0x0000);
		uint8_t dummy = read_ppu_register(0x2007);
		uint8_t data = read_ppu_register(0x2007);

		REQUIRE(data == 0x42);
	}

	SECTION("Nametable addresses should mirror correctly") {
		// Nametables mirror every $1000 bytes in the $2000-$2FFF range
		uint16_t test_data = 0x55;

		// Write to base nametable
		set_vram_address(0x2000);
		write_ppu_register(0x2007, test_data);

		// Check mirrors
		for (uint16_t mirror = 0x3000; mirror <= 0x3F00; mirror += 0x1000) {
			if (mirror < 0x3F00) { // Don't test palette area
				set_vram_address(mirror);
				uint8_t dummy = read_ppu_register(0x2007);
				uint8_t data = read_ppu_register(0x2007);

				INFO("Testing mirror at address: 0x" << std::hex << mirror);
				REQUIRE(data == test_data);
			}
		}
	}
}

TEST_CASE_METHOD(VRAMAddressTestFixture, "Scroll Register Interaction", "[ppu][vram][scroll]") {
	SECTION("PPUSCROLL should affect VRAM address") {
		reset_toggle();

		// Set horizontal scroll
		write_ppu_register(0x2005, 8); // X scroll = 8 pixels (1 tile)
		write_ppu_register(0x2005, 0); // Y scroll = 0

		// The scroll values should be stored in temp VRAM address
		// This is difficult to test directly without internal access
	}

	SECTION("PPUADDR and PPUSCROLL should share toggle") {
		reset_toggle();

		// Write to PPUSCROLL (should set toggle)
		write_ppu_register(0x2005, 0x10);

		// Write to PPUADDR (should use the toggle state)
		write_ppu_register(0x2006, 0x20); // This should be treated as low byte due to toggle

		// The behavior here depends on internal implementation
	}
}

TEST_CASE_METHOD(VRAMAddressTestFixture, "Fine Scroll Behavior", "[ppu][vram][fine_scroll]") {
	SECTION("Fine X scroll should be extracted correctly") {
		reset_toggle();

		// Test various fine X values (0-7)
		for (uint8_t fine_x = 0; fine_x < 8; fine_x++) {
			write_ppu_register(0x2005, fine_x);
			write_ppu_register(0x2005, 0);

			// Fine X affects pixel-level scrolling within tiles
			// This would need to be tested through rendering output
		}
	}

	SECTION("Coarse scroll should affect nametable addressing") {
		reset_toggle();

		// Coarse X scroll (tile-level)
		for (uint8_t coarse_x = 0; coarse_x < 32; coarse_x += 8) {
			write_ppu_register(0x2005, coarse_x * 8); // Convert to pixel scroll
			write_ppu_register(0x2005, 0);

			// This affects which tile is fetched from nametable
		}
	}
}

TEST_CASE_METHOD(VRAMAddressTestFixture, "VRAM Address During Rendering", "[ppu][vram][rendering]") {
	SECTION("VRAM address should be updated during rendering") {
		// Enable rendering
		write_ppu_register(0x2001, 0x18); // Enable background and sprites

		// Set scroll position
		reset_toggle();
		write_ppu_register(0x2005, 0);
		write_ppu_register(0x2005, 0);

		// During rendering, the PPU automatically updates VRAM address
		// This is complex to test without cycle-accurate simulation
	}

	SECTION("Horizontal scroll should reset during rendering") {
		write_ppu_register(0x2001, 0x18); // Enable rendering

		// Set initial scroll
		reset_toggle();
		write_ppu_register(0x2005, 64); // Some horizontal scroll
		write_ppu_register(0x2005, 0);

		// During visible scanlines, horizontal position is reset from temp address
		// This happens at cycle 257 of each scanline
	}
}

TEST_CASE_METHOD(VRAMAddressTestFixture, "Address Calculation Edge Cases", "[ppu][vram][edge_cases]") {
	SECTION("High addresses should be masked") {
		// PPU only has 14-bit address space
		set_vram_address(0x7FFF); // Should be masked to $3FFF

		write_ppu_register(0x2007, 0x99);

		// Verify it was written to the masked address
		set_vram_address(0x3FFF);
		uint8_t dummy = read_ppu_register(0x2007);
		uint8_t data = read_ppu_register(0x2007);

		REQUIRE(data == 0x99);
	}

	SECTION("Palette addresses should behave correctly") {
		// Palette addresses $3F20-$3FFF mirror $3F00-$3F1F
		set_vram_address(0x3F00);
		write_ppu_register(0x2007, 0x0F);

		set_vram_address(0x3F20); // Mirror of $3F00
		uint8_t palette_data = read_ppu_register(0x2007);

		REQUIRE(palette_data == 0x0F);
	}

	SECTION("Backdrop color mirrors should work") {
		// Addresses $3F10, $3F14, $3F18, $3F1C mirror $3F00
		set_vram_address(0x3F00);
		write_ppu_register(0x2007, 0x30); // Universal backdrop color

		uint16_t mirror_addresses[] = {0x3F10, 0x3F14, 0x3F18, 0x3F1C};

		for (uint16_t addr : mirror_addresses) {
			set_vram_address(addr);
			uint8_t data = read_ppu_register(0x2007);

			INFO("Testing backdrop mirror at: 0x" << std::hex << addr);
			REQUIRE(data == 0x30);
		}
	}
}
