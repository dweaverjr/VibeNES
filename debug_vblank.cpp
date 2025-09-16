#include "core/bus.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include "ppu/ppu_memory.hpp"
#include <iostream>
#include <memory>

int main() {
	// Create minimal test setup
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	auto ppu_memory = std::make_shared<PPUMemory>();
	auto ppu = std::make_shared<PPU>();

	bus->connect_ram(ram);
	bus->connect_ppu(ppu);
	ppu->connect_memory(ppu_memory);

	// Check initial state immediately after construction
	std::cout << "=== Initial PPU State ===" << std::endl;
	std::cout << "Scanline: " << ppu->get_current_scanline() << std::endl;
	std::cout << "Cycle: " << ppu->get_current_cycle() << std::endl;

	// Read PPUSTATUS through bus (same as test)
	uint8_t status = bus->read(0x2002);
	std::cout << "PPUSTATUS value: " << static_cast<int>(status) << " (0x" << std::hex << static_cast<int>(status)
			  << ")" << std::endl;
	std::cout << "VBlank flag (bit 7): " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;

	return 0;
}
