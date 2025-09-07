#include "core/bus.hpp"
#include "memory/ram.hpp"
#include <iomanip>
#include <iostream>

namespace nes {

SystemBus::SystemBus() : ram_{nullptr} {
}

void SystemBus::tick(CpuCycle cycles) {
	// Tick all connected components
	if (ram_) {
		ram_->tick(cycles);
	}
	// TODO: Tick other components when available
	(void)cycles; // Suppress unused parameter warning until more components added
}

void SystemBus::reset() {
	// Reset all connected components
	if (ram_) {
		ram_->reset();
	}
	last_bus_value_ = 0xFF;
}

void SystemBus::power_on() {
	// Power on all connected components
	if (ram_) {
		ram_->power_on();
	}
	last_bus_value_ = 0xFF;
}

const char *SystemBus::get_name() const noexcept {
	return "System Bus";
}

Byte SystemBus::read(Address address) const {
	// RAM: $0000-$1FFF (includes mirroring)
	if (is_ram_address(address)) {
		if (ram_) {
			last_bus_value_ = ram_->read(address);
			return last_bus_value_;
		}
	}

	// PPU: $2000-$3FFF (TODO: implement when PPU ready)
	if (is_ppu_address(address)) {
		std::cout << "PPU read at $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << address
				  << " - not implemented yet\n"
				  << std::dec;
		return last_bus_value_; // Open bus
	}

	// APU/IO: $4000-$401F (TODO: implement when APU ready)
	if (is_apu_address(address)) {
		std::cout << "APU/IO read at $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << address
				  << " - not implemented yet\n"
				  << std::dec;
		return last_bus_value_; // Open bus
	}

	// Unmapped region - open bus behavior
	std::cout << "Open bus read at $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << address
			  << std::dec << "\n";
	return last_bus_value_;
}

void SystemBus::write(Address address, Byte value) {
	last_bus_value_ = value; // Bus remembers last written value

	// RAM: $0000-$1FFF (includes mirroring)
	if (is_ram_address(address)) {
		if (ram_) {
			ram_->write(address, value);
			return;
		}
	}

	// PPU: $2000-$3FFF (TODO: implement when PPU ready)
	if (is_ppu_address(address)) {
		std::cout << "PPU write $" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
				  << static_cast<int>(value) << " to $" << std::setfill('0') << std::setw(4) << address
				  << " - not implemented yet\n"
				  << std::dec;
		return;
	}

	// APU/IO: $4000-$401F (TODO: implement when APU ready)
	if (is_apu_address(address)) {
		std::cout << "APU/IO write $" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
				  << static_cast<int>(value) << " to $" << std::setfill('0') << std::setw(4) << address
				  << " - not implemented yet\n"
				  << std::dec;
		return;
	}

	// Unmapped region - ignore write
	std::cout << "Open bus write $" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
			  << static_cast<int>(value) << " to $" << std::setfill('0') << std::setw(4) << address << std::dec
			  << " (ignored)\n";
}

void SystemBus::connect_ram(std::shared_ptr<Ram> ram) {
	ram_ = std::move(ram);
	std::cout << "[OK] RAM connected to system bus\n";
}

void SystemBus::debug_print_memory_map() const {
	std::cout << "=== System Bus Memory Map ===\n";
	std::cout << "$0000-$1FFF: RAM" << (ram_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$2000-$3FFF: PPU registers [not implemented]\n";
	std::cout << "$4000-$401F: APU/IO registers [not implemented]\n";
	std::cout << "$4020-$5FFF: Expansion ROM [not implemented]\n";
	std::cout << "$6000-$7FFF: SRAM [not implemented]\n";
	std::cout << "$8000-$FFFF: PRG ROM [not implemented]\n";
}

bool SystemBus::is_ram_address(Address address) const noexcept {
	return address <= 0x1FFF; // RAM and its mirrors
}

bool SystemBus::is_ppu_address(Address address) const noexcept {
	return address >= 0x2000 && address <= 0x3FFF;
}

bool SystemBus::is_apu_address(Address address) const noexcept {
	return address >= 0x4000 && address <= 0x401F;
}

} // namespace nes
