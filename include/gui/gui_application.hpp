#pragma once

#include "core/types.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>

struct ImGuiIO;

// Forward declarations
namespace nes {
class CPU6502;
class SystemBus;
class Cartridge;
class PPU;
class AudioPanel;
class GamepadManager;
class Controller;
class SaveStateManager;
} // namespace nes

namespace nes::gui {

// Forward declarations for panels
class CPUStatePanel;
class DisassemblerPanel;
class MemoryViewerPanel;
class RomLoaderPanel;
class PPUViewerPanel;
class TimingPanel;

/**
 * Main GUI application class that manages the SDL2 window and ImGui context
 * Provides the debugging interface for the NES emulator
 */
class GuiApplication {
  public:
	GuiApplication();
	~GuiApplication();

	// Initialize SDL2 and ImGui
	bool initialize();

	// Run the main application loop
	void run();

	// Shutdown and cleanup
	void shutdown();

	// Components are now internally managed - no external setters needed

	// Setup ROM loading callbacks
	void setup_callbacks();

  private:
	// SDL2 resources
	SDL_Window *window_;
	SDL_GLContext gl_context_;

	// ImGui state
	ImGuiIO *io_;

	// Application state
	bool running_;
	bool show_demo_window_;

	// Fullscreen state
	bool fullscreen_mode_;
	int fullscreen_scale_;	  // Integer scaling multiplier (calculated automatically)
	int fullscreen_offset_x_; // X offset for centering
	int fullscreen_offset_y_; // Y offset for centering

	// Emulation state
	bool emulation_running_;
	bool emulation_paused_;
	float emulation_speed_; // Speed multiplier (1.0 = normal speed)
	double cycle_accumulator_;
	std::uint64_t last_frame_counter_;
	bool frame_timer_initialized_;

	// Emulator references
	std::shared_ptr<nes::CPU6502> cpu_;
	std::shared_ptr<nes::SystemBus> bus_;
	std::shared_ptr<nes::Cartridge> cartridge_;
	std::shared_ptr<nes::PPU> ppu_;
	std::shared_ptr<nes::GamepadManager> gamepad_manager_;
	std::shared_ptr<nes::Controller> controllers_;

	// GUI panels
	std::unique_ptr<CPUStatePanel> cpu_panel_;
	std::unique_ptr<DisassemblerPanel> disassembler_panel_;
	std::unique_ptr<MemoryViewerPanel> memory_panel_;
	std::unique_ptr<RomLoaderPanel> rom_loader_panel_;
	std::unique_ptr<PPUViewerPanel> ppu_viewer_panel_;
	std::unique_ptr<TimingPanel> timing_panel_;
	std::unique_ptr<nes::AudioPanel> audio_panel_;

	// Save state manager
	std::unique_ptr<nes::SaveStateManager> save_state_manager_;
	std::string save_state_status_message_;
	float save_state_status_timer_;

	// Layout constants - Optimized for 1080p displays (1920x1080)
	static constexpr int WINDOW_WIDTH = 1176;
	static constexpr int WINDOW_HEIGHT = 1000;
	static constexpr float HEADER_HEIGHT = 25.0f;
	static constexpr float LEFT_WIDTH = 310.0f;
	static constexpr float CENTER_WIDTH = 530.0f;
	static constexpr float RIGHT_WIDTH = 336.0f;   // Pattern/Palette/Audio panel
	static constexpr float BOTTOM_HEIGHT = 300.0f; // For memory/disassembler row

	// Private methods
	bool initialize_sdl();
	bool initialize_imgui();
	void initialize_emulation_components();
	void handle_events();
	void render_frame();
	void render_main_menu_bar();
	void cleanup();

	// Fullscreen mode
	void toggle_fullscreen();
	void calculate_fullscreen_layout();
	void render_fullscreen_display();

	// Emulation control
	void step_emulation();
	void step_frame();
	void start_emulation();
	void pause_emulation();
	void toggle_run_pause();
	void process_continuous_emulation(double delta_seconds);
	bool can_run_emulation() const;
	bool is_emulation_active() const;

	// System reset
	void reset_system();

	// Save state operations
	void save_state_to_slot(int slot);
	void load_state_from_slot(int slot);
	void quick_save();
	void quick_load();
	void show_save_state_status(const std::string &message, bool success);
};

} // namespace nes::gui
