#pragma once

#include "cartridge/mappers/mapper.hpp"
#include "cartridge/rom_loader.hpp"
#include "core/component.hpp"
#include <memory>
#include <string>

namespace nes {

/**
 * NES Cartridge - Manages ROM data and mapper hardware
 * Handles loading ROM files and provides CPU/PPU memory access
 */
class Cartridge final : public Component {
  public:
	Cartridge() = default;
	~Cartridge() = default;

	// Component interface
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	const char *get_name() const noexcept override;

	// Cartridge operations
	bool load_rom(const std::string &filepath);
	bool load_from_rom_data(const RomData &rom_data); // For testing with synthetic ROM data
	void unload_rom();
	bool is_loaded() const noexcept {
		return mapper_ != nullptr;
	}

	// Memory access (called by SystemBus)
	Byte cpu_read(Address address) const;
	void cpu_write(Address address, Byte value);
	Byte ppu_read(Address address) const;
	void ppu_write(Address address, Byte value);

	// Mapper notifications (for MMC3 scanline counter, etc.)
	void ppu_a12_toggle() const;

	// IRQ support (for MMC3, MMC5, etc.). Inline + non-virtual: polled every
	// CPU cycle by the bus, so this must collapse to a couple of loads.
	bool is_irq_pending() const noexcept {
		return mapper_ && mapper_->is_irq_pending();
	}
	void clear_irq() const noexcept {
		if (mapper_) {
			mapper_->clear_irq();
		}
	}

	// Per-cycle mapper notification, inline with cached early-out (only MMC1
	// opts in). Called from the bus hot path instead of the virtual tick().
	void notify_cpu_cycles(int count) noexcept {
		if (mapper_wants_cycle_notify_) {
			for (int i = 0; i < count; ++i) {
				mapper_->notify_cpu_cycle();
			}
		}
	}

	// ROM information
	const RomData &get_rom_data() const noexcept {
		return rom_data_;
	}
	std::uint8_t get_mapper_id() const noexcept;
	const char *get_mapper_name() const noexcept;
	Mapper::Mirroring get_mirroring() const noexcept;

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset);

	// Additional getters needed for save state
	const std::vector<uint8_t> &get_prg_rom() const {
		return rom_data_.prg_rom;
	}
	const std::string &get_rom_filename() const {
		return rom_data_.filename;
	}

  private:
	std::unique_ptr<Mapper> mapper_;
	RomData rom_data_;
	// Cached at load: whether mapper_ needs per-cycle notify_cpu_cycle() calls
	// (only MMC1). Lets tick() early-out instead of a virtual call per CPU cycle.
	bool mapper_wants_cycle_notify_ = false;
};

} // namespace nes
