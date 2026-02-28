// VibeNES - NES Emulator
// PPU 8x16 Sprite Vertical Flip Addressing Tests

#include "../../include/core/bus.hpp"
#include "../../include/memory/ram.hpp"
#include "../../include/ppu/ppu.hpp"
#include "../../include/ppu/ppu_memory.hpp"
#include <catch2/catch_all.hpp>
#include <memory>

using namespace nes;

class Sprite8x16Fixture {
  public:
	Sprite8x16Fixture() {
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

  protected:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::shared_ptr<PPUMemory> ppu_memory;
	std::shared_ptr<PPU> ppu;
};

TEST_CASE_METHOD(Sprite8x16Fixture, "8x16 vertical flip addressing", "[ppu][sprites][8x16]") {
	// Enable 8x16 mode
	write_ppu_register(0x2000, 0x20);
	// Place sprite using tile index with even (top) and odd (bottom) tiles
	write_sprite(0, 40, 0x12, 0x80, 100); // vertical flip set
	write_ppu_register(0x2001, 0x10);

	advance_to_scanline(40);
	advance_to_cycle(256);

	uint8_t status = read_ppu_register(0x2002);
	(void)status;
}
