#pragma once

#include "core/types.hpp"
#include <array>
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

	// IRQ line status (for MMC3, MMC5, etc.). Non-virtual: the bus polls this
	// every CPU cycle, so it must be a plain bool load, not a virtual call.
	// Mappers with IRQ support set/clear the protected irq_pending_ member.
	bool is_irq_pending() const noexcept {
		return irq_pending_;
	}

	// Clear mapper IRQ (called when CPU acknowledges the interrupt)
	void clear_irq() noexcept {
		irq_pending_ = false;
	}

	// Notify mapper of CPU cycles elapsed (for timing-sensitive behavior)
	virtual void notify_cpu_cycle() {
		// Default implementation does nothing
		// Override in mappers that need cycle tracking (like MMC1)
	}

	// Whether this mapper needs per-cycle notify_cpu_cycle() calls.
	// Queried once at ROM load so the hot path can skip the call entirely.
	virtual bool wants_cpu_cycle_notifications() const noexcept {
		return false;
	}

	// Save state serialization
	virtual void serialize_state(std::vector<uint8_t> &buffer) const = 0;
	virtual void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) = 0;

  protected:
	// IRQ pending flag (read by the non-virtual is_irq_pending() above).
	// Only IRQ-capable mappers (MMC3) ever set this.
	bool irq_pending_ = false;

	// Helper to check if address is in PRG ROM range
	static constexpr bool is_prg_rom_address(Address address) noexcept {
		return address >= 0x8000;
	}

	// Helper to check if address is in CHR range
	static constexpr bool is_chr_address(Address address) noexcept {
		return address <= 0x1FFF;
	}

	// Shared 8KB page of 0xFF used as the target for bank-map slots whose
	// computed offset falls outside the ROM data (open-bus reads). Lets the
	// hot read path stay a branch-free pointer lookup.
	static inline const std::array<Byte, 8192> OPEN_BUS_PAGE = [] {
		std::array<Byte, 8192> page{};
		page.fill(0xFF);
		return page;
	}();
};

} // namespace nes
