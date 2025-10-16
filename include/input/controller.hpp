#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include "input/gamepad_manager.hpp"
#include <memory>

namespace nes {

/**
 * NES Controller button bits
 * Standard NES controller has 8 buttons read in this order:
 * A, B, Select, Start, Up, Down, Left, Right
 */
enum class NESButton : uint8_t {
	A = 0,		// Bit 0
	B = 1,		// Bit 1
	SELECT = 2, // Bit 2
	START = 3,	// Bit 3
	UP = 4,		// Bit 4
	DOWN = 5,	// Bit 5
	LEFT = 6,	// Bit 6
	RIGHT = 7	// Bit 7
};

/**
 * Controller - NES controller input handling
 *
 * Emulates the standard NES controller with 8 buttons:
 * - A, B, Select, Start
 * - D-pad: Up, Down, Left, Right
 *
 * Hardware behavior:
 * - $4016 bit 0 = Controller 1 data (serial read, one bit per read)
 * - $4016 bit 1 = Strobe (latch button states when set to 1)
 * - $4017 bit 0 = Controller 2 data
 *
 * Reading sequence:
 * 1. Write $01 to $4016 (strobe high - start reading)
 * 2. Write $00 to $4016 (strobe low - latch button states)
 * 3. Read $4016 8 times to get button states in order: A, B, Select, Start, Up, Down, Left, Right
 */
class Controller final : public Component {
  public:
	/**
	 * Constructor
	 * @param gamepad_manager Shared gamepad manager for reading input
	 */
	explicit Controller(std::shared_ptr<GamepadManager> gamepad_manager);

	// Component interface
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	[[nodiscard]] const char *get_name() const noexcept override;

	/**
	 * Read from controller port ($4016 or $4017)
	 * Returns the next bit from the shift register
	 * @param address Memory address (0x4016 for controller 1, 0x4017 for controller 2)
	 * @return Bit 0 = button state (0 or 1), bits 1-7 = open bus
	 */
	[[nodiscard]] Byte read(Address address) const noexcept;

	/**
	 * Write to controller strobe register ($4016)
	 * Bit 0 = strobe signal to latch button states
	 * @param value Value to write
	 */
	void write(Byte value) noexcept;

	/**
	 * Get current button states for debugging
	 * @param controller_index 0 for controller 1, 1 for controller 2
	 * @return 8-bit button state mask
	 */
	[[nodiscard]] Byte get_button_states(int controller_index) const noexcept;

  private:
	std::shared_ptr<GamepadManager> gamepad_manager_;

	// Controller state
	bool strobe_ = false;				   // Strobe latch signal
	mutable Byte shift_register_1_ = 0x00; // Controller 1 shift register
	mutable Byte shift_register_2_ = 0x00; // Controller 2 shift register
	mutable int shift_count_1_ = 0;		   // Current bit position for controller 1
	mutable int shift_count_2_ = 0;		   // Current bit position for controller 2

	// Latched button states
	Byte button_states_1_ = 0x00; // Controller 1 button states
	Byte button_states_2_ = 0x00; // Controller 2 button states

	// Helper methods
	void latch_button_states();
	[[nodiscard]] Byte read_gamepad_state(int player_index) const noexcept;
};

} // namespace nes
