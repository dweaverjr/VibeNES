#pragma once

#include <functional>
#include <string>

// Forward declarations
namespace nes {
class Cartridge;
}

namespace nes::gui {

/**
 * GUI panel for loading ROM files
 * Provides file browser and ROM information display
 */
class RomLoaderPanel {
  public:
	RomLoaderPanel();
	~RomLoaderPanel() = default;

	// Render the panel
	void render(nes::Cartridge *cartridge);

	// Set callback for when ROM is successfully loaded
	void set_rom_loaded_callback(std::function<void()> callback) {
		rom_loaded_callback_ = std::move(callback);
	}

	// Panel visibility
	void set_visible(bool visible) {
		visible_ = visible;
	}
	bool is_visible() const {
		return visible_;
	}

  private:
	bool visible_ = false;

	// File browser state
	std::string selected_file_;
	std::string current_directory_;
	std::string filter_extension_ = ".nes";

	// Callback for ROM loading events
	std::function<void()> rom_loaded_callback_;

	// ROM info display
	void render_file_browser(nes::Cartridge *cartridge);
	void render_rom_info(nes::Cartridge *cartridge);
	void render_load_button(nes::Cartridge *cartridge);

	// Helper functions
	bool is_nes_file(const std::string &filename) const;
	std::string get_file_extension(const std::string &filename) const;
	void find_default_rom_directory();
};

} // namespace nes::gui
