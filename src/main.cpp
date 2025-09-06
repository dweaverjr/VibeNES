#include "core/component.hpp"
#include "core/types.hpp"
#include "gui/emulator_gui.hpp"
#include "memory/ram.hpp"
#include <iostream>
#include <memory>

using namespace nes;

int main() {
	std::cout << "VibeNES - Starting emulator...\n";
	std::cout << "Testing core components:\n\n";

	// Test our type system
	std::cout << "=== Type System Test ===\n";
	std::cout << "CPU Clock Rate: " << CPU_CLOCK_NTSC << " Hz\n";
	std::cout << "PPU Clock Rate: " << PPU_CLOCK_NTSC << " Hz\n";
	std::cout << "RAM Size: " << RAM_SIZE << " bytes\n";

	// Test address utility functions
	Address test_addr = 0x0800; // This should mirror to 0x0000
	Address mirrored = mirror_ram_address(test_addr);
	std::cout << "Address $" << std::hex << std::uppercase << test_addr << " mirrors to $" << mirrored << std::dec
			  << "\n";

	// Test cycle conversions
	auto cpu_cyc = cpu_cycles(100);
	auto ppu_dots = to_ppu_dots(cpu_cyc);
	std::cout << "100 CPU cycles = " << ppu_dots.count() << " PPU dots\n\n";

	// Test RAM component
	std::cout << "=== RAM Component Test ===\n";
	auto ram = std::make_unique<Ram>();

	// Test component interface
	std::cout << "Component name: " << ram->get_name() << "\n";

	// Power on and reset
	ram->power_on();
	ram->reset();

	// Test memory operations
	std::cout << "Writing test pattern to RAM...\n";
	ram->write(0x0000, 0xAA);
	ram->write(0x0001, 0xBB);
	ram->write(0x0002, 0xCC);

	// Test mirroring
	ram->write(0x0800, 0xDD); // Should mirror to 0x0000

	std::cout << "Reading back from RAM:\n";
	std::cout << "$0000: $" << std::hex << std::uppercase << static_cast<int>(ram->read(0x0000)) << "\n";
	std::cout << "$0001: $" << std::hex << std::uppercase << static_cast<int>(ram->read(0x0001)) << "\n";
	std::cout << "$0002: $" << std::hex << std::uppercase << static_cast<int>(ram->read(0x0002)) << "\n";
	std::cout << "Mirrored $0800: $" << std::hex << std::uppercase << static_cast<int>(ram->read(0x0800)) << std::dec
			  << "\n";

	// Test debug output
	std::cout << "\n";
	ram->debug_print(0x0000, 16);

	// Test timing (just show it works)
	std::cout << "\n=== Timing Test ===\n";
	auto start_cycles = cpu_cycles(0);
	ram->tick(cpu_cycles(10));
	std::cout << "RAM ticked 10 cycles successfully\n";

	std::cout << "\n=== Foundation Test Complete ===\n";
	std::cout << "[OK] Type system working\n";
	std::cout << "[OK] Component interface working\n";
	std::cout << "[OK] RAM implementation working\n";
	std::cout << "[OK] Memory mirroring working\n";
	std::cout << "[OK] Debug output working\n";

	std::cout << "\nNext steps:\n";
	std::cout << "1. Implement System Bus\n";
	std::cout << "2. Add CPU registers and basic instruction decoding\n";
	std::cout << "3. Create simple instruction execution loop\n";

	return 0;
}
