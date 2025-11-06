#pragma once

#include "core/types.hpp"
#include <vector>

namespace nes {

/**
 * Base class for all NES mappers
 * Mappers control how ROM/RAM is mapped into CPU and PPU address spaces
 */
class Mapper {
  public:
	virtual ~Mapper() = default;

	// CPU memory access (PRG ROM/RAM)
	virtual Byte cpu_read(Address address) const = 0;
	virtual void cpu_write(Address address, Byte value) = 0;

	// PPU memory access (CHR ROM/RAM)
	virtual Byte ppu_read(Address address) const = 0;
	virtual void ppu_write(Address address, Byte value) = 0;

	// Mapper information
	virtual std::uint8_t get_mapper_id() const noexcept = 0;
	virtual const char *get_name() const noexcept = 0;

	// Reset the mapper state
	virtual void reset() = 0;

	// Get mirroring mode for nametables
	enum class Mirroring { Horizontal, Vertical, SingleScreenLow, SingleScreenHigh, FourScreen };
	virtual Mirroring get_mirroring() const noexcept = 0;

	// PPU A12 line toggle notification (for MMC3 scanline counting)
	virtual void ppu_a12_toggle() {
		// Default implementation does nothing
		// Override in mappers that need A12 monitoring (like MMC3)
	}

	// IRQ line status (for MMC3, MMC5, etc.)
	virtual bool is_irq_pending() const {
		return false; // Default: no IRQ support
	}

	// Clear mapper IRQ (called when CPU acknowledges the interrupt)
	virtual void clear_irq() {
		// Default implementation does nothing
	}

	// Save state serialization
	virtual void serialize_state(std::vector<uint8_t> &buffer) const = 0;
	virtual void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) = 0;

  protected:
	// Helper to check if address is in PRG ROM range
	static constexpr bool is_prg_rom_address(Address address) noexcept {
		return address >= 0x8000;
	}

	// Helper to check if address is in CHR range
	static constexpr bool is_chr_address(Address address) noexcept {
		return address <= 0x1FFF;
	}
};

} // namespace nes
