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
	  selected_palette_(0), display_scale_(2.0f), textures_initialized_(false) {

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
	ImGui::Text("Pattern Tables (CHR ROM/RAM)");

	// Pattern table selection
	ImGui::Text("Pattern Table:");
	ImGui::SameLine();
	ImGui::RadioButton("$0000", &selected_pattern_table_, 0);
	ImGui::SameLine();
	ImGui::RadioButton("$1000", &selected_pattern_table_, 1);

	// Palette selection for visualization
	ImGui::Text("Palette for visualization:");
	ImGui::SameLine();
	ImGui::SliderInt("##palette", &selected_palette_, 0, 7);

	// Generate and display pattern table visualization
	if (cartridge) {
		generate_pattern_table_visualization(ppu, cartridge);

		if (pattern_table_texture_ != 0) {
			ImVec2 display_size(128 * 2.0f, 128 * 2.0f); // 16x16 tiles, 8x8 each
			ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(pattern_table_texture_)), display_size);
		}
	} else {
		ImGui::Text("No cartridge loaded");
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
	// Generate OpenGL textures
	glGenTextures(1, &main_display_texture_);
	glGenTextures(1, &pattern_table_texture_);
	glGenTextures(1, &nametable_texture_);

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

void PPUViewerPanel::generate_pattern_table_visualization(nes::PPU *ppu, nes::Cartridge *cartridge) {
	if (!ppu || !cartridge) {
		// Clear buffer if no data available
		std::memset(pattern_table_buffer_.get(), 0, 256 * 128 * sizeof(uint32_t));
		return;
	}

	// Pattern table layout: 256x128 pixels
	// Left half (0-127x): Pattern table 0 ($0000-$0FFF)
	// Right half (128-255x): Pattern table 1 ($1000-$1FFF)

	for (int table = 0; table < 2; table++) {
		uint16_t base_address = table * 0x1000; // $0000 or $1000
		int x_offset = table * 128;				// Left or right half

		// Each pattern table is 16x16 tiles, each tile is 8x8 pixels
		for (int tile_y = 0; tile_y < 16; tile_y++) {
			for (int tile_x = 0; tile_x < 16; tile_x++) {
				uint8_t tile_index = tile_y * 16 + tile_x;
				uint16_t tile_address = base_address + (tile_index * 16);

				// Render 8x8 tile
				for (int pixel_y = 0; pixel_y < 8; pixel_y++) {
					// Read pattern data for this row
					uint8_t plane0 = ppu->read_chr_rom(tile_address + pixel_y);
					uint8_t plane1 = ppu->read_chr_rom(tile_address + pixel_y + 8);

					for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
						// Extract 2-bit pixel value
						uint8_t bit_pos = 7 - pixel_x;
						uint8_t pixel_value = ((plane0 >> bit_pos) & 1) | (((plane1 >> bit_pos) & 1) << 1);

						// Calculate screen position
						int screen_x = x_offset + (tile_x * 8) + pixel_x;
						int screen_y = (tile_y * 8) + pixel_y;
						int buffer_index = screen_y * 256 + screen_x;

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
	}

	// Upload to OpenGL texture
	if (pattern_table_texture_ != 0) {
		glBindTexture(GL_TEXTURE_2D, pattern_table_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 128, GL_RGBA, GL_UNSIGNED_BYTE, pattern_table_buffer_.get());
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
	if (!ppu || pixel_value == 0) {
		// Transparent pixels show as dark gray for visibility
		return 0xFF404040;
	}

	// Get the actual palette RAM data
	const auto &palette_ram = ppu->get_memory().get_palette_ram();

	// Background palettes start at index 0, each palette is 4 colors
	// Palette index 0-3 for background palettes
	uint8_t palette_base = (palette_index & 0x03) * 4;
	uint8_t color_index = palette_ram[palette_base + pixel_value];

	// Debug: Check palette data for the first few calls
	static int debug_count = 0;
	if (debug_count < 5) {
		printf("Pattern pixel: value=%d, palette_idx=%d, palette_base=%d, color_idx=%d\n", pixel_value, palette_index,
			   palette_base, color_index);
		debug_count++;
	}

	// Convert NES color index to RGBA
	return NESPalette::get_rgba_color(color_index);
}

} // namespace nes::gui
