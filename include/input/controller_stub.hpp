#pragma once

#include "core/component.hpp"
#include "core/types.hpp"

namespace nes {

/// Controller Stub - Placeholder for input handling
/// Implements basic controller register access for Phase 1
/// Will be expanded into full controller implementation later
class ControllerStub final : public Component {
  public:
	ControllerStub() = default;

	// Component interface
	void tick(CpuCycle cycles) override {
		// Controller stub doesn't need timing behavior yet
		(void)cycles;
	}

	void reset() override {
		// Reset controller state
		strobe_latch_ = false;
		shift_register_1_ = 0x00;
		shift_register_2_ = 0x00;
	}

	void power_on() override {
		// Clear controller state on power-on
		strobe_latch_ = false;
		shift_register_1_ = 0x00;
		shift_register_2_ = 0x00;
	}

	[[nodiscard]] const char *get_name() const noexcept override {
		return "Controllers (Stub)";
	}

	/// Read from controller port
	[[nodiscard]] Byte read(Address address) const noexcept {
		switch (address) {
		case 0x4016: // Controller 1
			// Return next bit from shift register
			// TODO: Implement proper controller reading
			return 0x40; // No buttons pressed
			
		case 0x4017: // Controller 2
			// Return next bit from shift register
			// TODO: Implement proper controller reading
			return 0x40; // No buttons pressed
			
		default:
			return 0x00;
		}
	}

	/// Write to controller port
	void write(Address address, Byte value) noexcept {
		if (address == 0x4016) {
			// Controller strobe - latch current button states
			strobe_latch_ = (value & 0x01) != 0;
			
			if (strobe_latch_) {
				// Reset shift registers when strobing
				shift_register_1_ = 0x00; // TODO: Load actual button states
				shift_register_2_ = 0x00; // TODO: Load actual button states
			}
		}
	}

	/// Set button state for testing
	void set_button_state(int controller, Byte button_mask) noexcept {
		if (controller == 0) {
			shift_register_1_ = button_mask;
		} else if (controller == 1) {
			shift_register_2_ = button_mask;
		}
	}

  private:
	bool strobe_latch_ = false;
	Byte shift_register_1_ = 0x00; // Controller 1 button states
	Byte shift_register_2_ = 0x00; // Controller 2 button states
};

} // namespace nes
