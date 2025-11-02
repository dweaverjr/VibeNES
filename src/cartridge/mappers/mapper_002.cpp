#include "cartridge/mappers/mapper_002.hpp"

namespace nes {

Mapper002::Mapper002(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring)
	: prg_rom_(std::move(prg_rom)), mirroring_(mirroring), selected_bank_(0) {

	// Calculate number of 16KB banks
	num_banks_ = static_cast<std::uint8_t>(prg_rom_.size() / 16384);

	// UxROM uses CHR RAM (8KB), not CHR ROM
	// Initialize CHR RAM if not provided
	if (chr_rom.empty()) {
		chr_ram_.resize(8192, 0); // 8KB CHR RAM
	} else {
		// Some variants may have CHR ROM, copy it to RAM
		chr_ram_ = std::move(chr_rom);
		if (chr_ram_.size() < 8192) {
			chr_ram_.resize(8192, 0);
		}
	}
}

Byte Mapper002::cpu_read(Address address) const {
	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	// Address range: $8000-$FFFF
	if (address >= 0x8000 && address <= 0xBFFF) {
		// $8000-$BFFF: Switchable 16KB bank
		Address bank_offset = (selected_bank_ * 16384);
		Address rom_address = bank_offset + (address - 0x8000);

		if (rom_address >= prg_rom_.size()) {
			return 0xFF; // Beyond ROM bounds
		}

		return prg_rom_[rom_address];
	} else {
		// $C000-$FFFF: Fixed to last 16KB bank
		Address last_bank_offset = (num_banks_ - 1) * 16384;
		Address rom_address = last_bank_offset + (address - 0xC000);

		if (rom_address >= prg_rom_.size()) {
			return 0xFF; // Beyond ROM bounds
		}

		return prg_rom_[rom_address];
	}
}

void Mapper002::cpu_write(Address address, Byte value) {
	if (!is_prg_rom_address(address)) {
		return; // Outside mapper control range
	}

	// Any write to $8000-$FFFF selects PRG bank
	// Use mask to handle different ROM sizes (typically 3-4 bits)
	selected_bank_ = value & get_bank_mask();
}

Byte Mapper002::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF; // Outside CHR range
	}

	if (address >= chr_ram_.size()) {
		return 0xFF; // Beyond CHR RAM bounds
	}

	return chr_ram_[address];
}

void Mapper002::ppu_write(Address address, Byte value) {
	if (!is_chr_address(address)) {
		return; // Outside CHR range
	}

	if (address >= chr_ram_.size()) {
		return; // Beyond CHR RAM bounds
	}

	// CHR RAM is writable
	chr_ram_[address] = value;
}

void Mapper002::reset() {
	// Reset to first bank
	selected_bank_ = 0;
}

} // namespace nes
