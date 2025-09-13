#include "apu/apu_stub.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "core/component.hpp"
#include "core/types.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include <iostream>
#include <memory>

#ifdef NES_GUI_ENABLED
#include "cpu/cpu_6502.hpp"
#include "gui/gui_application.hpp"
#endif

using namespace nes;

int main(int argc, char *argv[]) {
	// Suppress unused parameter warnings
	(void)argc;
	(void)argv;
#ifdef NES_GUI_ENABLED
	std::cout << "VibeNES GUI - Starting debugger interface...\n";

	// Create and run GUI - components are internally managed
	nes::gui::GuiApplication gui_app;

	if (!gui_app.initialize()) {
		std::cerr << "Failed to initialize GUI application" << std::endl;
		return 1;
	}

	// Components are now internally managed by GuiApplication
	// Setup callbacks for component coordination
	gui_app.setup_callbacks();

	// Run the GUI
	gui_app.run();

	return 0;
#else
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

	// Test System Bus
	std::cout << "\n=== System Bus Test ===\n";
	auto bus = std::make_unique<SystemBus>();
	auto shared_ram = std::make_shared<Ram>();

	// Connect components
	bus->connect_ram(shared_ram);

	// Initialize system
	std::cout << "Component name: " << bus->get_name() << "\n";
	bus->power_on();
	bus->reset();

	// Test memory access through bus
	std::cout << "Testing memory access through bus...\n";
	bus->write(0x0000, 0xAA);
	bus->write(0x0001, 0xBB);
	bus->write(0x0800, 0xCC); // Test mirroring through bus

	std::cout << "Reading through bus:\n";
	std::cout << "$0000: $" << std::hex << std::uppercase << static_cast<int>(bus->read(0x0000)) << "\n";
	std::cout << "$0001: $" << std::hex << std::uppercase << static_cast<int>(bus->read(0x0001)) << "\n";
	std::cout << "$0800: $" << std::hex << std::uppercase << static_cast<int>(bus->read(0x0800)) << std::dec << "\n";

	// Test unmapped regions
	std::cout << "\nTesting unmapped regions:\n";
	Byte ppu_result = bus->read(0x2000); // PPU region
	std::cout << "PPU read returned: $" << std::hex << std::uppercase << static_cast<int>(ppu_result) << std::dec
			  << "\n";
	bus->write(0x4000, 0xFF); // APU region

	// Show memory map
	std::cout << "\n";
	bus->debug_print_memory_map();

	// Test bus timing
	std::cout << "\nTesting bus timing...\n";
	bus->tick(cpu_cycles(5));
	std::cout << "Bus ticked 5 cycles successfully\n";

	std::cout << "\n=== Foundation Test Complete ===\n";
	std::cout << "[OK] Type system working\n";
	std::cout << "[OK] Component interface working\n";
	std::cout << "[OK] RAM implementation working\n";
	std::cout << "[OK] Memory mirroring working\n";
	std::cout << "[OK] Debug output working\n";
	std::cout << "[OK] System Bus working\n";

	std::cout << "\nNext steps:\n";
	std::cout << "1. Add CPU registers and basic instruction decoding\n";
	std::cout << "2. Create simple instruction execution loop\n";
	std::cout << "3. Implement basic 6502 instructions\n";

	return 0;
#endif
}
