#pragma once

#include "core/types.hpp"
#include <SDL2/SDL.h>
#include <memory>

struct ImGuiIO;

// Forward declarations
namespace nes {
class CPU6502;
class SystemBus;
class Cartridge;
class PPU;
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

	// Set the CPU reference for debugging
	void set_cpu(nes::CPU6502 *cpu) {
		cpu_ = cpu;
	}

	// Set the system bus reference for memory viewing
	void set_bus(nes::SystemBus *bus) {
		bus_ = bus;
	}

	// Set the cartridge reference for ROM loading
	void set_cartridge(nes::Cartridge *cartridge) {
		cartridge_ = cartridge;
	}

	// Set the PPU reference for graphics debugging
	void set_ppu(nes::PPU *ppu) {
		ppu_ = ppu;
	}

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

	// Emulator references
	nes::CPU6502 *cpu_;
	nes::SystemBus *bus_;
	nes::Cartridge *cartridge_;
	nes::PPU *ppu_;

	// GUI panels
	std::unique_ptr<CPUStatePanel> cpu_panel_;
	std::unique_ptr<DisassemblerPanel> disassembler_panel_;
	std::unique_ptr<MemoryViewerPanel> memory_panel_;
	std::unique_ptr<RomLoaderPanel> rom_loader_panel_;
	std::unique_ptr<PPUViewerPanel> ppu_viewer_panel_;
	std::unique_ptr<TimingPanel> timing_panel_;

	// Layout constants
	static constexpr int WINDOW_WIDTH = 1600;
	static constexpr int WINDOW_HEIGHT = 1200;
	static constexpr float HEADER_HEIGHT = 25.0f;
	static constexpr float LEFT_WIDTH = 350.0f;
	static constexpr float CENTER_WIDTH = 900.0f;
	static constexpr float RIGHT_WIDTH = 350.0f;

	// Private methods
	bool initialize_sdl();
	bool initialize_imgui();
	void handle_events();
	void render_frame();
	void render_main_menu_bar();
	void cleanup();

	// Emulation control
	void step_emulation();
	void step_frame();
};

} // namespace nes::gui
