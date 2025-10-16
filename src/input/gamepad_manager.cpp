#include "input/gamepad_manager.hpp"
#include <iostream>

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

	// Initialize SDL gamepad subsystem if not already initialized
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
			std::cerr << "Failed to initialize SDL gamepad subsystem: " << SDL_GetError() << std::endl;
			return false;
		}
	}

	std::cout << "SDL Gamepad subsystem initialized" << std::endl;

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
		if (info.controller) {
			SDL_GameControllerClose(info.controller);
			info.controller = nullptr;
			info.connected = false;
		}
	}

	initialized_ = false;
	std::cout << "SDL Gamepad subsystem shutdown" << std::endl;
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
	case SDL_CONTROLLERDEVICEADDED:
		add_controller(event.cdevice.which);
		return true;

	case SDL_CONTROLLERDEVICEREMOVED:
		remove_controller(event.cdevice.which);
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
	return SDL_GameControllerName(controllers_[player_index].controller);
}

bool GamepadManager::is_button_pressed(int player_index, SDL_GameControllerButton button) const {
	if (!is_controller_connected(player_index)) {
		return false;
	}

	return SDL_GameControllerGetButton(controllers_[player_index].controller, button) == 1;
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
	int num_joysticks = SDL_NumJoysticks();
	std::cout << "Scanning for controllers... Found " << num_joysticks << " joystick(s)" << std::endl;

	for (int i = 0; i < num_joysticks; i++) {
		if (SDL_IsGameController(i)) {
			add_controller(i);
		}
	}
}

void GamepadManager::add_controller(int device_index) {
	// Find first available slot
	int slot = -1;
	for (size_t i = 0; i < controllers_.size(); i++) {
		if (!controllers_[i].connected) {
			slot = static_cast<int>(i);
			break;
		}
	}

	if (slot == -1) {
		std::cout << "Cannot add controller: all slots occupied" << std::endl;
		return;
	}

	SDL_GameController *controller = SDL_GameControllerOpen(device_index);
	if (!controller) {
		std::cerr << "Failed to open game controller " << device_index << ": " << SDL_GetError() << std::endl;
		return;
	}

	controllers_[slot].controller = controller;
	controllers_[slot].joystick_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
	controllers_[slot].connected = true;

	std::cout << "Controller connected in slot " << slot << ": " << SDL_GameControllerName(controller) << std::endl;
}

void GamepadManager::remove_controller(SDL_JoystickID joystick_id) {
	for (auto &info : controllers_) {
		if (info.connected && info.joystick_id == joystick_id) {
			std::cout << "Controller disconnected: " << SDL_GameControllerName(info.controller) << std::endl;
			SDL_GameControllerClose(info.controller);
			info.controller = nullptr;
			info.joystick_id = -1;
			info.connected = false;
			break;
		}
	}
}

} // namespace nes
