#include "gui/panels/ppu_viewer_panel.hpp"
#include "cartridge/cartridge.hpp"
#include "gui/style/retro_theme.hpp"
#include "ppu/nes_palette.hpp"
#include "ppu/ppu.hpp"
#include <GL/gl.h>
#include <cstring>
#include <imgui.h>

namespace nes::gui {

PPUViewerPanel::PPUViewerPanel()
	: visible_(true), display_mode_(PPUDisplayMode::FRAME_COMPLETE), main_display_texture_(0),
	  pattern_table_texture_(0), nametable_texture_(0), selected_pattern_table_(0), selected_nametable_(0),
	  selected_palette_(0), display_scale_(2.0f), pattern_table_dirty_(true), textures_initialized_(false) {

	// Allocate buffers for texture data
	pattern_table_buffer_ = std::make_unique<uint32_t[]>(256 * 128); // 2 pattern tables side by side
	nametable_buffer_ = std::make_unique<uint32_t[]>(512 * 480);	 // 4 nametables in 2x2 grid
}

PPUViewerPanel::~PPUViewerPanel() {
	cleanup_textures();
}

void PPUViewerPanel::render(nes::PPU *ppu, nes::Cartridge *cartridge) {
	if (!ppu)
		return;

	// Initialize textures on first render
	if (!textures_initialized_) {
		initialize_textures();
	}

	// Render the full tabbed interface (for floating window mode)
	// Display mode controls at the top
	render_display_controls();
	ImGui::Separator();

	// Create tabs for different views
	if (ImGui::BeginTabBar("PPUTabs")) {

		// Main Display Tab
		if (ImGui::BeginTabItem("Display")) {
			render_main_display(ppu);
			ImGui::EndTabItem();
		}

		// Pattern Tables Tab
		if (ImGui::BeginTabItem("Pattern Tables")) {
			render_pattern_tables(ppu, cartridge);
			ImGui::EndTabItem();
		}

		// Nametables Tab
		if (ImGui::BeginTabItem("Nametables")) {
			render_nametables(ppu);
			ImGui::EndTabItem();
		}

		// Palettes Tab
		if (ImGui::BeginTabItem("Palettes")) {
			render_palette_viewer(ppu);
			ImGui::EndTabItem();
		}

		// Sprites Tab
		if (ImGui::BeginTabItem("Sprites")) {
			render_sprite_viewer(ppu);
			ImGui::EndTabItem();
		}

		// Registers Tab
		if (ImGui::BeginTabItem("Registers")) {
			render_ppu_registers(ppu);
			render_timing_info(ppu);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void PPUViewerPanel::render_registers_only(nes::PPU *ppu) {
	if (!ppu)
		return;

	// Just render the PPU registers content without window or tabs
	render_ppu_registers(ppu);
	ImGui::Separator();
	render_timing_info(ppu);
}
void PPUViewerPanel::render_display_controls() {
	ImGui::Text("Display Mode:");
	ImGui::SameLine();

	if (ImGui::RadioButton("Frame Complete", display_mode_ == PPUDisplayMode::FRAME_COMPLETE)) {
		display_mode_ = PPUDisplayMode::FRAME_COMPLETE;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Real-time", display_mode_ == PPUDisplayMode::REAL_TIME)) {
		display_mode_ = PPUDisplayMode::REAL_TIME;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Scanline Step", display_mode_ == PPUDisplayMode::SCANLINE_STEP)) {
		display_mode_ = PPUDisplayMode::SCANLINE_STEP;
	}

	ImGui::SameLine();
	ImGui::Text("Scale:");
	ImGui::SameLine();
	ImGui::SliderFloat("##scale", &display_scale_, 1.0f, 4.0f, "%.1fx");
}

void PPUViewerPanel::render_main_display(nes::PPU *ppu) {
	ImGui::Text("NES Video Output (256x240)");

	// Check if we should update the display based on mode
	bool should_update = false;
	switch (display_mode_) {
	case PPUDisplayMode::FRAME_COMPLETE:
		should_update = ppu->is_frame_ready();
		break;
	case PPUDisplayMode::REAL_TIME:
		should_update = true; // Always update
		break;
	case PPUDisplayMode::SCANLINE_STEP:
		// Update if we're on a new scanline
		should_update = true; // Simplified for now
		break;
	}

	if (should_update && ppu->get_frame_buffer()) {
		update_main_display_texture(ppu->get_frame_buffer());
	}

	// Display the texture
	if (main_display_texture_ != 0) {
		ImVec2 display_size(256 * display_scale_, 240 * display_scale_);
		ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(main_display_texture_)), display_size);

		// Show current frame info
		ImGui::Text("Frame: %llu, Scanline: %d, Cycle: %d", ppu->get_frame_count(), ppu->get_current_scanline(),
					ppu->get_current_cycle());
	} else {
		ImGui::Text("No frame data available");
	}
}

void PPUViewerPanel::render_pattern_tables(nes::PPU *ppu, nes::Cartridge *cartridge) {
	// Ensure textures are initialized before we try to use them
	if (!textures_initialized_) {
		initialize_textures();
	}

	ImGui::Text("Pattern Tables (CHR ROM/RAM)");

	// Constrain content to available width
	float content_width = ImGui::GetContentRegionAvail().x;

	// Pattern table selection - keep on same line if space allows
	ImGui::Text("Pattern Table:");
	ImGui::SameLine();
	ImGui::RadioButton("$0000", &selected_pattern_table_, 0);
	ImGui::SameLine();
	ImGui::RadioButton("$1000", &selected_pattern_table_, 1);

	// Palette selection for visualization - constrain slider width
	ImGui::Text("Visualization Palette:");
	ImGui::SetNextItemWidth(std::min(150.0f, content_width * 0.6f));
	ImGui::SliderInt("##palette", &selected_palette_, 0, 7);

	// Generate and display pattern table visualization
	// Static variables for ROM change detection and clearing flags
	static const nes::Cartridge *last_cartridge = nullptr;
	static size_t last_chr_size = 0;
	static bool need_clear_when_no_rom = true;
	static bool need_clear_when_no_cartridge = true;

	if (cartridge) {
		if (!cartridge->is_loaded()) {
			ImGui::Text("Cartridge present but no ROM loaded");
			// Clear pattern table when no ROM is loaded
			if (need_clear_when_no_rom) {
				std::memset(pattern_table_buffer_.get(), 0, 256 * 128 * sizeof(uint32_t));
				update_pattern_table_texture();
				need_clear_when_no_rom = false;
				// Reset ROM tracking when unloaded so next ROM will be detected as change
				last_cartridge = nullptr;
				last_chr_size = 0;
			}
		} else {
			// Reset clearing flags when ROM is loaded
			need_clear_when_no_rom = true;
			need_clear_when_no_cartridge = true;

			const auto &rom_data = cartridge->get_rom_data();
			ImGui::Text("ROM: %d CHR pages (%d bytes)", static_cast<int>(rom_data.chr_rom_pages),
						static_cast<int>(rom_data.chr_rom.size()));

			if (rom_data.chr_rom_pages == 0) {
				ImGui::Text("Using CHR RAM (no CHR ROM)");
			}

			// Detect ROM changes by checking CHR ROM size or cartridge pointer
			bool rom_changed = (last_cartridge != cartridge) || (last_chr_size != rom_data.chr_rom.size());

			if (rom_changed) {
				printf("ROM change detected: cartridge %p->%p, chr_size %zu->%zu\n",
					   static_cast<const void *>(last_cartridge), static_cast<const void *>(cartridge), last_chr_size,
					   rom_data.chr_rom.size());
			}

			// Only regenerate when settings change, ROM changes, or first time
			static int last_pattern_table = -1;
			static int last_palette = -1;
			bool settings_changed =
				(last_pattern_table != selected_pattern_table_) || (last_palette != selected_palette_);

			if (pattern_table_dirty_ || settings_changed || rom_changed) {
				generate_pattern_table_visualization(ppu, cartridge);
				update_pattern_table_texture(); // Upload to OpenGL
				pattern_table_dirty_ = false;
				last_pattern_table = selected_pattern_table_;
				last_palette = selected_palette_;
				last_cartridge = cartridge;
				last_chr_size = rom_data.chr_rom.size();

				if (rom_changed) {
					printf("ROM changed detected - refreshing pattern tables\n");
				}
			}

			if (pattern_table_texture_ != 0) {
				// Scale display to fit available space - single pattern table is square (1:1 ratio)
				float max_width = content_width - 20.0f;		   // Leave some margin
				float display_width = std::min(max_width, 400.0f); // Max 400 pixels wide for square
				float display_height = display_width;			   // 1:1 ratio for single pattern table

				// Only show left half of texture (128x128) since we only render to that area
				ImVec2 uv0(0.0f, 0.0f); // Top-left
				ImVec2 uv1(0.5f, 1.0f); // Bottom at half-width (128 out of 256)
				ImVec2 display_size(display_width, display_height);
				ImGui::Text("Pattern Table %d (128x128, scaled to %.0fx%.0f):", selected_pattern_table_, display_width,
							display_height);
				ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(pattern_table_texture_)), display_size, uv0,
							 uv1);
			} else {
				ImGui::Text("Pattern table texture not initialized");
			}
		}
	} else {
		ImGui::Text("No cartridge loaded");
		// Clear pattern table when no cartridge is loaded
		if (need_clear_when_no_cartridge) {
			std::memset(pattern_table_buffer_.get(), 0, 256 * 128 * sizeof(uint32_t));
			update_pattern_table_texture();
			need_clear_when_no_cartridge = false;
			// Reset ROM tracking when cartridge is removed so next cartridge will be detected as change
			last_cartridge = nullptr;
			last_chr_size = 0;
		}
	}
}

