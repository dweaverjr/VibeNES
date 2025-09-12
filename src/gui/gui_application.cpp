#include "gui/gui_application.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <memory>

namespace nes::gui {

GuiApplication::GuiApplication()
	: window_(nullptr), gl_context_(nullptr), io_(nullptr), running_(false), show_demo_window_(false), cpu_(nullptr),
	  bus_(nullptr), cartridge_(nullptr), cpu_panel_(std::make_unique<CPUStatePanel>()),
	  disassembler_panel_(std::make_unique<DisassemblerPanel>()), memory_panel_(std::make_unique<MemoryViewerPanel>()),
	  rom_loader_panel_(std::make_unique<RomLoaderPanel>()) {
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

	// Create window
	window_ = SDL_CreateWindow("VibeNES Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
							   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

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

	// Render panels
	if (cpu_panel_->is_visible()) {
		cpu_panel_->render(cpu_);
	}

	if (disassembler_panel_->is_visible()) {
		disassembler_panel_->render(cpu_, bus_);
	}

	if (memory_panel_->is_visible()) {
		memory_panel_->render(bus_);
	}

	if (rom_loader_panel_->is_visible()) {
		rom_loader_panel_->render(cartridge_);
	}

	// Show demo window if requested
	if (show_demo_window_) {
		ImGui::ShowDemoWindow(&show_demo_window_);
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
			bool rom_loader_visible = rom_loader_panel_->is_visible();
			if (ImGui::MenuItem("Load ROM...", nullptr, rom_loader_visible)) {
				rom_loader_panel_->set_visible(!rom_loader_visible);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			bool cpu_visible = cpu_panel_->is_visible();
			bool disasm_visible = disassembler_panel_->is_visible();
			bool mem_visible = memory_panel_->is_visible();

			if (ImGui::MenuItem("CPU State", nullptr, cpu_visible)) {
				cpu_panel_->set_visible(!cpu_visible);
			}
			if (ImGui::MenuItem("Disassembler", nullptr, disasm_visible)) {
				disassembler_panel_->set_visible(!disasm_visible);
			}
			if (ImGui::MenuItem("Memory Viewer", nullptr, mem_visible)) {
				memory_panel_->set_visible(!mem_visible);
			}
			ImGui::Separator();
			ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_window_);
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
