#include "core/bus.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include <iostream>
#include <memory>

int main() {
	// Create minimal test setup
	auto bus = std::make_unique<nes::SystemBus>();
	auto ram = std::make_shared<nes::Ram>();
	auto ppu = std::make_shared<nes::PPU>();

	bus->connect_ram(ram);
	bus->connect_ppu(ppu);
	ppu->connect_bus(bus.get());
	ppu->power_on();

	std::cout << "=== Initial PPU State ===" << std::endl;
	std::cout << "Scanline: " << ppu->get_current_scanline() << std::endl;
	std::cout << "Cycle: " << ppu->get_current_cycle() << std::endl;

	// Tick PPU a few times to see if scanline advances
	std::cout << "\n=== After 10 ticks ===" << std::endl;
	for (int i = 0; i < 10; i++) {
		ppu->tick(nes::CpuCycle{1});
	}
	std::cout << "Scanline: " << ppu->get_current_scanline() << std::endl;
	std::cout << "Cycle: " << ppu->get_current_cycle() << std::endl;

	// Tick enough to advance to next scanline (341 cycles per scanline)
	std::cout << "\n=== After 341 more ticks (should advance scanline) ===" << std::endl;
	for (int i = 0; i < 341; i++) {
		ppu->tick(nes::CpuCycle{1});
	}
	std::cout << "Scanline: " << ppu->get_current_scanline() << std::endl;
	std::cout << "Cycle: " << ppu->get_current_cycle() << std::endl;

	return 0;
}
