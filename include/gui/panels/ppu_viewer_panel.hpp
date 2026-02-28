#pragma once

#include "core/types.hpp"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <SDL3/SDL.h>
#include <memory>

// Forward declarations
namespace nes {
class PPU;
class Cartridge;
} // namespace nes

namespace nes::gui {

/**
 * PPU Display Modes for different visualization types
 */
enum class PPUDisplayMode {
	FRAME_COMPLETE, // Show complete frames only (hardware accurate)
	REAL_TIME,		// Show real-time updates as PPU renders
	SCANLINE_STEP	// Show scanline-by-scanline rendering
};

/**
 * Comprehensive PPU visualization panel
 * Shows NES video output, pattern tables, nametables, sprites, and PPU state
 */
class PPUViewerPanel {
  public:
	PPUViewerPanel();
	~PPUViewerPanel();

	// Main render method
	void render(nes::PPU *ppu, nes::Cartridge *cartridge);

	// Render just the PPU registers for embedded display
	void render_registers_only(nes::PPU *ppu);

	// Render individual debug components in separate windows
	void render_pattern_tables(nes::PPU *ppu, nes::Cartridge *cartridge);
	void render_nametables(nes::PPU *ppu);
	void render_palette_viewer(nes::PPU *ppu);
	void render_sprite_viewer(nes::PPU *ppu);

	// Render the main NES display
	void render_main_display(nes::PPU *ppu);

	// Show/hide panel
	void set_visible(bool visible) {
		visible_ = visible;
	}
	bool is_visible() const {
		return visible_;
	}

	// Display mode control
	void set_display_mode(PPUDisplayMode mode) {
		display_mode_ = mode;
	}
	PPUDisplayMode get_display_mode() const {
		return display_mode_;
	}

	// Force refresh of pattern tables (useful when ROM is changed/unloaded)
	void refresh_pattern_tables() {
		pattern_table_dirty_ = true;
	}

	// Get texture ID for external rendering (fullscreen mode)
	GLuint get_main_display_texture() const {
		return main_display_texture_;
	}

	// Update texture without rendering UI (for fullscreen mode)
	void update_display_texture_only(nes::PPU *ppu);

  private:
	bool visible_;
	PPUDisplayMode display_mode_;

	// OpenGL textures for visualization
	GLuint main_display_texture_;  // 256x240 NES output
	GLuint pattern_table_texture_; // Pattern tables visualization
	GLuint nametable_texture_;	   // Nametables visualization

	// Texture data buffers
	std::unique_ptr<uint32_t[]> pattern_table_buffer_;
	std::unique_ptr<uint32_t[]> nametable_buffer_;

	// Panel state
	int selected_pattern_table_; // 0 or 1
	int selected_nametable_;	 // 0-3
	int selected_palette_;		 // 0-7
	float display_scale_;		 // Display scaling factor
	bool pattern_table_dirty_;	 // Flag to track when to regenerate pattern table

	// Rendering methods
	void render_display_controls();
	void render_ppu_registers(nes::PPU *ppu);
	void render_timing_info(nes::PPU *ppu);

	// Helper methods
	void initialize_textures();
	void cleanup_textures();
	void update_main_display_texture(const uint32_t *frame_buffer);
	void update_pattern_table_texture();
	void generate_pattern_table_visualization(nes::PPU *ppu, nes::Cartridge *cartridge);
	void generate_nametable_visualization(nes::PPU *ppu);
	uint32_t get_pattern_pixel_color(uint8_t pixel_value, uint8_t palette_index, nes::PPU *ppu);

	// Texture management
	bool textures_initialized_;
};

} // namespace nes::gui
