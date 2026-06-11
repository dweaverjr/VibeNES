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

	update_bank_maps();
}

void Mapper003::update_bank_maps() {
	// PRG: fixed mapping (16KB mirrored or 32KB) in 8KB slots
	const bool mirror_16kb = is_16kb_prg();
	for (std::size_t slot = 0; slot < prg_map_.size(); ++slot) {
		std::size_t offset = slot * 0x2000;
		if (mirror_16kb) {
			offset &= 0x3FFF;
		}
		prg_map_[slot] = (offset + 0x2000 <= prg_rom_.size()) ? prg_rom_.data() + offset : OPEN_BUS_PAGE.data();
	}

	// CHR: selected 8KB bank in 1KB slots
	const std::size_t bank_base = static_cast<std::size_t>(selected_chr_bank_) * 8192;
	for (std::size_t slot = 0; slot < chr_map_.size(); ++slot) {
		std::size_t offset = bank_base + slot * 1024;
		chr_map_[slot] = (offset + 1024 <= chr_rom_.size()) ? chr_rom_.data() + offset : OPEN_BUS_PAGE.data();
	}
}

Byte Mapper003::cpu_read(Address address) const {
	// PRG RAM at $6000-$7FFF: not present on CNROM
	if (address >= 0x6000 && address < 0x8000) {
		return 0xFF; // Open bus
	}

	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	// Cached 8KB bank pointers
	return prg_map_[(address >> 13) & 0x03][address & 0x1FFF];
}

void Mapper003::cpu_write(Address address, Byte value) {
	if (!is_prg_rom_address(address)) {
		return; // Outside mapper control range
	}

	// CNROM has bus conflicts: the mapper sees the AND of CPU data bus and ROM data bus.
	// cpu_read(address) returns the byte currently mapped at exactly this address, which
	// is precisely the ROM operand the hardware ANDs against (CNROM PRG mapping is fixed,
	// so this is always the correct byte).
	Byte rom_value = cpu_read(address);
	Byte effective_value = value & rom_value;

	// Any write to $8000-$FFFF selects CHR bank
	// Only lower bits used depending on number of CHR banks
	selected_chr_bank_ = effective_value & get_chr_bank_mask();
	update_bank_maps();
}

Byte Mapper003::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF; // Outside CHR range
	}

	// Cached 1KB bank pointers
	return chr_map_[address >> 10][address & 0x03FF];
}

void Mapper003::ppu_write(Address address, Byte value) {
	// CNROM uses CHR ROM (read-only), writes are ignored
	(void)address;
	(void)value;
}

void Mapper003::reset() {
	// Reset to first CHR bank
	selected_chr_bank_ = 0;
	update_bank_maps();
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
	update_bank_maps();
}

} // namespace nes
