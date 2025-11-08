#include "cartridge/mappers/mapper_002.hpp"
#include <iostream>

namespace nes {

Mapper002::Mapper002(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring)
	: prg_rom_(std::move(prg_rom)), mirroring_(mirroring), selected_bank_(0) {

	// Calculate number of 16KB banks
	num_banks_ = static_cast<std::uint8_t>(prg_rom_.size() / 16384);

	std::cout << "[Mapper002] Initialized with " << static_cast<int>(num_banks_) << " PRG banks (" << prg_rom_.size()
			  << " bytes)" << std::endl;
	std::cout << "[Mapper002] Bank mask: 0x" << std::hex << static_cast<int>(get_bank_mask()) << std::dec << std::endl;

	// Debug: Check reset vector in last bank
	std::size_t reset_vector_offset = static_cast<std::size_t>((num_banks_ - 1) * 16384) + 0x3FFC;
	if (reset_vector_offset + 1 < prg_rom_.size()) {
		std::uint16_t reset_vector = prg_rom_[reset_vector_offset] | (prg_rom_[reset_vector_offset + 1] << 8);
		std::cout << "[Mapper002] Reset vector at $FFFC: $" << std::hex << reset_vector << std::dec << std::endl;
	}

	// UxROM uses CHR RAM (8KB), not CHR ROM
	// Initialize CHR RAM if not provided
	if (chr_rom.empty()) {
		chr_ram_.resize(8192, 0); // 8KB CHR RAM
		std::cout << "[Mapper002] Using 8KB CHR RAM" << std::endl;
	} else {
		// Some variants may have CHR ROM, copy it to RAM
		chr_ram_ = std::move(chr_rom);
		if (chr_ram_.size() < 8192) {
			chr_ram_.resize(8192, 0);
		}
		std::cout << "[Mapper002] Using CHR RAM initialized from ROM" << std::endl;
	}
}

Byte Mapper002::cpu_read(Address address) const {
	if (!is_prg_rom_address(address)) {
		return 0xFF; // Open bus
	}

	std::size_t rom_address;

	// Address range: $8000-$FFFF
	if (address >= 0x8000 && address <= 0xBFFF) {
		// $8000-$BFFF: Switchable 16KB bank
		std::size_t bank_offset = static_cast<std::size_t>(selected_bank_) * 16384;
		rom_address = bank_offset + (address - 0x8000);
	} else {
		// $C000-$FFFF: Fixed to last 16KB bank
		std::size_t last_bank_offset = static_cast<std::size_t>(num_banks_ - 1) * 16384;
		rom_address = last_bank_offset + (address - 0xC000);
	}

	if (rom_address >= prg_rom_.size()) {
		std::cerr << "[Mapper002] ERROR: rom_address " << std::hex << rom_address << " >= prg_rom_.size() "
				  << prg_rom_.size() << " (address=$" << address << ", bank=" << static_cast<int>(selected_bank_)
				  << ", num_banks=" << static_cast<int>(num_banks_) << ")" << std::dec << std::endl;
		return 0xFF; // Beyond ROM bounds
	}

	return prg_rom_[rom_address];
}

void Mapper002::cpu_write(Address address, Byte value) {
	if (!is_prg_rom_address(address)) {
		return; // Outside mapper control range
	}

	// UxROM has bus conflicts: The mapper sees the AND of CPU data bus and ROM data bus
	// Nintendo-published games (like Guardian Legend) work around this by writing to
	// ROM addresses that already contain the desired value
	Byte rom_value = cpu_read(address);
	Byte effective_value = value & rom_value;

	// Any write to $8000-$FFFF selects PRG bank
	// Use mask to handle different ROM sizes (typically 3-4 bits)
	selected_bank_ = effective_value & get_bank_mask();
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

void Mapper002::serialize_state(std::vector<Byte> &buffer) const {
	// Serialize CHR RAM (8KB)
	buffer.insert(buffer.end(), chr_ram_.begin(), chr_ram_.end());

	// Serialize selected bank
	buffer.push_back(selected_bank_);
}

void Mapper002::deserialize_state(const std::vector<Byte> &buffer, size_t &offset) {
	// Deserialize CHR RAM (8KB)
	for (size_t i = 0; i < chr_ram_.size() && offset < buffer.size(); ++i) {
		chr_ram_[i] = buffer[offset++];
	}

	// Deserialize selected bank
	selected_bank_ = buffer[offset++];
}

} // namespace nes
