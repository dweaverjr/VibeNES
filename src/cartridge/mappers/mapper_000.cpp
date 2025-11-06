#include "cartridge/mappers/mapper_000.hpp"
#include <iostream>

namespace nes {

Mapper000::Mapper000(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring)
	: prg_rom_(std::move(prg_rom)), chr_rom_(std::move(chr_rom)), mirroring_(mirroring) {
	// Mapper initialized successfully
}

Byte Mapper000::cpu_read(Address address) const {
	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	// Map to PRG ROM space
	Address rom_address = address - 0x8000;

	if (is_16kb_prg()) {
		// 16KB ROM: Mirror the 16KB ROM in both halves
		// $8000-$BFFF and $C000-$FFFF both map to the same 16KB
		rom_address &= 0x3FFF; // Mask to 16KB
	}
	// For 32KB ROM, use address as-is (0x0000-0x7FFF)

	if (rom_address >= prg_rom_.size()) {
		return 0xFF; // Beyond ROM bounds
	}

	Byte value = prg_rom_[rom_address];
	return value;
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

	if (address >= chr_rom_.size()) {
		return 0xFF; // Beyond CHR ROM bounds
	}

	return chr_rom_[address];
}
void Mapper000::ppu_write(Address address, Byte value) {
	// NROM uses CHR ROM (read-only), writes are ignored
	(void)address;
	(void)value;
}

void Mapper000::reset() {
	// NROM has no state to reset
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
