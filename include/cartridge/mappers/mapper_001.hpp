#pragma once

#include "cartridge/mappers/mapper.hpp"
#include <vector>

namespace nes {

/**
 * Mapper 1 (MMC1) - Nintendo's first mapper chip
 * Complex bank switching with shift register interface
 *
 * PRG ROM: Up to 512KB with 16KB or 32KB banking
 * PRG RAM: 8KB at $6000-$7FFF (battery-backed on some carts)
 * CHR ROM/RAM: Up to 128KB with 4KB or 8KB banking
 *
 * Control Register ($8000-$9FFF):
 *   bit 0-1: Mirroring (0=one-screen lower, 1=one-screen upper, 2=vertical, 3=horizontal)
 *   bit 2-3: PRG ROM bank mode (0,1=32KB mode, 2=fix first bank, 3=fix last bank)
 *   bit 4: CHR ROM bank mode (0=8KB mode, 1=4KB mode)
 *
 * Used by: The Legend of Zelda, Metroid, Mega Man 2, Faxanadu, many others
 */
class Mapper001 final : public Mapper {
  public:
	Mapper001(std::vector<Byte> prg_rom, std::vector<Byte> chr_mem, Mirroring mirroring, bool has_prg_ram = true,
			  bool chr_is_ram = false);

	// CPU memory access
	Byte cpu_read(Address address) const override;
	void cpu_write(Address address, Byte value) override;

	// PPU memory access
	Byte ppu_read(Address address) const override;
	void ppu_write(Address address, Byte value) override;

	// Mapper info
	std::uint8_t get_mapper_id() const noexcept override {
		return 1;
	}
	const char *get_name() const noexcept override {
		return "MMC1";
	}

	void reset() override;
	Mirroring get_mirroring() const noexcept override;

	// Cycle tracking for consecutive-write filter
	void notify_cpu_cycle() override {
		++cpu_cycle_counter_;
	}

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const override;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) override;

  private:
	std::vector<Byte> prg_rom_;	  // Program ROM (up to 512KB)
	std::vector<Byte> prg_ram_;	  // Program RAM (8KB at $6000-$7FFF)
	std::vector<Byte> chr_mem_;	  // Character ROM or RAM (up to 128KB)
	Mirroring initial_mirroring_; // Initial mirroring from iNES header
	bool has_prg_ram_;			  // Does cartridge have PRG RAM?
	bool chr_is_ram_;			  // Is CHR memory writable RAM?

	// MMC1 Registers
	Byte shift_register_; // 5-bit shift register for serial writes
	Byte shift_count_;	  // Number of bits written (0-4)

	// Internal control registers (loaded from shift register)
	Byte control_register_; // $8000-$9FFF: mirroring, PRG/CHR modes
	Byte chr_bank_0_;		// $A000-$BFFF: CHR bank 0 (4KB or 8KB)
	Byte chr_bank_1_;		// $C000-$DFFF: CHR bank 1 (4KB mode only)
	Byte prg_bank_;			// $E000-$FFFF: PRG bank select
	bool prg_ram_enabled_;	// PRG RAM enable bit (bit 4 of PRG bank register)

	// Consecutive-write filter: real MMC1 ignores writes on consecutive CPU cycles.
	// RMW instructions (INC, DEC, ASL, etc.) write the old value then the new value
	// on back-to-back cycles; only the first (old value) should be processed.
	uint64_t cpu_cycle_counter_; // Running CPU cycle count
	uint64_t last_write_cycle_;	 // Cycle of last $8000+ write

	// Helper functions
	void write_shift_register(Address address, Byte value);
	void load_register(Address address);

	std::size_t get_prg_bank_offset(Address address) const;
	std::size_t get_chr_bank_offset(Address address) const;

	// Control register bit extraction
	Byte get_mirroring_mode() const {
		return control_register_ & 0x03;
	}
	Byte get_prg_bank_mode() const {
		return (control_register_ >> 2) & 0x03;
	}
	bool get_chr_bank_mode() const {
		return (control_register_ & 0x10) != 0;
	}
};

} // namespace nes
