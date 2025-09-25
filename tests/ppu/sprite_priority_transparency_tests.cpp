// VibeNES - NES Emulator
// PPU Sprite Priority and Transparency Tests

#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

class SpritePriorityFixture {
  public:
	SpritePriorityFixture() {
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

	void write_sprite(uint8_t index, uint8_t y, uint8_t tile, uint8_t attributes, uint8_t x) {
		uint8_t oam_address = index * 4;
		write_ppu_register(0x2003, oam_address);
		write_ppu_register(0x2004, y);
		write_ppu_register(0x2004, tile);
		write_ppu_register(0x2004, attributes);
		write_ppu_register(0x2004, x);
	}

	void advance_to_scanline(int target_scanline) {
		int safety = 0;
		const int MAX = 200000;
		while (ppu->get_current_scanline() < target_scanline && safety < MAX) {
			ppu->tick_single_dot();
			safety++;
		}
		REQUIRE(safety < MAX);
	}

	void advance_to_cycle(int target_cycle) {
		int safety = 0;
		const int MAX = 200000;
		int initial_scanline = ppu->get_current_scanline();
		while (ppu->get_current_cycle() < target_cycle && ppu->get_current_scanline() == initial_scanline &&
			   safety < MAX) {
			ppu->tick_single_dot();
			safety++;
		}
		REQUIRE(safety < MAX);
	}

	void enable_background_and_sprites(uint8_t mask = 0x18) {
		write_ppu_register(0x2001, mask);
	}

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(SpritePriorityFixture, "Sprite vs background transparency/priority", "[ppu][sprites][priority]") {
	// Setup: background enabled, sprites enabled
	enable_background_and_sprites();

	// Place sprite 0 at Y=60, X=100, tile 1, normal priority (in front)
	write_sprite(0, 60, 0x01, 0x00, 100);

	// Advance to where it would render
	advance_to_scanline(60);
	advance_to_cycle(128);

	// Read status to ensure no false sprite 0 hit when background pixel is transparent
	uint8_t status1 = read_ppu_register(0x2002);
	(void)status1; // Not asserting specific value; just exercise path
}

TEST_CASE_METHOD(SpritePriorityFixture, "Sprite behind background when priority set", "[ppu][sprites][priority]") {
	// Bit 5 set => behind background
	enable_background_and_sprites();
	write_sprite(0, 60, 0x01, 0x20, 100);
	advance_to_scanline(60);
	advance_to_cycle(200);

	// Nothing to assert numerically without framebuffer tap; ensure no crashes and status readable
	uint8_t status = read_ppu_register(0x2002);
	(void)status;
}
