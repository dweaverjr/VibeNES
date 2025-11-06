#pragma once

#include "cartridge/mappers/mapper.hpp"
#include <vector>

namespace nes {

/**
 * Mapper 2 (UxROM) - Simple bank switching
 * PRG ROM bank switching mapper
 *
 * PRG ROM: Switchable 16KB bank at $8000-$BFFF
 *          Fixed 16KB bank (last bank) at $C000-$FFFF
 * CHR RAM: 8KB at $0000-$1FFF (PPU) - writable
 *
 * Bank select: Write to $8000-$FFFF selects PRG bank
 * Only lower 3-4 bits used depending on ROM size
 *
 * Used by: Mega Man, Castlevania, Duck Tales, The Guardian Legend, etc.
 */
class Mapper002 final : public Mapper {
  public:
	Mapper002(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring);

	// CPU memory access
	Byte cpu_read(Address address) const override;
	void cpu_write(Address address, Byte value) override;

	// PPU memory access
	Byte ppu_read(Address address) const override;
	void ppu_write(Address address, Byte value) override;

	// Mapper info
	std::uint8_t get_mapper_id() const noexcept override {
		return 2;
	}
	const char *get_name() const noexcept override {
		return "UxROM";
	}

	void reset() override;
	Mirroring get_mirroring() const noexcept override {
		return mirroring_;
	}

	// Save state support
	void serialize_state(std::vector<Byte> &buffer) const override;
	void deserialize_state(const std::vector<Byte> &buffer, size_t &offset) override;

  private:
	std::vector<Byte> prg_rom_;	 // Program ROM (multiple 16KB banks)
	std::vector<Byte> chr_ram_;	 // Character RAM (8KB, writable)
	Mirroring mirroring_;		 // Nametable mirroring mode
	std::uint8_t selected_bank_; // Currently selected PRG bank (0-15)
	std::uint8_t num_banks_;	 // Total number of 16KB PRG banks

	// Calculate mask for bank selection based on ROM size
	std::uint8_t get_bank_mask() const noexcept {
		return num_banks_ - 1;
	}
};

} // namespace nes
