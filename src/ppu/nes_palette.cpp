#include "ppu/nes_palette.hpp"
#ifdef NES_GUI_ENABLED
#include <imgui.h>
#endif

namespace nes {

uint32_t NESPalette::get_rgb_color(uint8_t nes_color_index) {
	// Mask to 6 bits (64 colors total)
	return NES_COLORS[nes_color_index & 0x3F];
}

uint32_t NESPalette::get_rgba_color(uint8_t nes_color_index) {
	uint32_t rgb = get_rgb_color(nes_color_index);

	// Convert RGB to RGBA for OpenGL (add alpha channel)
	uint8_t r = (rgb >> 16) & 0xFF;
	uint8_t g = (rgb >> 8) & 0xFF;
	uint8_t b = rgb & 0xFF;
	uint8_t a = 0xFF; // Fully opaque

	return (a << 24) | (b << 16) | (g << 8) | r; // ABGR format for OpenGL
}

#ifdef NES_GUI_ENABLED
ImVec4 NESPalette::get_imgui_color(uint8_t nes_color_index) {
	uint32_t rgb = get_rgb_color(nes_color_index);

	float r = ((rgb >> 16) & 0xFF) / 255.0f;
	float g = ((rgb >> 8) & 0xFF) / 255.0f;
	float b = (rgb & 0xFF) / 255.0f;

	return ImVec4(r, g, b, 1.0f);
}
#endif

} // namespace nes
