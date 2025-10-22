#pragma once

#include "core/types.hpp"
#include <SDL2/SDL.h>
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

	// Layout constants - Optimized for 1080p displays (1920x1080)
	static constexpr int WINDOW_WIDTH = 1900;
	static constexpr int WINDOW_HEIGHT = 1000;
	static constexpr float HEADER_HEIGHT = 25.0f;
	static constexpr float LEFT_WIDTH = 300.0f;
	static constexpr float CENTER_WIDTH = 760.0f;
	static constexpr float RIGHT_WIDTH = 840.0f; // Fill remaining space (1900 - 300 - 760 = 840)
	static constexpr float BOTTOM_HEIGHT = 300.0f; // For memory/disassembler row

	// Private methods
	bool initialize_sdl();
	bool initialize_imgui();
	void initialize_emulation_components();
	void handle_events();
	void render_frame();
	void render_main_menu_bar();
	void cleanup();

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
};

} // namespace nes::gui
