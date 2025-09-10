#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include <array>

namespace nes {

/// APU Stub - Placeholder for Audio Processing Unit
/// Implements basic register access for Phase 1
/// Will be expanded into full APU implementation later
class APUStub final : public Component {
  public:
	APUStub() = default;

	// Component interface
	void tick(CpuCycle cycles) override {
		// APU stub doesn't need timing behavior yet
		(void)cycles;
	}

	void reset() override {
		// Clear all registers on reset
		registers_.fill(0x00);
		// APU is silenced on reset
		status_register_ = 0x00;
	}

	void power_on() override {
		// APU registers are cleared on power-on
		registers_.fill(0x00);
		status_register_ = 0x00;
	}

	[[nodiscard]] const char *get_name() const noexcept override {
		return "APU (Stub)";
	}

	/// Read from APU register
	[[nodiscard]] Byte read(Address address) const noexcept {
		// Only $4015 (status register) is readable
		if (address == 0x4015) {
			return status_register_;
		}

		// All other APU registers are write-only
		return 0x00; // Open bus behavior
	}

	/// Write to APU register
	void write(Address address, Byte value) noexcept {
		if (address >= APU_REGISTERS_START && address <= APU_REGISTERS_END) {
			const Address reg_index = address - APU_REGISTERS_START;
			registers_[reg_index] = value;

			// Special handling for status register
			if (address == 0x4015) {
				status_register_ = value;
			}
		}
	}

	/// Get register for debugging
	[[nodiscard]] Byte get_register(Address address) const noexcept {
		if (address >= APU_REGISTERS_START && address <= APU_REGISTERS_END) {
			const Address reg_index = address - APU_REGISTERS_START;
			return registers_[reg_index];
		}
		return 0x00;
	}

  private:
	static constexpr Address APU_REGISTERS_START = 0x4000;
	static constexpr Address APU_REGISTERS_END = 0x401F;
	static constexpr std::size_t APU_REGISTER_COUNT = 0x20; // $4000-$401F

	/// APU registers ($4000-$401F)
	std::array<Byte, APU_REGISTER_COUNT> registers_{};

	/// APU status register ($4015) - readable
	Byte status_register_ = 0x00;
};

} // namespace nes
