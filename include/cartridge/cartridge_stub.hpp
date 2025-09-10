#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>
#include <vector>

namespace nes {

/// Cartridge Stub - Placeholder for cartridge/ROM handling
/// Implements basic cartridge memory access for Phase 1
/// Will be expanded into full cartridge implementation later
class CartridgeStub final : public Component {
  public:
	CartridgeStub() = default;

	// Component interface
	void tick(CpuCycle cycles) override {
		// Cartridge stub doesn't need timing behavior yet
		(void)cycles;
	}

	void reset() override {
		// Cartridge state doesn't change on reset
		// TODO: Some mappers may need reset behavior
	}

	void power_on() override {
		// Cartridge state doesn't change on power-on
		// TODO: Some mappers may need power-on behavior
	}

	[[nodiscard]] const char *get_name() const noexcept override {
		return "Cartridge (Stub)";
	}

	/// Read from cartridge memory space
	[[nodiscard]] Byte read(Address address) const noexcept {
		// SRAM area ($6000-$7FFF)
		if (address >= 0x6000 && address <= 0x7FFF) {
			if (has_sram_) {
				const Address sram_addr = address - 0x6000;
				return sram_[sram_addr];
			}
			return 0x00; // No SRAM
		}

		// PRG ROM area ($8000-$FFFF)
		if (address >= 0x8000) {
			const Address rom_addr = address - 0x8000;
			if (rom_addr < prg_rom_.size()) {
				return prg_rom_[rom_addr];
			}
			return 0xFF; // Open bus
		}

		return 0x00;
	}

	/// Write to cartridge memory space
	void write(Address address, Byte value) noexcept {
		// SRAM area ($6000-$7FFF)
		if (address >= 0x6000 && address <= 0x7FFF) {
			if (has_sram_) {
				const Address sram_addr = address - 0x6000;
				sram_[sram_addr] = value;
			}
			return;
		}

		// PRG ROM area ($8000-$FFFF) - typically read-only
		// TODO: Some mappers may allow writes for bank switching
		// For now, ignore writes to ROM area
		(void)address;
		(void)value;
	}

	/// Load test ROM data
	void load_test_rom(const std::vector<Byte> &rom_data) {
		prg_rom_ = rom_data;
		// Ensure minimum size for interrupt vectors
		if (prg_rom_.size() < 0x8000) {
			prg_rom_.resize(0x8000, 0xFF);
		}
	}

	/// Set interrupt vectors for testing
	void set_interrupt_vectors(Address reset_vector, Address nmi_vector, Address irq_vector) {
		// Vectors are stored at the end of the address space
		if (prg_rom_.size() >= 0x8000) {
			// Reset vector at $FFFC-$FFFD
			prg_rom_[0x7FFC] = low_byte(reset_vector);
			prg_rom_[0x7FFD] = high_byte(reset_vector);

			// NMI vector at $FFFA-$FFFB
			prg_rom_[0x7FFA] = low_byte(nmi_vector);
			prg_rom_[0x7FFB] = high_byte(nmi_vector);

			// IRQ vector at $FFFE-$FFFF
			prg_rom_[0x7FFE] = low_byte(irq_vector);
			prg_rom_[0x7FFF] = high_byte(irq_vector);
		}
	}

	/// Enable/disable SRAM
	void set_sram_enabled(bool enabled) {
		has_sram_ = enabled;
		if (enabled) {
			sram_.fill(0x00);
		}
	}

  private:
	/// PRG ROM data (maps to $8000-$FFFF)
	std::vector<Byte> prg_rom_;

	/// SRAM data (maps to $6000-$7FFF if present)
	std::array<Byte, 0x2000> sram_{};

	/// Whether cartridge has SRAM
	bool has_sram_ = false;
};

} // namespace nes
