#include "gui/gui_application.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"
#include "input/controller.hpp"
#include "input/gamepad_manager.hpp"
#include "memory/ram.hpp"
#include "ppu/ppu.hpp"
#include "system/save_state.hpp"

// Panel includes
#include "gui/panels/audio_panel.hpp"
#include "gui/panels/cpu_state_panel.hpp"
#include "gui/panels/disassembler_panel.hpp"
#include "gui/panels/memory_viewer_panel.hpp"
#include "gui/panels/ppu_viewer_panel.hpp"
#include "gui/panels/rom_loader_panel.hpp"
#include "gui/panels/timing_panel.hpp"

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
	  fullscreen_mode_(false), fullscreen_scale_(0), fullscreen_offset_x_(0), fullscreen_offset_y_(0),
	  emulation_running_(false), emulation_paused_(true), emulation_speed_(1.0f), cycle_accumulator_(0.0),
	  last_frame_counter_(0), frame_timer_initialized_(false), cpu_(nullptr), bus_(nullptr), cartridge_(nullptr),
	  ppu_(nullptr), cpu_panel_(std::make_unique<CPUStatePanel>()),
	  disassembler_panel_(std::make_unique<DisassemblerPanel>()), memory_panel_(std::make_unique<MemoryViewerPanel>()),
	  rom_loader_panel_(std::make_unique<RomLoaderPanel>()), ppu_viewer_panel_(std::make_unique<PPUViewerPanel>()),
	  timing_panel_(std::make_unique<TimingPanel>()), audio_panel_(std::make_unique<nes::AudioPanel>()),
	  save_state_manager_(nullptr), save_state_status_message_(""), save_state_status_timer_(0.0f) {
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

	// Initialize core emulation components
	initialize_emulation_components();

	return true;
}

