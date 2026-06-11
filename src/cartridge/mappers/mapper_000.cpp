#include "cartridge/mappers/mapper_000.hpp"
#include <iostream>

namespace nes {

Mapper000::Mapper000(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring)
	: prg_rom_(std::move(prg_rom)), chr_rom_(std::move(chr_rom)), mirroring_(mirroring) {
	update_bank_maps();
}

void Mapper000::update_bank_maps() {
	// PRG: 4 slots of 8KB covering $8000-$FFFF. 16KB ROMs mirror into both halves.
	const bool mirror_16kb = is_16kb_prg();
	for (std::size_t slot = 0; slot < prg_map_.size(); ++slot) {
		std::size_t offset = slot * 0x2000;
		if (mirror_16kb) {
			offset &= 0x3FFF;
		}
		prg_map_[slot] = (offset + 0x2000 <= prg_rom_.size()) ? prg_rom_.data() + offset : OPEN_BUS_PAGE.data();
	}

	// CHR: 8 slots of 1KB covering $0000-$1FFF (no banking on NROM).
	for (std::size_t slot = 0; slot < chr_map_.size(); ++slot) {
		std::size_t offset = slot * 1024;
		chr_map_[slot] = (offset + 1024 <= chr_rom_.size()) ? chr_rom_.data() + offset : OPEN_BUS_PAGE.data();
	}
}

Byte Mapper000::cpu_read(Address address) const {
	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	// Cached 8KB bank pointers — no per-byte offset math or bounds checks
	return prg_map_[(address >> 13) & 0x03][address & 0x1FFF];
}

void Mapper000::cpu_write(Address address, Byte value) {
	// NROM has no PRG RAM, writes are ignored
	(void)address;
	(void)value;
}

Byte Mapper000::ppu_read(Address address) const {
	if (!is_chr_address(address)) {
		return 0xFF; // Outside CHR range
	}

	// Cached 1KB bank pointers
	return chr_map_[address >> 10][address & 0x03FF];
}
void Mapper000::ppu_write(Address address, Byte value) {
	// NROM uses CHR ROM (read-only), writes are ignored
	(void)address;
	(void)value;
}

void Mapper000::reset() {
	// NROM has no banking state; rebuild the (static) maps for consistency
	update_bank_maps();
}

// Save state serialization
void Mapper000::serialize_state(std::vector<uint8_t> &buffer) const {
	// NROM has no mapper state (no registers, no RAM)
	// CHR ROM is read-only, so we don't need to save it
	// Nothing to serialize!
	(void)buffer;
}

void Mapper000::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// NROM has no mapper state to restore
	// Nothing to deserialize!
	(void)buffer;
	(void)offset;
}

} // namespace nes
