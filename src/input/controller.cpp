#include "input/controller.hpp"
#include <iostream>

namespace nes {

Controller::Controller(std::shared_ptr<GamepadManager> gamepad_manager) : gamepad_manager_(std::move(gamepad_manager)) {
}

void Controller::tick(CpuCycle cycles) {
	// Controller doesn't need per-cycle timing
	(void)cycles;
}

void Controller::reset() {
	strobe_ = false;
	shift_register_1_ = 0x00;
	shift_register_2_ = 0x00;
	shift_count_1_ = 0;
	shift_count_2_ = 0;
	button_states_1_ = 0x00;
	button_states_2_ = 0x00;
}

void Controller::power_on() {
	reset();
}

const char *Controller::get_name() const noexcept {
	return "NES Controllers";
}

Byte Controller::read(Address address) const noexcept {
	if (address == 0x4016) {
		// Controller 1
		if (strobe_) {
			// While strobe is high, continuously return button A state
			return (button_states_1_ & 0x01);
		} else {
			// Return current bit from shift register
			Byte bit = (shift_register_1_ >> shift_count_1_) & 0x01;
			shift_count_1_++;
			if (shift_count_1_ > 7) {
				shift_count_1_ = 0; // Wrap around (returns 1s after 8 reads on real hardware)
			}
			return bit | 0x40; // Bit 6 is always 1 (open bus behavior)
		}
	} else if (address == 0x4017) {
		// Controller 2
		if (strobe_) {
			// While strobe is high, continuously return button A state
			return (button_states_2_ & 0x01);
		} else {
			// Return current bit from shift register
			Byte bit = (shift_register_2_ >> shift_count_2_) & 0x01;
			shift_count_2_++;
			if (shift_count_2_ > 7) {
				shift_count_2_ = 0;
			}
			return bit | 0x40; // Bit 6 is always 1 (open bus behavior)
		}
	}

	return 0x40; // Open bus
}

void Controller::write(Byte value) noexcept {
	bool new_strobe = (value & 0x01) != 0;

	// Latch button states on strobe falling edge (1 -> 0)
	if (strobe_ && !new_strobe) {
		latch_button_states();
	}

	strobe_ = new_strobe;

	// Reset shift counters when strobe goes high
	if (strobe_) {
		shift_count_1_ = 0;
		shift_count_2_ = 0;
	}
}

Byte Controller::get_button_states(int controller_index) const noexcept {
	return controller_index == 0 ? button_states_1_ : button_states_2_;
}

void Controller::latch_button_states() {
	// Read current gamepad states and latch them
	button_states_1_ = read_gamepad_state(0);
	button_states_2_ = read_gamepad_state(1);

	// Load shift registers with button states
	shift_register_1_ = button_states_1_;
	shift_register_2_ = button_states_2_;

	// Reset shift counters
	shift_count_1_ = 0;
	shift_count_2_ = 0;
}

Byte Controller::read_gamepad_state(int player_index) const noexcept {
	if (!gamepad_manager_ || !gamepad_manager_->is_controller_connected(player_index)) {
		return 0x00; // No buttons pressed if controller not connected
	}

	Byte state = 0x00;

	// Button mapping: Modern controller -> NES controller
	// A button (Xbox A, PS Cross)
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_SOUTH) ||
		gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_EAST)) {
		state |= (1 << static_cast<uint8_t>(NESButton::A));
	}

	// B button (Xbox B, PS Circle) OR X button
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_WEST) ||
		gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_NORTH)) {
		state |= (1 << static_cast<uint8_t>(NESButton::B));
	}

	// Select
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_BACK)) {
		state |= (1 << static_cast<uint8_t>(NESButton::SELECT));
	}

	// Start
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_START)) {
		state |= (1 << static_cast<uint8_t>(NESButton::START));
	}

	// D-pad Up
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
		state |= (1 << static_cast<uint8_t>(NESButton::UP));
	}

	// D-pad Down
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
		state |= (1 << static_cast<uint8_t>(NESButton::DOWN));
	}

	// D-pad Left
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
		state |= (1 << static_cast<uint8_t>(NESButton::LEFT));
	}

	// D-pad Right
	if (gamepad_manager_->is_button_pressed(player_index, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
		state |= (1 << static_cast<uint8_t>(NESButton::RIGHT));
	}

	return state;
}

} // namespace nes
