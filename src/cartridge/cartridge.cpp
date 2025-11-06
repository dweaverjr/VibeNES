#include "cartridge/cartridge.hpp"
#include "cartridge/mapper_factory.hpp"
#include <iostream>

namespace nes {

void Cartridge::tick(CpuCycle cycles) {
	// Most mappers don't need tick behavior
	// Some advanced mappers might need timing (MMC5, etc.)
	(void)cycles;
}

void Cartridge::reset() {
	if (mapper_) {
		mapper_->reset();
	}
}

void Cartridge::power_on() {
	if (mapper_) {
		mapper_->reset(); // Same as reset for most mappers
	}
}

const char *Cartridge::get_name() const noexcept {
	return "Cartridge";
}

bool Cartridge::load_rom(const std::string &filepath) {
	// Load ROM data using RomLoader
	rom_data_ = RomLoader::load_rom(filepath);
	if (!rom_data_.valid) {
		std::cerr << "Failed to load ROM: " << filepath << std::endl;
		return false;
	}

	// Create appropriate mapper using MapperFactory
	mapper_ = MapperFactory::create_mapper(rom_data_);
	if (!mapper_) {
		std::cerr << "Unsupported mapper: " << static_cast<int>(rom_data_.mapper_id) << std::endl;
		rom_data_ = {}; // Clear invalid data
		return false;
	}

	std::cout << "Cartridge loaded successfully!" << std::endl;
	std::cout << "ROM: " << filepath << std::endl;
	std::cout << "Mapper: " << static_cast<int>(rom_data_.mapper_id) << " (" << mapper_->get_name() << ")" << std::endl;
	std::cout << "PRG ROM: " << rom_data_.prg_rom_pages << " x 16KB" << std::endl;
	std::cout << "CHR ROM: " << rom_data_.chr_rom_pages << " x 8KB" << std::endl;
	return true;
}

bool Cartridge::load_from_rom_data(const RomData &rom_data) {
	// Validate ROM data
	if (!rom_data.valid) {
		std::cerr << "Invalid ROM data provided" << std::endl;
		return false;
	}

	// Store ROM data
	rom_data_ = rom_data;

	// Create appropriate mapper using MapperFactory
	mapper_ = MapperFactory::create_mapper(rom_data_);
	if (!mapper_) {
		std::cerr << "Unsupported mapper: " << static_cast<int>(rom_data_.mapper_id) << std::endl;
		rom_data_ = {}; // Clear invalid data
		return false;
	}

	return true;
}

void Cartridge::unload_rom() {
	mapper_.reset();
	rom_data_ = {};
}

Byte Cartridge::cpu_read(Address address) const {
	if (!mapper_) {
		return 0xFF; // No ROM loaded
	}
	return mapper_->cpu_read(address);
}

void Cartridge::cpu_write(Address address, Byte value) {
	if (!mapper_) {
		return; // No ROM loaded
	}
	mapper_->cpu_write(address, value);
}

Byte Cartridge::ppu_read(Address address) const {
	if (!mapper_) {
		return 0xFF; // No ROM loaded
	}
	return mapper_->ppu_read(address);
}

void Cartridge::ppu_write(Address address, Byte value) {
	if (!mapper_) {
		return; // No ROM loaded
	}
	mapper_->ppu_write(address, value);
}

std::uint8_t Cartridge::get_mapper_id() const noexcept {
	if (!mapper_) {
		return 0xFF; // Invalid
	}
	return mapper_->get_mapper_id();
}

const char *Cartridge::get_mapper_name() const noexcept {
	if (!mapper_) {
		return "None";
	}
	return mapper_->get_name();
}

Mapper::Mirroring Cartridge::get_mirroring() const noexcept {
	if (!mapper_) {
		return Mapper::Mirroring::Horizontal;
	}
	return mapper_->get_mirroring();
}

void Cartridge::ppu_a12_toggle() const {
	if (mapper_) {
		mapper_->ppu_a12_toggle();
	}
}

bool Cartridge::is_irq_pending() const {
	if (!mapper_) {
		return false;
	}
	return mapper_->is_irq_pending();
}

void Cartridge::clear_irq() const {
	if (mapper_) {
		mapper_->clear_irq();
	}
}

// Save state serialization
void Cartridge::serialize_state(std::vector<uint8_t> &buffer) const {
	// Serialize mapper state (includes PRG RAM, CHR RAM, and mapper registers)
	if (mapper_) {
		mapper_->serialize_state(buffer);
	}
}

void Cartridge::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// Deserialize mapper state
	if (mapper_) {
		mapper_->deserialize_state(buffer, offset);
	}
}

} // namespace nes