void PPUViewerPanel::render_nametables(nes::PPU *ppu) {
	ImGui::Text("Nametables (Background Layout)");

	// Nametable selection
	ImGui::Text("Nametable:");
	ImGui::SameLine();
	for (int i = 0; i < 4; i++) {
		if (i > 0)
			ImGui::SameLine();
		char label[16];
		snprintf(label, sizeof(label), "$%04X", 0x2000 + (i * 0x400));
		ImGui::RadioButton(label, &selected_nametable_, i);
	}

	// Generate and display nametable visualization
	generate_nametable_visualization(ppu);

	if (nametable_texture_ != 0) {
		ImVec2 display_size(256 * 1.5f, 240 * 1.5f);
		ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(nametable_texture_)), display_size);
	}
}

void PPUViewerPanel::render_palette_viewer(nes::PPU *ppu) {
	if (!ppu) {
		ImGui::Text("No PPU connected");
		return;
	}

	ImGui::Text("NES Palette RAM");

	const auto &palette_ram = ppu->get_memory().get_palette_ram();

	// Background palettes
	ImGui::Text("Background Palettes:");
	for (int palette = 0; palette < 4; palette++) {
		ImGui::Text("BG %d:", palette);
		ImGui::SameLine();

		for (int color = 0; color < 4; color++) {
			uint8_t palette_index = palette * 4 + color;
			uint8_t nes_color = palette_ram[palette_index];
			ImVec4 color_vec = NESPalette::get_imgui_color(nes_color);

			char label[32];
			snprintf(label, sizeof(label), "##bg%d_%d", palette, color);
			ImGui::ColorButton(label, color_vec, ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24));

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Palette %d, Color %d\nNES Color: $%02X", palette, color, nes_color);
			}

			if (color < 3)
				ImGui::SameLine();
		}
	}

	ImGui::Separator();

	// Sprite palettes
	ImGui::Text("Sprite Palettes:");
	for (int palette = 0; palette < 4; palette++) {
		ImGui::Text("SP %d:", palette);
		ImGui::SameLine();

		for (int color = 0; color < 4; color++) {
			// Sprite palettes are at indices 16-31 (0x10-0x1F)
			uint8_t palette_index = 16 + palette * 4 + color;
			uint8_t nes_color = palette_ram[palette_index];
			ImVec4 color_vec = NESPalette::get_imgui_color(nes_color);

			char label[32];
			snprintf(label, sizeof(label), "##sp%d_%d", palette, color);
			ImGui::ColorButton(label, color_vec, ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24));

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Sprite Palette %d, Color %d\nNES Color: $%02X", palette, color, nes_color);
			}

			if (color < 3)
				ImGui::SameLine();
		}
	}

	ImGui::Separator();

	// Universal backdrop color
	ImGui::Text("Universal Backdrop:");
	uint8_t backdrop_color = palette_ram[0];
	ImVec4 backdrop_vec = NESPalette::get_imgui_color(backdrop_color);
	ImGui::ColorButton("##backdrop", backdrop_vec, ImGuiColorEditFlags_NoTooltip, ImVec2(32, 32));
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Universal Backdrop Color\nNES Color: $%02X", backdrop_color);
	}
}

