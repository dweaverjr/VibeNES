#pragma once

#include "cartridge/mappers/mapper.hpp"
#include <array>
#include <vector>

namespace nes {

/**
 * Mapper 4 (MMC3) - Nintendo's most complex and widely used mapper
 * Advanced bank switching with scanline-based IRQ timer
 *
 * PRG ROM: Up to 512KB with 8KB and 16KB banking modes
 * PRG RAM: 8KB at $6000-$7FFF (often battery-backed)
 * CHR ROM/RAM: Up to 256KB with 1KB and 2KB banking
 * IRQ: Scanline counter for precise timing interrupts
 *
 * Bank Select Register ($8000-$9FFF, even addresses):
 *   bit 0-2: Bank register to update (0-7)
 *   bit 3: Reserved (should be 0)
 *   bit 4-5: Reserved (should be 0)
 *   bit 6: PRG ROM bank mode (0=normal, 1=swapped)
 *   bit 7: CHR bank mode (0=normal, 1=inverted)
 *
 * Bank Data Register ($8001-$9FFF, odd addresses):
 *   Updates the bank register selected by Bank Select
 *
 * Mirroring Register ($A000-$BFFF, even addresses):
 *   bit 0: Mirroring (0=vertical, 1=horizontal)
 *
 * PRG RAM Protect Register ($A001-$BFFF, odd addresses):
 *   bit 6: PRG RAM write protect (0=allow writes, 1=deny writes)
 *   bit 7: PRG RAM chip enable (0=disable, 1=enable)
 *
 * IRQ Latch Register ($C000-$DFFF, even addresses):
 *   Sets the IRQ counter reload value
 *
 * IRQ Reload Register ($C001-$DFFF, odd addresses):
 *   Reloads the IRQ counter with the latch value
 *
 * IRQ Disable Register ($E000-$FFFF, even addresses):
 *   Disables IRQ generation and acknowledges pending IRQ
 *
 * IRQ Enable Register ($E001-$FFFF, odd addresses):
 *   Enables IRQ generation
 *
 * Used by: Super Mario Bros. 2/3, Mega Man 3-6, Willow, many others
 */
class Mapper004 final : public Mapper {
  public:
	Mapper004(std::vector<Byte> prg_rom, std::vector<Byte> chr_mem, Mirroring mirroring, bool has_prg_ram = true,
			  bool chr_is_ram = false);

	// CPU memory access
	Byte cpu_read(Address address) const override;
	void cpu_write(Address address, Byte value) override;

	// PPU memory access
	Byte ppu_read(Address address) const override;
	void ppu_write(Address address, Byte value) override;

	// Mapper info
	std::uint8_t get_mapper_id() const noexcept override {
		return 4;
	}
	const char *get_name() const noexcept override {
		return "MMC3";
	}

	void reset() override;
	Mirroring get_mirroring() const noexcept override;

	// PPU A12 line monitoring for IRQ timing
	void ppu_a12_toggle() override;

	// IRQ support
	bool is_irq_pending() const override {
		return irq_pending_;
	}
	void clear_irq() override {
		irq_pending_ = false;
	}

	// Save state support
	void serialize_state(std::vector<Byte> &buffer) const override;
	void deserialize_state(const std::vector<Byte> &buffer, size_t &offset) override;

  private:
	std::vector<Byte> prg_rom_;	  // Program ROM (up to 512KB)
	std::vector<Byte> prg_ram_;	  // Program RAM (8KB at $6000-$7FFF)
	std::vector<Byte> chr_mem_;	  // Character ROM or RAM (up to 256KB)
	Mirroring initial_mirroring_; // Initial mirroring from iNES header
	bool has_prg_ram_;			  // Does cartridge have PRG RAM?
	bool chr_is_ram_;			  // Is CHR memory writable RAM?

	// MMC3 Registers
	Byte bank_select_;			// $8000: Bank select and mode control
	std::array<Byte, 8> banks_; // $8001: Bank data registers (R0-R7)
	bool mirroring_;			// $A000: Mirroring control (0=vertical, 1=horizontal)
	Byte prg_ram_protect_;		// $A001: PRG RAM protection

	// IRQ System
	Byte irq_latch_;	   // $C000: IRQ counter reload value
	Byte irq_counter_;	   // Current IRQ counter
	bool irq_reload_;	   // $C001: IRQ reload flag
	bool irq_enabled_;	   // $E001: IRQ enable flag
	bool irq_pending_;	   // Internal IRQ pending flag
	bool irq_initialized_; // Has the game written to any IRQ register?

	// Helper functions
	std::size_t get_prg_bank_offset(Address address) const;
	std::size_t get_chr_bank_offset(Address address) const;

	// Bank configuration helpers
	bool get_prg_bank_mode() const {
		return (bank_select_ & 0x40) != 0;
	}
	bool get_chr_bank_mode() const {
		return (bank_select_ & 0x80) != 0;
	}
	Byte get_selected_bank_register() const {
		return bank_select_ & 0x07;
	}

	// PRG RAM control helpers
	bool is_prg_ram_enabled() const {
		return (prg_ram_protect_ & 0x80) != 0;
	}
	bool is_prg_ram_writable() const {
		return (prg_ram_protect_ & 0x40) == 0;
	}

	// IRQ system helpers
	void clock_irq_counter();
	void update_irq_line();

	// Bank size calculations
	std::size_t get_prg_8kb_bank_count() const {
		return prg_rom_.size() / 8192;
	}
	std::size_t get_chr_1kb_bank_count() const {
		return chr_mem_.size() / 1024;
	}
};

} // namespace nes
