#include "cartridge/mappers/mapper_003.hpp"
#include <iostream>

namespace nes {

Mapper003::Mapper003(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring)
	: prg_rom_(std::move(prg_rom)), chr_rom_(std::move(chr_rom)), mirroring_(mirroring), selected_chr_bank_(0) {

	// Calculate number of 8KB CHR banks
	num_chr_banks_ = static_cast<std::uint8_t>(chr_rom_.size() / 8192);
	if (num_chr_banks_ == 0) {
		num_chr_banks_ = 1; // At least 1 bank
	}

	std::cout << "[Mapper003] Initialized: PRG=" << prg_rom_.size() << " bytes ("
			  << (is_16kb_prg() ? "16KB mirrored" : "32KB") << "), CHR=" << chr_rom_.size() << " bytes ("
			  << static_cast<int>(num_chr_banks_) << " banks)" << std::endl;
}

Byte Mapper003::cpu_read(Address address) const {
	// PRG RAM at $6000-$7FFF: not present on CNROM
	if (address >= 0x6000 && address < 0x8000) {
		return 0xFF; // Open bus
	}

	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	// Map to PRG ROM space
	Address rom_address = address - 0x8000;

	if (is_16kb_prg()) {
		// 16KB ROM: Mirror the 16KB ROM in both halves
		rom_address &= 0x3FFF;
	}

	if (rom_address >= prg_rom_.size()) {
		return 0xFF; // Beyond ROM bounds
	}

	return prg_rom_[rom_address];
}

void Mapper003::cpu_write(Address address, Byte value) {
	if (!is_prg_rom_address(address)) {
		return; // Outside mapper control range
	}

	// CNROM has bus conflicts: the mapper sees the AND of CPU data bus and ROM data bus
	Byte rom_value = cpu_read(address);
	Byte effective_value = value & rom_value;

	// Any write to $8000-$FFFF selects CHR bank
	// Only lower bits used depending on number of CHR banks
	selected_chr_bank_ = effective_value & get_chr_bank_mask();
}

Byte Mapper003::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF; // Outside CHR range
	}

	// Calculate offset into selected CHR bank
	std::size_t bank_offset = static_cast<std::size_t>(selected_chr_bank_) * 8192;
	std::size_t chr_address = bank_offset + address;

	if (chr_address >= chr_rom_.size()) {
		return 0xFF; // Beyond CHR ROM bounds
	}

	return chr_rom_[chr_address];
}

void Mapper003::ppu_write(Address address, Byte value) {
	// CNROM uses CHR ROM (read-only), writes are ignored
	(void)address;
	(void)value;
}

void Mapper003::reset() {
	// Reset to first CHR bank
	selected_chr_bank_ = 0;
}

void Mapper003::serialize_state(std::vector<uint8_t> &buffer) const {
	// Serialize selected CHR bank
	buffer.push_back(selected_chr_bank_);
}

void Mapper003::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// Deserialize selected CHR bank
	if (offset < buffer.size()) {
		selected_chr_bank_ = buffer[offset++];
	}
}

} // namespace nes
