#include "gui/gui_application.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"
#include "ppu/ppu.hpp"

// Panel includes
#include "gui/panels/cpu_state_panel.hpp"
#include "gui/panels/disassembler_panel.hpp"
#include "gui/panels/memory_viewer_panel.hpp"
#include "gui/panels/ppu_viewer_panel.hpp"
#include "gui/panels/rom_loader_panel.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <memory>

namespace nes::gui {

GuiApplication::GuiApplication()
	: window_(nullptr), gl_context_(nullptr), io_(nullptr), running_(false), show_demo_window_(false),
	  show_ppu_window_(false), show_pattern_tables_window_(false), show_nametables_window_(false), cpu_(nullptr),
	  bus_(nullptr), cartridge_(nullptr), ppu_(nullptr), cpu_panel_(std::make_unique<CPUStatePanel>()),
	  disassembler_panel_(std::make_unique<DisassemblerPanel>()), memory_panel_(std::make_unique<MemoryViewerPanel>()),
	  rom_loader_panel_(std::make_unique<RomLoaderPanel>()), ppu_viewer_panel_(std::make_unique<PPUViewerPanel>()) {
}

GuiApplication::~GuiApplication() {
	shutdown();
}

bool GuiApplication::initialize() {
	if (!initialize_sdl()) {
		return false;
	}

	if (!initialize_imgui()) {
		cleanup();
		return false;
	}

	return true;
}

bool GuiApplication::initialize_sdl() {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return false;
	}

	// Set OpenGL attributes
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// Create window - Fixed layout for 1440p monitors
	window_ = SDL_CreateWindow("VibeNES - Cycle-Accurate NES Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
							   1600, 1200, SDL_WINDOW_OPENGL);

	if (!window_) {
		std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return false;
	}

	// Create OpenGL context
	gl_context_ = SDL_GL_CreateContext(window_);
	if (!gl_context_) {
		std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
		return false;
	}

	// Enable VSync
	SDL_GL_SetSwapInterval(1);

	return true;
}

bool GuiApplication::initialize_imgui() {
	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io_ = &ImGui::GetIO();
	io_->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Apply retro theme
	RetroTheme::apply_retro_style();

	// Setup Platform/Renderer backends
	if (!ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_)) {
		std::cerr << "Failed to initialize ImGui SDL2 backend" << std::endl;
		return false;
	}

	if (!ImGui_ImplOpenGL3_Init("#version 130")) {
		std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
		return false;
	}

	return true;
}

void GuiApplication::run() {
	running_ = true;

	while (running_) {
		handle_events();
		render_frame();
	}
}

void GuiApplication::handle_events() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL2_ProcessEvent(&event);

		if (event.type == SDL_QUIT) {
			running_ = false;
		}

		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
			event.window.windowID == SDL_GetWindowID(window_)) {
			running_ = false;
		}
	}
}