void PPUViewerPanel::render_sprite_viewer(nes::PPU *ppu) {
	(void)ppu; // Suppress unused parameter warning
	ImGui::Text("Sprite OAM Data");

	// This would show the OAM contents
	// For now, show placeholder
	ImGui::Text("64 sprites, 4 bytes each:");
	ImGui::Text("Y, Tile, Attributes, X");

	if (ImGui::BeginTable("OAM", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("Y");
		ImGui::TableSetupColumn("Tile");
		ImGui::TableSetupColumn("Attr");
		ImGui::TableSetupColumn("X");
		ImGui::TableHeadersRow();

		// Show first 16 sprites as example
		for (int i = 0; i < 16; i++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%02d", i);
			ImGui::TableNextColumn();
			ImGui::Text("$%02X", 0); // Placeholder
			ImGui::TableNextColumn();
			ImGui::Text("$%02X", 0); // Placeholder
			ImGui::TableNextColumn();
			ImGui::Text("$%02X", 0); // Placeholder
			ImGui::TableNextColumn();
			ImGui::Text("$%02X", 0); // Placeholder
		}

		ImGui::EndTable();
	}
}

void PPUViewerPanel::render_ppu_registers(nes::PPU *ppu) {
	ImGui::Text("PPU Registers:");

	// PPUCTRL ($2000)
	uint8_t ctrl = ppu->get_control_register();
	ImGui::TextColored(RetroTheme::get_register_color(), "PPUCTRL ($2000): $%02X", ctrl);
	ImGui::SameLine();
	ImGui::TextColored(RetroTheme::get_address_color(), "[NT:%d Inc:%s SPT:%04X BGT:%04X SS:%s NMI:%s]", ctrl & 0x03,
					   (ctrl & 0x04) ? "+32" : "+1", (ctrl & 0x08) ? 0x1000 : 0x0000, (ctrl & 0x10) ? 0x1000 : 0x0000,
					   (ctrl & 0x20) ? "8x16" : "8x8", (ctrl & 0x80) ? "ON" : "OFF");

	// PPUMASK ($2001)
	uint8_t mask = ppu->get_mask_register();
	ImGui::TextColored(RetroTheme::get_register_color(), "PPUMASK ($2001): $%02X", mask);
	ImGui::SameLine();
	ImGui::TextColored(RetroTheme::get_address_color(), "[BG:%s SPR:%s]", (mask & 0x08) ? "ON" : "OFF",
					   (mask & 0x10) ? "ON" : "OFF");

	// PPUSTATUS ($2002)
	uint8_t status = ppu->get_status_register();
	ImGui::TextColored(RetroTheme::get_register_color(), "PPUSTATUS ($2002): $%02X", status);
	ImGui::SameLine();
	ImGui::TextColored(RetroTheme::get_address_color(), "[VBL:%s S0H:%s SOF:%s]", (status & 0x80) ? "1" : "0",
					   (status & 0x40) ? "1" : "0", (status & 0x20) ? "1" : "0");
}

void PPUViewerPanel::render_timing_info(nes::PPU *ppu) {
	ImGui::Separator();
	ImGui::Text("Timing Information:");

	ImGui::TextColored(RetroTheme::get_value_color(), "Current Scanline: %d", ppu->get_current_scanline());
	ImGui::TextColored(RetroTheme::get_value_color(), "Current Cycle: %d", ppu->get_current_cycle());
	ImGui::TextColored(RetroTheme::get_value_color(), "Frame Count: %llu", ppu->get_frame_count());

	// Show scanline phase
	uint16_t scanline = ppu->get_current_scanline();
	const char *phase = "Unknown";
	if (scanline < 240)
		phase = "Visible";
	else if (scanline == 240)
		phase = "Post-render";
	else if (scanline <= 260)
		phase = "VBlank";
	else
		phase = "Pre-render";

	ImGui::TextColored(RetroTheme::get_address_color(), "Scanline Phase: %s", phase);
}

void PPUViewerPanel::initialize_textures() {
	printf("Initializing OpenGL textures...\n");

	// Generate OpenGL textures
	glGenTextures(1, &main_display_texture_);
	glGenTextures(1, &pattern_table_texture_);
	glGenTextures(1, &nametable_texture_);

	printf("Generated textures: main=%u, pattern=%u, nametable=%u\n", main_display_texture_, pattern_table_texture_,
		   nametable_texture_);

	// Setup main display texture (256x240)
	glBindTexture(GL_TEXTURE_2D, main_display_texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Setup pattern table texture (256x128)
	glBindTexture(GL_TEXTURE_2D, pattern_table_texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Setup nametable texture (512x480)
	glBindTexture(GL_TEXTURE_2D, nametable_texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	textures_initialized_ = true;
	printf("Texture initialization complete!\n");
}

void PPUViewerPanel::cleanup_textures() {
	if (textures_initialized_) {
		glDeleteTextures(1, &main_display_texture_);
		glDeleteTextures(1, &pattern_table_texture_);
		glDeleteTextures(1, &nametable_texture_);
		textures_initialized_ = false;
	}
}

void PPUViewerPanel::update_main_display_texture(const uint32_t *frame_buffer) {
	if (main_display_texture_ == 0 || !frame_buffer)
		return;

	glBindTexture(GL_TEXTURE_2D, main_display_texture_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
}

void PPUViewerPanel::update_pattern_table_texture() {
	if (pattern_table_texture_ == 0 || !pattern_table_buffer_) {
		printf("Pattern table texture update failed: texture=%u, buffer=%p\n", pattern_table_texture_,
			   pattern_table_buffer_.get());
		return;
	}

	printf("Updating pattern table texture (256x128)...\n");
	glBindTexture(GL_TEXTURE_2D, pattern_table_texture_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 128, GL_RGBA, GL_UNSIGNED_BYTE, pattern_table_buffer_.get());
}

void PPUViewerPanel::generate_pattern_table_visualization(nes::PPU *ppu, nes::Cartridge *cartridge) {
	if (!ppu || !cartridge) {
		// Clear buffer if no data available
		std::memset(pattern_table_buffer_.get(), 0, 256 * 128 * sizeof(uint32_t));
		return;
	}

	// One-time debug output to verify CHR ROM is working
	static bool debug_printed = false;
	if (!debug_printed && cartridge->is_loaded()) {
		printf("CHR ROM Debug (first time only):\n");
		for (int i = 0; i < 16; i++) {
			uint8_t value = ppu->read_chr_rom(i);
			printf("  CHR[$%04X] = $%02X", i, value);
			if ((i + 1) % 8 == 0)
				printf("\n");
		}
		printf("CHR ROM samples: $0100=%02X, $1000=%02X\n", ppu->read_chr_rom(0x0100), ppu->read_chr_rom(0x1000));

		// Compare first tile of each pattern table
		printf("Pattern Table 0 first tile (tile 0):\n");
		for (int i = 0; i < 16; i++) {
			printf("  $%04X: %02X\n", i, ppu->read_chr_rom(i));
		}
		printf("Pattern Table 1 first tile (tile 0):\n");
		for (int i = 0x1000; i < 0x1010; i++) {
			printf("  $%04X: %02X\n", i, ppu->read_chr_rom(i));
		}

		debug_printed = true;
	}

	// Pattern table layout: 128x128 pixels (single pattern table)
	// Display only the selected pattern table ($0000 or $1000)

	uint16_t base_address = selected_pattern_table_ * 0x1000; // $0000 or $1000 based on selection

	// Each pattern table is 16x16 tiles, each tile is 8x8 pixels
	for (int tile_y = 0; tile_y < 16; tile_y++) {
		for (int tile_x = 0; tile_x < 16; tile_x++) {
			uint8_t tile_index = tile_y * 16 + tile_x;
			uint16_t tile_address = base_address + (tile_index * 16);

			// Render 8x8 tile
			for (int pixel_y = 0; pixel_y < 8; pixel_y++) {
				// Read pattern data for this row
				uint16_t plane0_addr = tile_address + pixel_y;
				uint16_t plane1_addr = tile_address + pixel_y + 8;
				uint8_t plane0 = ppu->read_chr_rom(plane0_addr);
				uint8_t plane1 = ppu->read_chr_rom(plane1_addr);

				for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
					// Extract 2-bit pixel value
					uint8_t bit_pos = 7 - pixel_x;
					uint8_t pixel_value = ((plane0 >> bit_pos) & 1) | (((plane1 >> bit_pos) & 1) << 1);

					// Calculate screen position (single pattern table, so no x_offset)
					int screen_x = (tile_x * 8) + pixel_x;
					int screen_y = (tile_y * 8) + pixel_y;
					int buffer_index = screen_y * 256 + screen_x; // Still use 256-wide buffer

					// Convert to color using selected palette
					uint32_t color;
					if (pixel_value == 0) {
						// Transparent pixels show as dark gray
						color = 0xFF404040;
					} else {
						// Use palette 0 + selected background palette for visualization
						color = get_pattern_pixel_color(pixel_value, selected_palette_, ppu);
					}

					pattern_table_buffer_[buffer_index] = color;
				}
			}
		}
	}

	// Clear the right half of the buffer since we're only showing one pattern table
	for (int y = 0; y < 128; y++) {
		for (int x = 128; x < 256; x++) {
			int buffer_index = y * 256 + x;
			pattern_table_buffer_[buffer_index] = 0xFF000000; // Black
		}
	}

	// Debug: Check if we generated any non-zero pixel data
	static bool debug_texture_once = false;
	if (!debug_texture_once) {
		int non_zero_pixels = 0;
		for (int i = 0; i < 256 * 128; i++) {
			if (pattern_table_buffer_[i] != 0)
				non_zero_pixels++;
		}
		printf("Pattern table texture: %d non-zero pixels out of %d total\n", non_zero_pixels, 256 * 128);
		debug_texture_once = true;
	}
}

void PPUViewerPanel::generate_nametable_visualization(nes::PPU *ppu) {
	(void)ppu; // Suppress unused parameter warning
	// This is a placeholder implementation
	// Would need to read nametable data and convert to visual representation
	// For now, just clear the buffer
	std::memset(nametable_buffer_.get(), 0, 512 * 480 * sizeof(uint32_t));

	if (nametable_texture_ != 0) {
		glBindTexture(GL_TEXTURE_2D, nametable_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 480, GL_RGBA, GL_UNSIGNED_BYTE, nametable_buffer_.get());
	}
}

uint32_t PPUViewerPanel::get_pattern_pixel_color(uint8_t pixel_value, uint8_t palette_index, nes::PPU *ppu) {
	// Suppress unused parameter warnings (using fixed palette for now)
	(void)palette_index;
	(void)ppu;

	if (pixel_value == 0) {
		// Transparent pixels show as dark gray for visibility
		return 0xFF202020;
	}

	// For pattern table visualization, use a fixed palette to ensure visibility
	// This is independent of the actual PPU palette RAM
	static const uint32_t visualization_palette[4] = {
		0xFF202020, // 0: Dark gray (transparent)
		0xFF808080, // 1: Medium gray
		0xFFB0B0B0, // 2: Light gray
		0xFFFFFFFF	// 3: White
	};

	if (pixel_value >= 4) {
		pixel_value = 3; // Clamp to valid range
	}

	return visualization_palette[pixel_value];
}

} // namespace nes::gui