bool GuiApplication::initialize_sdl() {
	// Initialize SDL with video, audio, and gamecontroller subsystems
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
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

	// Enable VSync (1 = sync to refresh rate for consistent frame pacing)
	// Standard VSync prevents the "compress/decompress" effect during scrolling
	if (SDL_GL_SetSwapInterval(1) < 0) {
		std::cerr << "Warning: Failed to enable VSync: " << SDL_GetError() << std::endl;
	} else {
		std::cout << "VSync enabled (standard mode for consistent frame pacing)\n";
	}

	// Verify VSync setting
	int swap_interval = SDL_GL_GetSwapInterval();
	std::cout << "Current swap interval: " << swap_interval << " (0=off, 1=vsync, -1=adaptive)\n";

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

void GuiApplication::initialize_emulation_components() {
	// Create components in dependency order
	bus_ = std::make_shared<nes::SystemBus>();

	// Initialize audio system
	if (bus_->initialize_audio()) {
		std::cout << "Audio system initialized successfully\n";
		// Audio will be started by the audio panel on first render to match checkbox state
	} else {
		std::cerr << "Warning: Audio system initialization failed\n";
	}

	// Initialize gamepad/controller system
	gamepad_manager_ = std::make_shared<nes::GamepadManager>();
	if (gamepad_manager_->initialize()) {
		std::cout << "Gamepad system initialized successfully\n";
		std::cout << "Connected controllers: " << gamepad_manager_->get_connected_count() << std::endl;
	} else {
		std::cerr << "Warning: Gamepad system initialization failed\n";
	}

	// Create memory components
	auto ram = std::make_shared<nes::Ram>();
	auto apu = std::make_shared<nes::APU>();

	// Create controller with gamepad manager
	controllers_ = std::make_shared<nes::Controller>(gamepad_manager_);

	// Create other core components
	cartridge_ = std::make_shared<nes::Cartridge>();
	ppu_ = std::make_shared<nes::PPU>();
	cpu_ = std::make_shared<nes::CPU6502>(bus_.get()); // CPU needs bus reference

	// Connect components to bus
	bus_->connect_ram(ram);
	bus_->connect_ppu(ppu_);
	bus_->connect_apu(apu);
	bus_->connect_controllers(controllers_);
	bus_->connect_cartridge(cartridge_);
	bus_->connect_cpu(cpu_); // Connect CPU to bus for APU IRQ handling

	// Connect cartridge to PPU for CHR ROM access
	ppu_->connect_cartridge(cartridge_);

	// Connect CPU to PPU for NMI generation
	ppu_->connect_cpu(cpu_.get());

	// Connect PPU to bus for OAM DMA and CPU memory access
	ppu_->connect_bus(bus_.get());

	// Initialize system
	bus_->power_on();

	// Set up a test reset vector for debugging (without a ROM loaded)
	bus_->write(0xFFFC, 0x00); // Reset vector low byte
	bus_->write(0xFFFD, 0x80); // Reset vector high byte -> PC will be $8000

	// Trigger a reset now that we have a proper reset vector
	cpu_->trigger_reset();

	// Create save state manager
	save_state_manager_ =
		std::make_unique<nes::SaveStateManager>(cpu_.get(), ppu_.get(), apu.get(), bus_.get(), cartridge_.get());

	printf("Emulation components initialized and connected\n");
}

void GuiApplication::run() {
	running_ = true;
	last_frame_counter_ = SDL_GetPerformanceCounter();
	frame_timer_initialized_ = true;

	std::cout << "[DEBUG] Entering main loop" << std::endl;
	std::cout.flush();
	int frame_count = 0;

	while (running_) {
		frame_count++;

		uint64_t current_counter = SDL_GetPerformanceCounter();
		double delta_seconds = 0.0;
		if (frame_timer_initialized_) {
			delta_seconds = static_cast<double>(current_counter - last_frame_counter_) /
							static_cast<double>(SDL_GetPerformanceFrequency());
		}
		last_frame_counter_ = current_counter;

		handle_events();

		// Emulation loop - run CPU and coordinate with PPU timing using real-time delta
		if (emulation_running_ && !emulation_paused_ && cpu_ && ppu_) {
			process_continuous_emulation(delta_seconds);
		}

		render_frame();
	}
}

void GuiApplication::handle_events() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// Check for window close events FIRST (before any other processing)
		if (event.type == SDL_QUIT) {
			running_ = false;
			continue;
		}

		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
			event.window.windowID == SDL_GetWindowID(window_)) {
			running_ = false;
			continue;
		}

		// Let gamepad manager handle controller events
		if (gamepad_manager_ && gamepad_manager_->handle_sdl_event(event)) {
			continue; // Event was handled, skip to next
		}

		// Let ImGui process the event
		ImGui_ImplSDL2_ProcessEvent(&event);

		// Handle hotkeys
		if (event.type == SDL_KEYDOWN) {
			// Debug: Print key press info
			std::cout << "[KeyPress] Key: " << SDL_GetKeyName(event.key.keysym.sym);
			if (io_) {
				std::cout << " | ImGui WantCapture: " << (io_->WantCaptureKeyboard ? "YES" : "NO");
			}
			std::cout << std::endl;

			// Process hotkeys even if ImGui wants keyboard (for critical functions like fullscreen/exit)
			bool shift_pressed = (SDL_GetModState() & KMOD_SHIFT) != 0;
			bool ctrl_pressed = (SDL_GetModState() & KMOD_CTRL) != 0;
			bool alt_pressed = (SDL_GetModState() & KMOD_ALT) != 0;

			// Fullscreen toggle hotkeys (always process these)
			if (event.key.keysym.sym == SDLK_F11 && !shift_pressed && !ctrl_pressed && !alt_pressed) {
				std::cout << "[Fullscreen] F11 pressed - toggling" << std::endl;
				toggle_fullscreen();
			}
			// Alt+Enter for fullscreen (alternate)
			else if (event.key.keysym.sym == SDLK_RETURN && alt_pressed && !shift_pressed && !ctrl_pressed) {
				std::cout << "[Fullscreen] Alt+Enter pressed - toggling" << std::endl;
				toggle_fullscreen();
			}
			// Escape to exit fullscreen
			else if (event.key.keysym.sym == SDLK_ESCAPE && fullscreen_mode_) {
				std::cout << "[Fullscreen] Escape pressed - exiting fullscreen" << std::endl;
				toggle_fullscreen();
			}
			// Save state hotkeys (F1-F9) - always process these
			else if (!shift_pressed && !ctrl_pressed && !alt_pressed && event.key.keysym.sym >= SDLK_F1 &&
					 event.key.keysym.sym <= SDLK_F9) {
				int slot = event.key.keysym.sym - SDLK_F1 + 1;
				std::cout << "[SaveState] Saving to slot " << slot << std::endl;
				save_state_to_slot(slot);
			}
			// Load state hotkeys (Shift+F1-F9) - always process these
			else if (shift_pressed && !ctrl_pressed && !alt_pressed && event.key.keysym.sym >= SDLK_F1 &&
					 event.key.keysym.sym <= SDLK_F9) {
				int slot = event.key.keysym.sym - SDLK_F1 + 1;
				std::cout << "[SaveState] Loading from slot " << slot << std::endl;
				load_state_from_slot(slot);
			}
			// Quick save (Ctrl+F5) - always process this
			else if (ctrl_pressed && !shift_pressed && !alt_pressed && event.key.keysym.sym == SDLK_F5) {
				std::cout << "[SaveState] Quick save" << std::endl;
				quick_save();
			}
			// Quick load (Ctrl+F8) - always process this
			else if (ctrl_pressed && !shift_pressed && !alt_pressed && event.key.keysym.sym == SDLK_F8) {
				std::cout << "[SaveState] Quick load" << std::endl;
				quick_load();
			}
		}
	}
}
void GuiApplication::render_frame() {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// Fullscreen mode: render only the NES display
	if (fullscreen_mode_) {
		render_fullscreen_display();

		// Render ImGui
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window_);
		return;
	}

	// Normal mode: render menu bar and all panels
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

		// LEFT COLUMN - ROM Loader, CPU State, and PPU Registers
		ImGui::SetCursorPos(ImVec2(left_start, 0));
		if (ImGui::BeginChild("LeftColumn", ImVec2(LEFT_WIDTH, content_height), true)) {
			// ROM Loader Section (top 40%)
			if (ImGui::BeginChild("ROMLoaderSection", ImVec2(LEFT_WIDTH - 10, content_height * 0.40f), true)) {
				ImGui::Text("ROM LOADER");
				ImGui::Separator();
				// Render ROM loader content directly (no longer a popup)
				if (rom_loader_panel_) {
					rom_loader_panel_->render(cartridge_.get());
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// CPU State Section (middle 25% - reduced since flags are now beside registers)
			if (ImGui::BeginChild("CPUStateSection", ImVec2(LEFT_WIDTH - 10, content_height * 0.25f), true)) {
				ImGui::Text("CPU STATE");
				ImGui::Separator();
				if (cpu_panel_) {
					// Pass step_emulation and reset_system as callbacks for proper coordination
					cpu_panel_->render(
						cpu_.get(), [this]() { step_emulation(); }, [this]() { reset_system(); },
						[this]() { toggle_run_pause(); }, is_emulation_active(), can_run_emulation());
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// PPU Registers Section (bottom 35% - increased by 2% to remove scrollbar)
			if (ImGui::BeginChild("PPURegistersSection", ImVec2(LEFT_WIDTH - 10, content_height * 0.35f - 20), true)) {
				ImGui::Text("PPU REGISTERS & STATUS");
				ImGui::Separator();
				if (ppu_viewer_panel_) {
					ppu_viewer_panel_->render_registers_only(ppu_.get());
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		// CENTER COLUMN - NES Display and PPU Registers
		ImGui::SetCursorPos(ImVec2(center_start, 0));
		if (ImGui::BeginChild("CenterColumn", ImVec2(CENTER_WIDTH, content_height), true)) {
			// NES Display Section (top 50%)
			if (ImGui::BeginChild("NESDisplaySection", ImVec2(CENTER_WIDTH - 10, content_height * 0.5f), true)) {
				ImGui::Text("NES DISPLAY");
				ImGui::Separator();
				// Render the actual NES display
				if (ppu_viewer_panel_) {
					ppu_viewer_panel_->render_main_display(ppu_.get());
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// Memory/Disassembly Section (top 50%)
			if (ImGui::BeginChild("MemoryDisassemblySection", ImVec2(CENTER_WIDTH - 10, content_height * 0.50f),
								  true)) {
				// Split this section in half
				float section_width = (CENTER_WIDTH - 20) / 2.0f;

				// Left half: Memory viewer
				if (ImGui::BeginChild("MemoryViewer", ImVec2(section_width, 0), true)) {
					ImGui::Text("MEMORY VIEWER");
					ImGui::Separator();
					if (memory_panel_) {
						memory_panel_->render(bus_.get());
					}
				}
				ImGui::EndChild();

				ImGui::SameLine();

				// Right half: Disassembler
				if (ImGui::BeginChild("Disassembler", ImVec2(section_width, 0), true)) {
					ImGui::Text("DISASSEMBLER");
					ImGui::Separator();
					if (disassembler_panel_) {
						disassembler_panel_->render(cpu_.get(), bus_.get());
					}
				}
				ImGui::EndChild();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		// RIGHT COLUMN - PPU Pattern Tables and Palettes
		ImGui::SetCursorPos(ImVec2(right_start, 0));
		if (ImGui::BeginChild("RightColumn", ImVec2(RIGHT_WIDTH, content_height), true)) {
			// Pattern Tables Section (top 45% - increased from 40%)
			if (ImGui::BeginChild("PatternTablesSection", ImVec2(RIGHT_WIDTH - 10, content_height * 0.45f), true)) {
				ImGui::Text("PATTERN TABLES");
				ImGui::Separator();
				if (ppu_viewer_panel_) {
					ppu_viewer_panel_->render_pattern_tables(ppu_.get(), cartridge_.get());
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// Palette Section (30% - decreased from 35%)
			if (ImGui::BeginChild("PaletteSection", ImVec2(RIGHT_WIDTH - 10, content_height * 0.30f), true)) {
				ImGui::Text("PPU PALETTES");
				ImGui::Separator();
				if (ppu_viewer_panel_) {
					ppu_viewer_panel_->render_palette_viewer(ppu_.get());
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();

			// Audio Control Section (bottom 25%)
			if (ImGui::BeginChild("AudioSection", ImVec2(RIGHT_WIDTH - 10, content_height * 0.25f - 20), true)) {
				ImGui::Text("AUDIO CONTROL");
				ImGui::Separator();
				if (audio_panel_) {
					audio_panel_->render(bus_.get());
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

	// Display save state status message as overlay
	if (save_state_status_timer_ > 0.0f) {
		ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH / 2.0f, 60.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.8f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
		if (ImGui::Begin("SaveStateStatus", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
							 ImGuiWindowFlags_NoNav)) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", save_state_status_message_.c_str());
		}
		ImGui::End();
		ImGui::PopStyleColor(2);

		// Decrease timer (approximate frame time at 60fps)
		save_state_status_timer_ -= 0.016f;
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
			// Save State submenu
			bool rom_loaded = cartridge_ && cartridge_->is_loaded();
			if (!rom_loaded) {
				ImGui::BeginDisabled();
			}

			if (ImGui::BeginMenu("Save State")) {
				for (int i = 1; i <= 9; ++i) {
					char label[32];
					snprintf(label, sizeof(label), "Slot %d (F%d)", i, i);
					if (ImGui::MenuItem(label)) {
						save_state_to_slot(i);
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Quick Save (F5)")) {
					quick_save();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Load State")) {
				for (int i = 1; i <= 9; ++i) {
					char label[64];
					bool slot_exists = save_state_manager_ && save_state_manager_->slot_exists(i);

					if (slot_exists) {
						auto timestamp = save_state_manager_->get_slot_timestamp(i);
						if (timestamp) {
							auto time_t_val = std::chrono::system_clock::to_time_t(*timestamp);
							std::tm tm_val;
							localtime_s(&tm_val, &time_t_val);
							snprintf(label, sizeof(label), "Slot %d (Shift+F%d) - %02d:%02d:%02d", i, i, tm_val.tm_hour,
									 tm_val.tm_min, tm_val.tm_sec);
						} else {
							snprintf(label, sizeof(label), "Slot %d (Shift+F%d)", i, i);
						}
					} else {
						snprintf(label, sizeof(label), "Slot %d (Shift+F%d) - Empty", i, i);
					}

					if (!slot_exists) {
						ImGui::BeginDisabled();
					}

					if (ImGui::MenuItem(label)) {
						load_state_from_slot(i);
					}

					if (!slot_exists) {
						ImGui::EndDisabled();
					}
				}
				ImGui::Separator();

				bool quick_save_exists =
					save_state_manager_ &&
					std::filesystem::exists(save_state_manager_->get_save_directory() / "quicksave.vns");
				if (!quick_save_exists) {
					ImGui::BeginDisabled();
				}
				if (ImGui::MenuItem("Quick Load (F8)")) {
					quick_load();
				}
				if (!quick_save_exists) {
					ImGui::EndDisabled();
				}

				ImGui::EndMenu();
			}

			if (!rom_loaded) {
				ImGui::EndDisabled();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				running_ = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Fullscreen", "F11", fullscreen_mode_)) {
				toggle_fullscreen();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Emulation")) {
			bool rom_loaded = cartridge_ && cartridge_->is_loaded();
			bool running = is_emulation_active();

			if (!rom_loaded) {
				ImGui::BeginDisabled();
			}

			if (ImGui::MenuItem("Start/Resume", "F5", running)) {
				start_emulation();
			}

			if (ImGui::MenuItem("Pause", "F6", !running)) {
				pause_emulation();
			}

			if (ImGui::MenuItem("Step", "F7")) {
				if (cpu_) {
					pause_emulation();
					step_emulation();
				}
			}

			if (ImGui::MenuItem("Step Frame", "F9")) {
				if (cpu_) {
					pause_emulation();
					step_frame();
				}
			}

			if (ImGui::MenuItem("Reset", "F8")) {
				if (cpu_) {
					reset_system(); // Use system-wide reset instead of just CPU reset
				}
			}

			if (!rom_loaded) {
				ImGui::EndDisabled();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_window_);
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

void GuiApplication::step_emulation() {
	if (!bus_ || !cpu_ || !ppu_)
		return;

	static int step_count = 0;
	step_count++;

	// Execute exactly one CPU instruction and get the cycles it consumed
	int cycles_consumed = cpu_->execute_instruction();

	// Advance PPU and other components by the exact number of cycles the CPU used
	// This maintains perfect cycle-accurate synchronization between CPU and PPU
	bus_->tick(cpu_cycles(cycles_consumed));
}

void GuiApplication::step_frame() {
	if (!bus_ || !cpu_ || !ppu_)
		return;

	// Run emulation for approximately one frame worth of cycles
	// NES runs at ~1.79 MHz CPU, ~60 FPS, so about 29,830 cycles per frame
	// We'll use a smaller chunk to simulate frame stepping
	bus_->tick(cpu_cycles(1000));
}

void GuiApplication::start_emulation() {
	if (!can_run_emulation()) {
		return;
	}

	emulation_running_ = true;
	emulation_paused_ = false;
	cycle_accumulator_ = 0.0;
	last_frame_counter_ = SDL_GetPerformanceCounter();
	frame_timer_initialized_ = true;
}

void GuiApplication::pause_emulation() {
	emulation_paused_ = true;
}

void GuiApplication::toggle_run_pause() {
	if (is_emulation_active()) {
		pause_emulation();
	} else {
		start_emulation();
	}
}

void GuiApplication::process_continuous_emulation(double delta_seconds) {
	if (!can_run_emulation() || delta_seconds <= 0.0) {
		return;
	}

	const double cpu_cycles_per_second = static_cast<double>(CPU_CLOCK_NTSC);
	cycle_accumulator_ += delta_seconds * cpu_cycles_per_second * static_cast<double>(emulation_speed_);
	const auto target_cycles = static_cast<std::int64_t>(cycle_accumulator_);
	if (target_cycles <= 0) {
		return;
	}

	// Safety check: Prevent infinite loops from excessive delta_seconds
	const std::int64_t MAX_CYCLES_PER_FRAME = 100000; // ~56ms worth of CPU cycles (safe upper bound)
	if (target_cycles > MAX_CYCLES_PER_FRAME) {
		std::cerr << "[WARNING] Excessive target_cycles: " << target_cycles << " (delta_seconds=" << delta_seconds
				  << "). Capping to prevent hang." << std::endl;
		cycle_accumulator_ = static_cast<double>(MAX_CYCLES_PER_FRAME);
		const auto capped_target = MAX_CYCLES_PER_FRAME;

		std::int64_t executed_cycles = 0;
		std::int64_t instruction_count = 0;
		while (executed_cycles < capped_target) {
			int consumed = cpu_->execute_instruction();
			if (consumed <= 0) {
				std::cerr << "[ERROR] CPU execute_instruction returned " << consumed << " cycles. Breaking loop."
						  << std::endl;
				break;
			}
			bus_->tick(cpu_cycles(consumed));
			executed_cycles += consumed;
			instruction_count++;

			// Safety timeout: prevent true infinite loops
			if (instruction_count > 50000) {
				std::cerr << "[CRITICAL] Executed 50000 instructions without reaching target. Possible infinite loop "
							 "detected!"
						  << std::endl;
				pause_emulation();
				break;
			}
		}
		cycle_accumulator_ = 0.0;
		return;
	}

	cycle_accumulator_ -= static_cast<double>(target_cycles);

	std::int64_t executed_cycles = 0;
	std::int64_t instruction_count = 0;

	// Debug: Periodic hang detection
	auto loop_start_time = std::chrono::steady_clock::now();

	while (executed_cycles < target_cycles) {
		// Every 5000 instructions, check if we're taking too long
		if (instruction_count > 0 && instruction_count % 5000 == 0) {
			auto now = std::chrono::steady_clock::now();
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - loop_start_time).count();
			if (elapsed_ms > 500) { // More than 500ms stuck
				std::cerr << "[HANG] Emulation loop stuck for " << elapsed_ms << "ms after " << instruction_count
						  << " instructions (" << executed_cycles << "/" << target_cycles << " cycles). PC=0x"
						  << std::hex << cpu_->get_program_counter() << std::dec << std::endl;
				pause_emulation();
				break;
			}
		}

		int consumed = cpu_->execute_instruction();
		if (consumed <= 0) {
			std::cerr << "[ERROR] CPU execute_instruction returned " << consumed << " cycles. Breaking loop."
					  << std::endl;
			break;
		}
		bus_->tick(cpu_cycles(consumed));
		executed_cycles += consumed;
		instruction_count++;

		// Safety timeout: prevent true infinite loops (50000 instructions = ~250K cycles = ~140ms)
		if (instruction_count > 50000) {
			std::cerr << "[CRITICAL] Executed 50000 instructions (" << executed_cycles << "/" << target_cycles
					  << " cycles) without reaching target. Possible infinite loop detected!" << std::endl;
			std::cerr << "[DEBUG] Last PC: 0x" << std::hex << cpu_->get_program_counter() << std::dec << std::endl;
			pause_emulation();
			break;
		}
	}

	cycle_accumulator_ += static_cast<double>(executed_cycles - target_cycles);
}

bool GuiApplication::can_run_emulation() const {
	return cpu_ && bus_ && ppu_ && cartridge_ && cartridge_->is_loaded();
}

bool GuiApplication::is_emulation_active() const {
	return emulation_running_ && !emulation_paused_;
}

void GuiApplication::reset_system() {
	if (!bus_) {
		std::cerr << "Cannot reset system: SystemBus not initialized" << std::endl;
		return;
	}

	// Reset the entire NES system through the SystemBus
	// This resets all connected components in the proper order
	bus_->reset();

	// Pause emulation after reset so user can examine the state
	pause_emulation();
	emulation_running_ = false;
	cycle_accumulator_ = 0.0;

	std::cout << "NES System Reset: All components reset to initial state" << std::endl;
}

void GuiApplication::setup_callbacks() {
	if (rom_loader_panel_ && cpu_) {
		rom_loader_panel_->set_rom_loaded_callback([this]() {
			// Reconnect cartridge to PPU to update mirroring mode from newly loaded ROM
			ppu_->connect_cartridge(cartridge_);

			// Reset the entire system (including PPU, mapper, etc.) when ROM is loaded
			// This ensures all components start in a clean state
			if (bus_) {
				bus_->reset();
			}
		});
	}
}

// =============================================================================
// Save State Implementation
// =============================================================================

void GuiApplication::save_state_to_slot(int slot) {
	if (!save_state_manager_ || !cartridge_ || !cartridge_->is_loaded()) {
		show_save_state_status("No ROM loaded!", false);
		return;
	}

	// Pause emulation during save
	bool was_running = emulation_running_ && !emulation_paused_;
	if (was_running) {
		pause_emulation();
	}

	bool success = save_state_manager_->save_to_slot(slot);

	if (success) {
		char message[64];
		snprintf(message, sizeof(message), "Saved to slot %d", slot);
		show_save_state_status(message, true);
	} else {
		const std::string &error = save_state_manager_->get_last_error();
		show_save_state_status("Save failed: " + error, false);
	}

	// Resume if it was running
	if (was_running) {
		start_emulation();
	}
}

void GuiApplication::load_state_from_slot(int slot) {
	if (!save_state_manager_ || !cartridge_ || !cartridge_->is_loaded()) {
		show_save_state_status("No ROM loaded!", false);
		return;
	}

	if (!save_state_manager_->slot_exists(slot)) {
		char message[64];
		snprintf(message, sizeof(message), "Slot %d is empty", slot);
		show_save_state_status(message, false);
		return;
	}

	// Pause emulation during load
	bool was_running = emulation_running_ && !emulation_paused_;
	if (was_running) {
		pause_emulation();
	}

	bool success = save_state_manager_->load_from_slot(slot);

	if (success) {
		char message[64];
		snprintf(message, sizeof(message), "Loaded from slot %d", slot);
		show_save_state_status(message, true);
	} else {
		const std::string &error = save_state_manager_->get_last_error();
		show_save_state_status("Load failed: " + error, false);
	}

	// Resume if it was running
	if (was_running) {
		start_emulation();
	}
}

void GuiApplication::quick_save() {
	if (!save_state_manager_ || !cartridge_ || !cartridge_->is_loaded()) {
		show_save_state_status("No ROM loaded!", false);
		return;
	}

	bool was_running = emulation_running_ && !emulation_paused_;
	if (was_running) {
		pause_emulation();
	}

	bool success = save_state_manager_->quick_save();

	if (success) {
		show_save_state_status("Quick save successful", true);
	} else {
		const std::string &error = save_state_manager_->get_last_error();
		show_save_state_status("Quick save failed: " + error, false);
	}

	if (was_running) {
		start_emulation();
	}
}

void GuiApplication::quick_load() {
	if (!save_state_manager_ || !cartridge_ || !cartridge_->is_loaded()) {
		show_save_state_status("No ROM loaded!", false);
		return;
	}

	bool was_running = emulation_running_ && !emulation_paused_;
	if (was_running) {
		pause_emulation();
	}

	bool success = save_state_manager_->quick_load();

	if (success) {
		show_save_state_status("Quick load successful", true);
	} else {
		const std::string &error = save_state_manager_->get_last_error();
		show_save_state_status("Quick load failed: " + error, false);
	}

	if (was_running) {
		start_emulation();
	}
}

void GuiApplication::show_save_state_status(const std::string &message, bool success) {
	save_state_status_message_ = message;
	save_state_status_timer_ = 3.0f; // Show for 3 seconds

	// Print to console as well
	if (success) {
		std::cout << "[Save State] " << message << std::endl;
	} else {
		std::cerr << "[Save State] " << message << std::endl;
	}
}

void GuiApplication::toggle_fullscreen() {
	fullscreen_mode_ = !fullscreen_mode_;

	if (fullscreen_mode_) {
		// Enter borderless fullscreen
		SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
		calculate_fullscreen_layout();
		std::cout << "[Fullscreen] Enabled - Scale: " << fullscreen_scale_ << "x (" << (256 * fullscreen_scale_) << "x"
				  << (240 * fullscreen_scale_) << ")" << std::endl;
	} else {
		// Exit fullscreen - return to windowed mode
		SDL_SetWindowFullscreen(window_, 0);
		std::cout << "[Fullscreen] Disabled" << std::endl;
	}
}

void GuiApplication::calculate_fullscreen_layout() {
	// Get current display dimensions
	SDL_DisplayMode display_mode;
	SDL_GetCurrentDisplayMode(0, &display_mode);

	const int screen_width = display_mode.w;
	const int screen_height = display_mode.h;

	// NES native resolution
	const int nes_width = 256;
	const int nes_height = 240;

	// Calculate maximum integer scale that fits
	const int max_scale_x = screen_width / nes_width;
	const int max_scale_y = screen_height / nes_height;
	fullscreen_scale_ = std::min(max_scale_x, max_scale_y);

	// Ensure at least 1x scale
	if (fullscreen_scale_ < 1) {
		fullscreen_scale_ = 1;
	}

	// Calculate actual display size
	const int display_width = nes_width * fullscreen_scale_;
	const int display_height = nes_height * fullscreen_scale_;

	// Center the display
	fullscreen_offset_x_ = (screen_width - display_width) / 2;
	fullscreen_offset_y_ = (screen_height - display_height) / 2;
}

void GuiApplication::render_fullscreen_display() {
	if (!ppu_ || !ppu_viewer_panel_) {
		return;
	}

	// Update the PPU display texture WITHOUT rendering any UI windows
	ppu_viewer_panel_->update_display_texture_only(ppu_.get());

	// Clear to black for letterboxing
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Create a fullscreen ImGui window with no decorations
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(
		ImVec2(static_cast<float>(ImGui::GetIO().DisplaySize.x), static_cast<float>(ImGui::GetIO().DisplaySize.y)));

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
									ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
									ImGuiWindowFlags_NoDecoration;

	if (ImGui::Begin("FullscreenDisplay", nullptr, window_flags)) {
		// Get the NES display texture from PPU viewer panel
		unsigned int texture_id = ppu_viewer_panel_->get_main_display_texture();

		if (texture_id != 0) {
			// Calculate display size and position
			ImVec2 display_size(static_cast<float>(256 * fullscreen_scale_),
								static_cast<float>(240 * fullscreen_scale_));
			ImVec2 display_pos(static_cast<float>(fullscreen_offset_x_), static_cast<float>(fullscreen_offset_y_));

			// Set cursor position for centering
			ImGui::SetCursorPos(display_pos);

			// Render the texture with integer scaling
			ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(texture_id)), display_size);
		}
	}
	ImGui::End();
}

} // namespace nes::gui
