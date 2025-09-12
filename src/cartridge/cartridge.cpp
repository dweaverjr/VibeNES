#include "cartridge/cartridge.hpp"
#include "cartridge/mappers/mapper_000.hpp"
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
	// Load ROM data
	rom_data_ = RomLoader::load_rom(filepath);
	if (!rom_data_.valid) {
		std::cerr << "Failed to load ROM: " << filepath << std::endl;
		return false;
	}

	// Create appropriate mapper
	mapper_ = create_mapper(rom_data_);
	if (!mapper_) {
		std::cerr << "Unsupported mapper: " << static_cast<int>(rom_data_.mapper_id) << std::endl;
		rom_data_ = {}; // Clear invalid data
		return false;
	}

	std::cout << "Cartridge loaded successfully!" << std::endl;
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

std::unique_ptr<Mapper> Cartridge::create_mapper(const RomData &rom_data) {
	// Determine mirroring mode
	Mapper::Mirroring mirroring;
	if (rom_data.four_screen_vram) {
		mirroring = Mapper::Mirroring::FourScreen;
	} else if (rom_data.vertical_mirroring) {
		mirroring = Mapper::Mirroring::Vertical;
	} else {
		mirroring = Mapper::Mirroring::Horizontal;
	}

	// Create mapper based on ID
	switch (rom_data.mapper_id) {
	case 0:
		return std::make_unique<Mapper000>(rom_data.prg_rom, rom_data.chr_rom, mirroring);

	default:
		std::cerr << "Unsupported mapper ID: " << static_cast<int>(rom_data.mapper_id) << std::endl;
		return nullptr;
	}
}

} // namespace nes
