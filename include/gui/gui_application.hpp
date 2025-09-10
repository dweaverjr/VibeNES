#pragma once

#include "core/types.hpp"
#include "gui/panels/cpu_state_panel.hpp"
#include "gui/panels/disassembler_panel.hpp"
#include "gui/panels/memory_viewer_panel.hpp"
#include <SDL.h>

struct ImGuiIO;

// Forward declarations
namespace nes {
class CPU6502;
class SystemBus;
} // namespace nes

namespace nes::gui {

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

	// Set the CPU reference for debugging
	void set_cpu(const nes::CPU6502 *cpu) {
		cpu_ = cpu;
	}

	// Set the system bus reference for memory viewing
	void set_bus(const nes::SystemBus *bus) {
		bus_ = bus;
	}

  private:
	// SDL2 resources
	SDL_Window *window_;
	SDL_GLContext gl_context_;

	// ImGui state
	ImGuiIO *io_;

	// Application state
	bool running_;
	bool show_demo_window_;

	// Emulator references
	const nes::CPU6502 *cpu_;
	const nes::SystemBus *bus_;

	// GUI panels
	std::unique_ptr<CPUStatePanel> cpu_panel_;
	std::unique_ptr<DisassemblerPanel> disassembler_panel_;
	std::unique_ptr<MemoryViewerPanel> memory_panel_;

	// Private methods
	bool initialize_sdl();
	bool initialize_imgui();
	void handle_events();
	void render_frame();
	void render_main_menu_bar();
	void cleanup();
};

} // namespace nes::gui
