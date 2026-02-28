#pragma once

#include "cartridge/mappers/mapper.hpp"
#include <vector>

namespace nes {

/**
 * Mapper 3 (CNROM) - CHR ROM bank switching
 *
 * PRG ROM: 16KB or 32KB at $8000-$FFFF (no bank switching)
 * CHR ROM: Switchable 8KB bank at $0000-$1FFF (PPU)
 *
 * Bank select: Write to $8000-$FFFF selects CHR bank (lower 2-3 bits)
 * Bus conflicts: Yes (same as UxROM)
 *
 * Used by: Arkanoid, Solomon's Key, Gradius, Paperboy, Q*bert,
 *          Cybernoid, Bump'n'Jump, Mighty Bomb Jack, etc.
 */
class Mapper003 final : public Mapper {
  public:
	Mapper003(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring);

	// CPU memory access
	Byte cpu_read(Address address) const override;
	void cpu_write(Address address, Byte value) override;

	// PPU memory access
	Byte ppu_read(Address address) const override;
	void ppu_write(Address address, Byte value) override;

	// Mapper info
	std::uint8_t get_mapper_id() const noexcept override {
		return 3;
	}
	const char *get_name() const noexcept override {
		return "CNROM";
	}

	void reset() override;
	Mirroring get_mirroring() const noexcept override {
		return mirroring_;
	}

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const override;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) override;

  private:
	std::vector<Byte> prg_rom_;		 // Program ROM (16KB or 32KB)
	std::vector<Byte> chr_rom_;		 // Character ROM (multiple 8KB banks)
	Mirroring mirroring_;			 // Nametable mirroring mode
	std::uint8_t selected_chr_bank_; // Currently selected CHR bank
	std::uint8_t num_chr_banks_;	 // Total number of 8KB CHR banks

	// PRG ROM can be 16KB (mirrored) or 32KB
	bool is_16kb_prg() const noexcept {
		return prg_rom_.size() <= 16384;
	}

	// Calculate mask for CHR bank selection based on ROM size
	std::uint8_t get_chr_bank_mask() const noexcept {
		return num_chr_banks_ > 0 ? (num_chr_banks_ - 1) : 0;
	}
};

} // namespace nes
