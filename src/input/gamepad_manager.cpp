#include "input/gamepad_manager.hpp"

namespace nes {

GamepadManager::GamepadManager() {
	// Reserve space for up to 4 controllers
	controllers_.resize(4);
}

GamepadManager::~GamepadManager() {
	shutdown();
}

bool GamepadManager::initialize() {
	if (initialized_) {
		return true;
	}

	// SDL3: gamepad subsystem is part of SDL_INIT_GAMEPAD (initialized externally)
	if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
			return false;
		}
	}

	// Scan for already connected controllers
	scan_for_controllers();

	initialized_ = true;
	return true;
}

void GamepadManager::shutdown() {
	if (!initialized_) {
		return;
	}

	// Close all open controllers
	for (auto &info : controllers_) {
		if (info.gamepad) {
			SDL_CloseGamepad(info.gamepad);
			info.gamepad = nullptr;
			info.connected = false;
		}
	}

	initialized_ = false;
}

void GamepadManager::update() {
	// This method is intentionally minimal
	// Controller connect/disconnect events are handled in handle_sdl_event()
	// Button states are queried on-demand via is_button_pressed()
}

bool GamepadManager::handle_sdl_event(const SDL_Event &event) {
	if (!initialized_) {
		return false;
	}

	switch (event.type) {
	case SDL_EVENT_GAMEPAD_ADDED:
		add_controller(event.gdevice.which);
		return true;

	case SDL_EVENT_GAMEPAD_REMOVED:
		remove_controller(event.gdevice.which);
		return true;

	default:
		return false;
	}
}

bool GamepadManager::is_controller_connected(int player_index) const {
	if (player_index < 0 || player_index >= static_cast<int>(controllers_.size())) {
		return false;
	}
	return controllers_[player_index].connected;
}

const char *GamepadManager::get_controller_name(int player_index) const {
	if (!is_controller_connected(player_index)) {
		return "";
	}
	return SDL_GetGamepadName(controllers_[player_index].gamepad);
}

bool GamepadManager::is_button_pressed(int player_index, SDL_GamepadButton button) const {
	if (!is_controller_connected(player_index)) {
		return false;
	}

	return SDL_GetGamepadButton(controllers_[player_index].gamepad, button);
}

int GamepadManager::get_connected_count() const {
	int count = 0;
	for (const auto &info : controllers_) {
		if (info.connected) {
			count++;
		}
	}
	return count;
}

void GamepadManager::scan_for_controllers() {
	int num_gamepads = 0;
	SDL_JoystickID *gamepads = SDL_GetGamepads(&num_gamepads);

	if (gamepads) {
		for (int i = 0; i < num_gamepads; i++) {
			add_controller(gamepads[i]);
		}
		SDL_free(gamepads);
	}
}

void GamepadManager::add_controller(SDL_JoystickID instance_id) {
	// Find first available slot
	int slot = -1;
	for (size_t i = 0; i < controllers_.size(); i++) {
		if (!controllers_[i].connected) {
			slot = static_cast<int>(i);
			break;
		}
	}

	if (slot == -1) {
		return;
	}

	SDL_Gamepad *gamepad = SDL_OpenGamepad(instance_id);
	if (!gamepad) {
		return;
	}

	controllers_[slot].gamepad = gamepad;
	controllers_[slot].instance_id = instance_id;
	controllers_[slot].connected = true;
}

void GamepadManager::remove_controller(SDL_JoystickID instance_id) {
	for (auto &info : controllers_) {
		if (info.connected && info.instance_id == instance_id) {
			SDL_CloseGamepad(info.gamepad);
			info.gamepad = nullptr;
			info.instance_id = 0;
			info.connected = false;
			break;
		}
	}
}

} // namespace nes
