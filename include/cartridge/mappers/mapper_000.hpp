#pragma once

#include "cartridge/mappers/mapper.hpp"
#include <vector>

namespace nes {

/**
 * Mapper 0 (NROM) - No mapper hardware
 * Simple direct mapping of ROM data
 *
 * PRG ROM: 16KB or 32KB at $8000-$FFFF
 * CHR ROM: 8KB at $0000-$1FFF (PPU)
 *
 * Used by: Super Mario Bros, Donkey Kong, etc.
 */
class Mapper000 final : public Mapper {
  public:
	Mapper000(std::vector<Byte> prg_rom, std::vector<Byte> chr_rom, Mirroring mirroring);

	// CPU memory access
	Byte cpu_read(Address address) const override;
	void cpu_write(Address address, Byte value) override;

	// PPU memory access
	Byte ppu_read(Address address) const override;
	void ppu_write(Address address, Byte value) override;

	// Mapper info
	std::uint8_t get_mapper_id() const noexcept override {
		return 0;
	}
	const char *get_name() const noexcept override {
		return "NROM";
	}

	void reset() override;
	Mirroring get_mirroring() const noexcept override {
		return mirroring_;
	}

  private:
	std::vector<Byte> prg_rom_; // Program ROM (16KB or 32KB)
	std::vector<Byte> chr_rom_; // Character ROM (8KB)
	Mirroring mirroring_;		// Nametable mirroring mode

	// PRG ROM can be 16KB (mirrored) or 32KB
	bool is_16kb_prg() const noexcept {
		return prg_rom_.size() == 16384;
	}
};

} // namespace nes