void GuiApplication::render_frame() {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	render_main_menu_bar();

	// Create a full-screen dockspace for the fixed layout
	ImGui::SetNextWindowPos(ImVec2(0, HEADER_HEIGHT));
	ImGui::SetNextWindowSize(
		ImVec2(static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT) - HEADER_HEIGHT));

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
									ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	if (ImGui::Begin("VibeNES Main Layout", nullptr, window_flags)) {
		// Calculate positions for the three-column layout
		const float left_start = 0.0f;
		const float center_start = LEFT_WIDTH;
		const float right_start = LEFT_WIDTH + CENTER_WIDTH;
		const float content_height = static_cast<float>(WINDOW_HEIGHT) - HEADER_HEIGHT;

		// LEFT COLUMN - ROM Loader and CPU State
		ImGui::SetCursorPos(ImVec2(left_start, 0));
		if (ImGui::BeginChild("LeftColumn", ImVec2(LEFT_WIDTH, content_height), true)) {
			// ROM Loader Section (top 1/3)
			if (ImGui::BeginChild("ROMLoaderSection", ImVec2(LEFT_WIDTH - 10, content_height * 0.33f), true)) {
				ImGui::Text("ROM LOADER");
				ImGui::Separator();
				// Render ROM loader content directly (no longer a popup)
				if (rom_loader_panel_) {
					rom_loader_panel_->render(cartridge_);
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// CPU State Section (bottom 2/3)
			if (ImGui::BeginChild("CPUStateSection", ImVec2(LEFT_WIDTH - 10, content_height * 0.66f - 15), true)) {
				ImGui::Text("CPU STATE");
				ImGui::Separator();
				if (cpu_panel_) {
					cpu_panel_->render(cpu_);
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		// CENTER COLUMN - NES Display and PPU Registers
		ImGui::SetCursorPos(ImVec2(center_start, 0));
		if (ImGui::BeginChild("CenterColumn", ImVec2(CENTER_WIDTH, content_height), true)) {
			// NES Display Section (top 2/3)
			if (ImGui::BeginChild("NESDisplaySection", ImVec2(CENTER_WIDTH - 10, content_height * 0.66f), true)) {
				ImGui::Text("NES DISPLAY");
				ImGui::Separator();
				// This will show the main game display
				ImGui::Text("Game display will go here...");
				ImGui::Text("Resolution: 256x240 NES pixels");
				ImGui::Text("Scaled to fit available space");
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// PPU Registers Section (bottom 1/3)
			if (ImGui::BeginChild("PPURegistersSection", ImVec2(CENTER_WIDTH - 10, content_height * 0.33f - 15),
								  true)) {
				ImGui::Text("PPU REGISTERS & STATUS");
				ImGui::Separator();
				if (ppu_viewer_panel_) {
					// Show just the registers content
					ppu_viewer_panel_->render_registers_only(ppu_);
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		// RIGHT COLUMN - Memory Viewer and Disassembler
		ImGui::SetCursorPos(ImVec2(right_start, 0));
		if (ImGui::BeginChild("RightColumn", ImVec2(RIGHT_WIDTH, content_height), true)) {
			// Memory Viewer Section (top 1/2)
			if (ImGui::BeginChild("MemoryViewerSection", ImVec2(RIGHT_WIDTH - 10, content_height * 0.5f), true)) {
				ImGui::Text("MEMORY VIEWER");
				ImGui::Separator();
				if (memory_panel_) {
					memory_panel_->render(bus_);
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// Disassembler Section (bottom 1/2)
			if (ImGui::BeginChild("DisassemblerSection", ImVec2(RIGHT_WIDTH - 10, content_height * 0.5f - 15), true)) {
				ImGui::Text("DISASSEMBLER");
				ImGui::Separator();
				if (disassembler_panel_) {
					disassembler_panel_->render(cpu_, bus_);
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
	}
	ImGui::End();

	ImGui::PopStyleVar(3);

	// Show demo window if requested (as overlay)
	if (show_demo_window_) {
		ImGui::ShowDemoWindow(&show_demo_window_);
	}

	// Show PPU debugging windows if requested
	if (show_ppu_window_ && ppu_viewer_panel_) {
		if (ImGui::Begin("PPU Debugger", &show_ppu_window_)) {
			ppu_viewer_panel_->render(ppu_, cartridge_);
		}
		ImGui::End();
	}

	if (show_pattern_tables_window_ && ppu_viewer_panel_) {
		if (ImGui::Begin("Pattern Tables", &show_pattern_tables_window_)) {
			ppu_viewer_panel_->render_pattern_tables(ppu_, cartridge_);
		}
		ImGui::End();
	}

	if (show_nametables_window_ && ppu_viewer_panel_) {
		if (ImGui::Begin("Nametables", &show_nametables_window_)) {
			ppu_viewer_panel_->render_nametables(ppu_);
		}
		ImGui::End();
	}

	// Rendering
	ImGui::Render();
	glViewport(0, 0, static_cast<int>(io_->DisplaySize.x), static_cast<int>(io_->DisplaySize.y));
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SDL_GL_SwapWindow(window_);
}

void GuiApplication::render_main_menu_bar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				running_ = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_window_);
			ImGui::Separator();
			ImGui::MenuItem("PPU Debugger", nullptr, &show_ppu_window_);
			ImGui::MenuItem("Pattern Tables", nullptr, &show_pattern_tables_window_);
			ImGui::MenuItem("Nametables", nullptr, &show_nametables_window_);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("About")) {
				// TODO: Show about dialog
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void GuiApplication::shutdown() {
	cleanup();
}

void GuiApplication::cleanup() {
	if (gl_context_) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_GL_DeleteContext(gl_context_);
		gl_context_ = nullptr;
	}

	if (window_) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}

	SDL_Quit();
}

} // namespace nes::gui
