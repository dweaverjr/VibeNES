#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <vector>

namespace nes {

/**
 * GamepadManager - SDL3 gamepad detection and management
 *
 * Handles:
 * - Automatic detection of connected game controllers
 * - Hot-plugging support (connect/disconnect at runtime)
 * - Button state reading
 * - Multiple controller support (Player 1 and Player 2)
 */
class GamepadManager {
  public:
	GamepadManager();
	~GamepadManager();

	// Delete copy/move to prevent double-free of SDL resources
	GamepadManager(const GamepadManager &) = delete;
	GamepadManager &operator=(const GamepadManager &) = delete;
	GamepadManager(GamepadManager &&) = delete;
	GamepadManager &operator=(GamepadManager &&) = delete;

	/**
	 * Initialize SDL gamepad subsystem
	 * @return true if successful
	 */
	bool initialize();

	/**
	 * Shutdown and cleanup SDL gamepad resources
	 */
	void shutdown();

	/**
	 * Update gamepad states (call once per frame)
	 * Handles hot-plug events
	 */
	void update();

	/**
	 * Handle SDL controller events from main event loop
	 * @param event SDL event to process
	 * @return true if event was handled (controller-related)
	 */
	bool handle_sdl_event(const SDL_Event &event);

	/**
	 * Check if a controller is connected for a player
	 * @param player_index 0 for Player 1, 1 for Player 2
	 * @return true if controller is connected
	 */
	bool is_controller_connected(int player_index) const;

	/**
	 * Get the name of the connected controller
	 * @param player_index 0 for Player 1, 1 for Player 2
	 * @return Controller name or empty string if not connected
	 */
	const char *get_controller_name(int player_index) const;

	/**
	 * Check if a button is currently pressed
	 * @param player_index 0 for Player 1, 1 for Player 2
	 * @param button SDL gamepad button code
	 * @return true if button is pressed
	 */
	bool is_button_pressed(int player_index, SDL_GamepadButton button) const;

	/**
	 * Get number of connected controllers
	 */
	int get_connected_count() const;

  private:
	struct ControllerInfo {
		SDL_Gamepad *gamepad = nullptr;
		SDL_JoystickID instance_id = 0;
		bool connected = false;
	};

	std::vector<ControllerInfo> controllers_; // Support up to 4 controllers
	bool initialized_ = false;

	// Helper methods
	void scan_for_controllers();
	void add_controller(SDL_JoystickID instance_id);
	void remove_controller(SDL_JoystickID instance_id);
};

} // namespace nes
