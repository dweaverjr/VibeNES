#include "core/bus.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu_6502.hpp"
#include "input/controller_stub.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include <iomanip>
#include <iostream>

namespace nes {

SystemBus::SystemBus()
	: ram_{nullptr}, ppu_{nullptr}, apu_{nullptr}, controllers_{nullptr}, cartridge_{nullptr}, cpu_{nullptr} {
}

void SystemBus::tick(CpuCycle cycles) {
	// Tick all connected components
	if (ram_) {
		ram_->tick(cycles);
	}
	if (ppu_) {
		ppu_->tick(cycles);
	}
	if (apu_) {
		apu_->tick(cycles);
	}
	if (controllers_) {
		controllers_->tick(cycles);
	}
	if (cartridge_) {
		cartridge_->tick(cycles);
	}
}

void SystemBus::reset() {
	// Reset all connected components
	if (ram_) {
		ram_->reset();
	}
	if (ppu_) {
		ppu_->reset();
	}
	if (apu_) {
		apu_->reset();
	}
	if (controllers_) {
		controllers_->reset();
	}
	if (cartridge_) {
		cartridge_->reset();
	}

	// Clear test memory
	test_high_memory_.fill(0x00);
	test_high_memory_valid_.fill(false);
	last_bus_value_ = 0xFF;
}

void SystemBus::power_on() {
	// Power on all connected components
	if (ram_) {
		ram_->power_on();
	}
	if (ppu_) {
		ppu_->power_on();
	}
	if (apu_) {
		apu_->power_on();
	}
	if (controllers_) {
		controllers_->power_on();
	}
	if (cartridge_) {
		cartridge_->power_on();
	}

	// Clear test memory
	test_high_memory_.fill(0x00);
	test_high_memory_valid_.fill(false);
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

	// PPU: $2000-$3FFF (includes register mirroring)
	if (is_ppu_address(address)) {
		if (ppu_) {
			last_bus_value_ = ppu_->read_register(address);
			return last_bus_value_;
		}
		return last_bus_value_; // Open bus
	}

	// APU/IO: $4000-$4015 (excluding $4017 which is controller 2 for reads)
	if (is_apu_read_address(address)) {
		if (apu_) {
			last_bus_value_ = apu_->read(address);
			return last_bus_value_;
		}
		return last_bus_value_; // Open bus
	}

	// Controllers: $4016 (controller 1), $4017 (controller 2 read)
	if (is_controller_address(address)) {
		if (controllers_) {
			last_bus_value_ = controllers_->read(address);
			return last_bus_value_;
		}
		return last_bus_value_; // Open bus
	}

	// Cartridge space: $4020-$FFFF (expansion, SRAM, PRG ROM)
	if (is_cartridge_address(address)) {
		if (cartridge_) {
			last_bus_value_ = cartridge_->cpu_read(address);
			return last_bus_value_;
		}
	}

	// High memory: $8000-$FFFF (test memory for ROM vectors)
	if (address >= 0x8000) {
		Address index = address - 0x8000;
		if (test_high_memory_valid_[index]) {
			last_bus_value_ = test_high_memory_[index];
			return last_bus_value_;
		}
		// Return open bus if no data written
		return last_bus_value_;
	}

	// Unmapped region - open bus behavior
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

	// PPU: $2000-$3FFF (includes register mirroring)
	if (is_ppu_address(address)) {
		if (ppu_) {
			ppu_->write_register(address, value);
		}
		return;
	}

	// APU/IO: $4000-$4015, $4017 (writes go to APU frame counter)
	if (is_apu_address(address)) {
		if (apu_) {
			apu_->write(address, value);
		}
		return;
	}

	// Controllers: $4016 (controller 1 strobe/data), $4017 handled by APU for writes
	if (is_controller_address(address)) {
		if (controllers_) {
			controllers_->write(address, value);
		}
		return;
	}

	// Cartridge space: $4020-$FFFF (expansion, SRAM, PRG ROM)
	if (is_cartridge_address(address)) {
		if (cartridge_) {
			cartridge_->cpu_write(address, value);
			return;
		}
	}

	// High memory: $8000-$FFFF (test memory for ROM vectors)
	if (address >= 0x8000) {
		Address index = address - 0x8000;
		test_high_memory_[index] = value;
		test_high_memory_valid_[index] = true;
		return;
	}

	// Unmapped region - ignore write
}

void SystemBus::connect_ram(std::shared_ptr<Ram> ram) {
	ram_ = std::move(ram);
}

void SystemBus::connect_ppu(std::shared_ptr<PPU> ppu) {
	ppu_ = std::move(ppu);
}

void SystemBus::connect_apu(std::shared_ptr<APU> apu) {
	apu_ = std::move(apu);
	// Connect CPU to APU for IRQ handling if both are available
	if (cpu_ && apu_) {
		apu_->connect_cpu(cpu_.get());
	}
}

void SystemBus::connect_controllers(std::shared_ptr<ControllerStub> controllers) {
	controllers_ = std::move(controllers);
}

void SystemBus::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
	cartridge_ = std::move(cartridge);
}

void SystemBus::connect_cpu(std::shared_ptr<CPU6502> cpu) {
	cpu_ = std::move(cpu);
	// Connect CPU to APU for IRQ handling if both are available
	if (cpu_ && apu_) {
		apu_->connect_cpu(cpu_.get());
	}
}

void SystemBus::debug_print_memory_map() const {
	std::cout << "=== System Bus Memory Map ===\n";
	std::cout << "$0000-$1FFF: RAM" << (ram_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$2000-$3FFF: PPU registers" << (ppu_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$4000-$401F: APU/IO registers" << (apu_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$4016-$4017: Controllers" << (controllers_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$4020-$5FFF: Expansion ROM" << (cartridge_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$6000-$7FFF: SRAM" << (cartridge_ ? " [connected]" : " [not connected]") << "\n";
	std::cout << "$8000-$FFFF: PRG ROM" << (cartridge_ ? " [connected]" : " [not connected]") << "\n";
}

bool SystemBus::is_ram_address(Address address) const noexcept {
	return address <= 0x1FFF; // RAM and its mirrors
}

bool SystemBus::is_ppu_address(Address address) const noexcept {
	return address >= 0x2000 && address <= 0x3FFF;
}

bool SystemBus::is_apu_address(Address address) const noexcept {
	return (address >= 0x4000 && address <= 0x4015) || address == 0x4017;
}

bool SystemBus::is_apu_read_address(Address address) const noexcept {
	return address >= 0x4000 && address <= 0x4015; // Exclude $4017 for reads
}

bool SystemBus::is_controller_address(Address address) const noexcept {
	return address == 0x4016 || address == 0x4017; // $4016=Controller1, $4017=Controller2(read)
}

bool SystemBus::is_cartridge_address(Address address) const noexcept {
	return address >= 0x4020; // Address is 16-bit, so it's always <= 0xFFFF
}

} // namespace nes
