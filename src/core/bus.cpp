#include "core/bus.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "cpu/cpu_6502.hpp"
#include "input/controller.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include <iomanip>
#include <iostream>

namespace nes {

SystemBus::SystemBus()
	: ram_{nullptr}, ppu_{nullptr}, apu_{nullptr}, controllers_{nullptr}, cartridge_{nullptr}, cpu_{nullptr},
	  audio_backend_{std::make_unique<AudioBackend>()} {
}

void SystemBus::tick(CpuCycle cycles) {
	// Tick all connected components
	if (ram_) {
		ram_->tick(cycles);
	}
	if (ppu_) {
		// PPU runs at 3× the CPU clock; convert CPU cycles to PPU dot units
		ppu_->tick(cpu_cycles(cycles.count() * 3));
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

	// Check for mapper IRQs (MMC3, etc.)
	if (cartridge_ && cpu_ && cartridge_->is_irq_pending()) {
		cpu_->trigger_irq();
		cartridge_->clear_irq();
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
	// BUT: Check test memory first for addresses $8000+ when cartridge has no ROM
	if (is_cartridge_address(address)) {
		// If a cartridge is loaded, always defer to mapper-provided memory first
		if (cartridge_ && cartridge_->is_loaded()) {
			last_bus_value_ = cartridge_->cpu_read(address);
			return last_bus_value_;
		}

		// Otherwise fall back to high-memory mirrors for unit tests without a cartridge
		if (address >= 0x8000) {
			Address index = address - 0x8000;
			if (test_high_memory_valid_[index]) {
				last_bus_value_ = test_high_memory_[index];
				return last_bus_value_;
			}
		}

		// If a cartridge object exists, allow it to supply open-bus semantics (0xFF when unloaded)
		if (cartridge_) {
			last_bus_value_ = cartridge_->cpu_read(address);
			return last_bus_value_;
		}

		// No cartridge data available and no mirror value—return open bus (last_bus_value_)
		return last_bus_value_;
	}

	// Unmapped region - open bus behavior
	return last_bus_value_;
}

Byte SystemBus::peek(Address address) const {
	// Non-intrusive memory peek for debugging - no side effects

	// RAM: $0000-$1FFF (includes mirroring)
	if (is_ram_address(address)) {
		if (ram_) {
			return ram_->read(address); // RAM reads have no side effects
		}
	}

	// PPU: $2000-$3FFF (includes register mirroring)
	if (is_ppu_address(address)) {
		// For PPU registers, return cached/approximate values to avoid side effects
		// $2002 (PPUSTATUS) - return status without clearing VBlank flag
		// $2007 (PPUDATA) - return last read buffer value without advancing address
		// Other registers are write-only, return open bus
		if (ppu_) {
			return ppu_->peek_register(address); // Need to implement this
		}
		return last_bus_value_; // Open bus
	}

	// APU/IO: $4000-$4015 (excluding $4017 which is controller 2 for reads)
	if (is_apu_read_address(address)) {
		// APU reads may have side effects, return open bus for safety
		return last_bus_value_;
	}

	// Controllers: $4016 (controller 1), $4017 (controller 2 read)
	if (is_controller_address(address)) {
		// Controller reads have side effects, return open bus for safety
		return last_bus_value_;
	}

	// Cartridge space: $4020-$FFFF (expansion, SRAM, PRG ROM)
	if (is_cartridge_address(address)) {
		if (cartridge_) {
			return cartridge_->cpu_read(address); // ROM reads typically have no side effects
		}
	}

	// High memory: $8000-$FFFF (test memory for ROM vectors)
	if (address >= 0x8000) {
		Address index = address - 0x8000;
		if (test_high_memory_valid_[index]) {
			return test_high_memory_[index];
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

	// OAM DMA: $4014 (sprite DMA transfer) - Check before APU!
	if (address == 0x4014) {
		perform_oam_dma(value);
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
			// Controller write only uses the strobe bit (bit 0) from value, address doesn't matter
			controllers_->write(value);
		}
		return;
	}

	// Cartridge space: $4020-$FFFF (expansion, SRAM, PRG ROM)
	// BUT: For test purposes, prioritize test memory for $8000+ addresses
	if (is_cartridge_address(address)) {
		// Writes go to the active cartridge when a ROM is loaded (handles PRG RAM / mapper regs)
		if (cartridge_ && cartridge_->is_loaded()) {
			cartridge_->cpu_write(address, value);
			return;
		}

		// When no cartridge is loaded, retain the high-memory mirror for tests at $8000-$FFFF
		if (address >= 0x8000) {
			Address index = address - 0x8000;
			test_high_memory_[index] = value;
			test_high_memory_valid_[index] = true;
			return;
		}

		// Otherwise nothing is mapped—ignore write
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
	// Connect audio backend to APU
	if (apu_ && audio_backend_) {
		apu_->connect_audio_backend(audio_backend_.get());
	}
	// Connect bus to APU for DMC memory access
	if (apu_) {
		apu_->connect_bus(this);
	}
}

void SystemBus::connect_controllers(std::shared_ptr<Controller> controllers) {
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

void SystemBus::perform_oam_dma(Byte page) {
	// OAM DMA transfers 256 bytes from page $XX00-$XXFF to PPU OAM
	// Takes 513 cycles (or 514 if on odd CPU cycle)
	// Hardware-accurate: CPU is halted and PPU performs cycle-by-cycle transfer

	if (!ppu_) {
		return;
	}

	// Start hardware-accurate OAM DMA in the PPU
	// This will halt the CPU for 513-514 cycles while PPU handles the transfer
	ppu_->write_oam_dma(page);
}

bool SystemBus::is_dma_active() const noexcept {
	return ppu_ && ppu_->is_oam_dma_active();
}

// Audio control implementation
bool SystemBus::initialize_audio(int sample_rate, int buffer_size) {
	if (!audio_backend_) {
		return false;
	}

	bool success = audio_backend_->initialize(sample_rate, buffer_size);

	// Update APU's sample rate converter with actual SDL sample rate
	// (SDL may change it due to SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)
	if (success && apu_) {
		int actual_sample_rate = audio_backend_->get_sample_rate();
		std::cout << "SystemBus: Configuring APU for " << actual_sample_rate << " Hz output" << std::endl;
		std::cout << "  APU input rate: " << CPU_CLOCK_NTSC << " Hz (CPU clock)" << std::endl;
		std::cout << "  Sample rate conversion ratio: " << (static_cast<float>(CPU_CLOCK_NTSC) / actual_sample_rate)
				  << ":1" << std::endl;
		apu_->set_output_sample_rate(static_cast<float>(actual_sample_rate));
	}

	return success;
}

void SystemBus::start_audio() {
	if (audio_backend_) {
		audio_backend_->start();
	}
	if (apu_) {
		apu_->enable_audio(true);
	}
}

void SystemBus::stop_audio() {
	if (audio_backend_) {
		audio_backend_->stop();
	}
	if (apu_) {
		apu_->enable_audio(false);
	}
}

void SystemBus::set_audio_volume(float volume) {
	if (audio_backend_) {
		audio_backend_->set_volume(volume);
	}
}

float SystemBus::get_audio_volume() const {
	return audio_backend_ ? audio_backend_->get_volume() : 0.0f;
}

bool SystemBus::is_audio_playing() const {
	return audio_backend_ ? audio_backend_->is_playing() : false;
}

} // namespace nes
