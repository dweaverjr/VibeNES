#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>

namespace nes {

/// PPU Stub - Placeholder for Picture Processing Unit
/// Implements basic register access and mirroring for Phase 1
/// Will be expanded into full PPU implementation later
class PPUStub final : public Component {
  public:
	PPUStub() = default;

	// Component interface
	void tick(CpuCycle cycles) override {
		// PPU stub doesn't need timing behavior yet
		(void)cycles;
	}

	void reset() override {
		// Clear all registers on reset
		registers_.fill(0x00);
		last_write_ = 0x00;
	}

	void power_on() override {
		// PPU registers are undefined on power-on
		registers_.fill(0x00);
		last_write_ = 0x00;
	}

	[[nodiscard]] const char *get_name() const noexcept override {
		return "PPU (Stub)";
	}

	/// Read from PPU register
	/// PPU registers are mirrored every 8 bytes from $2000-$3FFF
	[[nodiscard]] Byte read(Address address) const noexcept {
		const Address reg_addr = mirror_ppu_address(address);
		(void)reg_addr; // Suppress unused variable warning

		// Most PPU registers are write-only, return last written value
		// TODO: Implement proper read behavior for status register, etc.
		return last_write_;
	}

	/// Write to PPU register
	/// PPU registers are mirrored every 8 bytes from $2000-$3FFF
	void write(Address address, Byte value) noexcept {
		const Address reg_addr = mirror_ppu_address(address);
		registers_[reg_addr] = value;
		last_write_ = value;
	}

	/// Get register for debugging
	[[nodiscard]] Byte get_register(Address reg_index) const noexcept {
		if (reg_index < PPU_REGISTER_COUNT) {
			return registers_[reg_index];
		}
		return 0x00;
	}

  private:
	/// Mirror PPU address to register index (0-7)
	[[nodiscard]] static constexpr Address mirror_ppu_address(Address address) noexcept {
		return (address - PPU_REGISTERS_START) & 0x07; // Mirror every 8 bytes
	}

	static constexpr std::size_t PPU_REGISTER_COUNT = 8;

	/// PPU registers ($2000-$2007, mirrored through $3FFF)
	std::array<Byte, PPU_REGISTER_COUNT> registers_{};

	/// Last written value (for read behavior)
	Byte last_write_ = 0x00;
};

} // namespace nes
