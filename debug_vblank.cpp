#include "include/ppu/ppu.hpp" #include "include/ppu/ppu.hpp" #include "core/bus.hpp"

#include "include/core/bus.hpp"

#include "include/memory/ram.hpp" #include "include/core/bus.hpp" #include "memory/ram.hpp"

#include "include/cartridge/cartridge.hpp"

#include "include/apu/apu.hpp" #include "include/memory/ram.hpp" #include "ppu/ppu.hpp"

#include "include/cpu/cpu_6502.hpp"

#include <iostream>#include "include/cartridge/cartridge.hpp"#include "ppu/ppu_memory.hpp"

#include <memory>

#include "include/apu/apu.hpp" #include < iostream>

using namespace nes;

#include "include/cpu/cpu_6502.hpp" #include < memory>

int main() {

	auto bus = std::make_unique<SystemBus>();
#include <iostream>

    auto ram = std::make_shared<Ram>();

	auto cartridge = std::make_shared<Cartridge>();
#include <memory>using namespace nes;

	auto apu = std::make_shared<APU>();

	auto cpu = std::make_shared<CPU6502>(bus.get());

	// Connect components to bususing namespace nes;int main() {

	bus->connect_ram(ram);

	bus->connect_cartridge(cartridge); // Create minimal test setup

	bus->connect_apu(apu);

	bus->connect_cpu(cpu);
	int main() {
		auto bus = std::make_unique<SystemBus>();

		// Create and connect PPU    auto bus = std::make_unique<SystemBus>();	auto ram = std::make_shared<Ram>();

		auto ppu = std::make_shared<PPU>();

		ppu->connect_bus(bus.get());
		auto ram = std::make_shared<Ram>();
		auto ppu_memory = std::make_shared<PPUMemory>();

		bus->connect_ppu(ppu);

		ppu->connect_cartridge(cartridge);
		auto cartridge = std::make_shared<Cartridge>();

		ppu->connect_cpu(cpu.get());

		auto apu = std::make_shared<APU>();
		bus->connect_ram(ram);

		// Power on the system

		bus->power_on();
		auto cpu = std::make_shared<CPU6502>(bus.get());

		ppu->power_on();

		// Create PPU

		std::cout << "Initial state: scanline=" << ppu->get_current_scanline()

				  << " cycle=" << ppu->get_current_cycle()
				  << std::endl; // Connect components to bus	auto ppu = std::make_shared<PPU>();

		uint8_t status = bus->read(0x2002);
		bus->connect_ram(ram);
		ppu->connect_bus(bus.get());

		std::cout << "Initial VBlank flag: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;

		bus->connect_cartridge(cartridge);
		bus->connect_ppu(ppu);

		// Advance to scanline 241

		while (ppu->get_current_scanline() < 241) {
			bus->connect_apu(apu);

			ppu->tick_single_dot();
		}
		bus->connect_cpu(cpu); // Initialize PPU state

		std::cout << "After advance to scanline 241: scanline=" << ppu->get_current_scanline() ppu->power_on();

		<< " cycle=" << ppu->get_current_cycle() << std::endl;

		// Create and connect PPU

		status = bus->read(0x2002);

		std::cout << "VBlank flag after reaching scanline 241: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;
		auto ppu = std::make_shared<PPU>(); // Check initial state immediately after construction

		// Advance to cycle 0 if needed    ppu->connect_bus(bus.get());	std::cout << "=== Initial PPU State ===" <<
		// std::endl;

		while (ppu->get_current_cycle() != 0) {

			ppu->tick_single_dot();
			bus->connect_ppu(ppu);
			std::cout << "Scanline: " << ppu->get_current_scanline() << std::endl;

			if (ppu->get_current_scanline() != 241) {

				std::cout << "ERROR: Scanline changed while trying to reach cycle 0!" << std::endl;
				ppu->connect_cartridge(cartridge);
				std::cout << "Cycle: " << ppu->get_current_cycle() << std::endl;

				break;
			}
			ppu->connect_cpu(cpu.get());
		}

		// Read PPUSTATUS through bus (same as test)

		std::cout << "After advance to cycle 0: scanline=" << ppu->get_current_scanline()

				  << " cycle=" << ppu->get_current_cycle()
				  << std::endl; // Power on the system	uint8_t status = bus->read(0x2002);

		status = bus->read(0x2002);
		bus->power_on();
		std::cout << "PPUSTATUS value: " << static_cast<int>(status) << " (0x" << std::hex
				  << static_cast<int>(status)

						 std::cout
				  << "VBlank flag at scanline 241, cycle 0: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;

		ppu->power_on();
		<< ")" << std::endl;

		// Advance to cycle 1

		ppu->tick_single_dot();
		std::cout << "VBlank flag (bit 7): " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;

		std::cout << "After advance to cycle 1: scanline=" << ppu->get_current_scanline() std::cout
				  << "Initial state: scanline=" << ppu->get_current_scanline()

				  << " cycle=" << ppu->get_current_cycle() << std::endl;

		<< " cycle=" << ppu->get_current_cycle() << std::endl; // Now try to advance to scanline 241 like the test does

		status = bus->read(0x2002);

		std::cout << "VBlank flag at scanline 241, cycle 1: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;
		std::cout << "\n=== Advancing to Scanline 241 ===" << std::endl;

		return 0;
		uint8_t status = bus->read(0x2002);
		int safety_counter = 0;
	}
	std::cout << "Initial VBlank flag: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;
	int max_cycles = 100000;

	while (ppu->get_current_scanline() < 241 && safety_counter < max_cycles) {

		// Advance to scanline 241		ppu->tick(CpuCycle{1});

		while (ppu->get_current_scanline() < 241) {
			safety_counter++;

			ppu->tick_single_dot();

		} // Print every 10000 cycles

		if (safety_counter % 10000 == 0) {

			std::cout << "After advance to scanline 241: scanline=" << ppu->get_current_scanline() std::cout
					  << "Cycles: " << safety_counter << ", Scanline: " << ppu->get_current_scanline()

					  << " cycle=" << ppu->get_current_cycle() << std::endl;
			<< ", Cycle: " << ppu->get_current_cycle() << std::endl;
		}

		status = bus->read(0x2002);
	}

	std::cout << "VBlank flag after reaching scanline 241: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;

	if (safety_counter >= max_cycles) {

		// Advance to cycle 0 if needed		std::cout << "Hit safety limit! PPU may not be advancing properly." <<
		// std::endl;

		while (ppu->get_current_cycle() != 0) {
			return 1;

			ppu->tick_single_dot();
		}

		if (ppu->get_current_scanline() != 241) {

			std::cout << "ERROR: Scanline changed while trying to reach cycle 0!" << std::endl;
			std::cout << "Reached scanline 241 after " << safety_counter << " PPU cycles" << std::endl;

			break;
			std::cout << "Current state - Scanline: " << ppu->get_current_scanline()
					  << ", Cycle: " << ppu->get_current_cycle()
		}
		<< std::endl;
	}

	// Check VBlank flag at scanline 241, cycle 0

	std::cout << "After advance to cycle 0: scanline=" << ppu->get_current_scanline() uint8_t status_before =
		bus->read(0x2002);

	<< " cycle=" << ppu->get_current_cycle() << std::endl;
	std::cout << "PPUSTATUS at scanline 241, cycle 0: 0x" << std::hex << static_cast<int>(status_before)

			  << " (VBlank: " << ((status_before & 0x80) ? "SET" : "CLEAR") << ")" << std::endl;

	status = bus->read(0x2002);

	std::cout << "VBlank flag at scanline 241, cycle 0: " << ((status & 0x80) ? "SET" : "CLEAR")
			  << std::endl; // Advance to cycle 1 (when VBlank should be set)

	ppu->tick(CpuCycle{1});

	// Advance to cycle 1	std::cout << "Advanced to cycle " << ppu->get_current_cycle() << std::endl;

	ppu->tick_single_dot();

	uint8_t status_after = bus->read(0x2002);

	std::cout << "After advance to cycle 1: scanline=" << ppu->get_current_scanline() std::cout
			  << "PPUSTATUS at scanline 241, cycle 1: 0x" << std::hex << static_cast<int>(status_after)

			  << " cycle=" << ppu->get_current_cycle() << std::endl;
	<< " (VBlank: " << ((status_after & 0x80) ? "SET" : "CLEAR") << ")" << std::endl;

	status = bus->read(0x2002);
	return 0;

	std::cout << "VBlank flag at scanline 241, cycle 1: " << ((status & 0x80) ? "SET" : "CLEAR") << std::endl;
}

return 0;
}
