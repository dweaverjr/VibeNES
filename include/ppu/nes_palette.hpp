#pragma once

#include "core/types.hpp"
#include <array>

// Forward declaration for ImGui
struct ImVec4;

namespace nes {

/// NES Master Palette - The 64 possible colors the NES can display
/// These are the exact RGB values for the NES 2C02 PPU
class NESPalette {
  public:
	/// Get RGB color for NES color index (0-63)
	static uint32_t get_rgb_color(uint8_t nes_color_index);

	/// Get RGBA color for ImGui/OpenGL display
	static uint32_t get_rgba_color(uint8_t nes_color_index);

	/// Convert NES color to ImVec4 for ImGui
	static ImVec4 get_imgui_color(uint8_t nes_color_index);

  private:
	/// The complete NES color palette (64 colors, RGB format)
	static constexpr std::array<uint32_t, 64> NES_COLORS = {
		// Row 0 (0x00-0x0F)
		0x545454, 0x001E74, 0x081090, 0x300088, 0x440064, 0x5C0030, 0x540400, 0x3C1800, 0x202A00, 0x083A00, 0x004000,
		0x003C00, 0x00323C, 0x000000, 0x000000, 0x000000,

		// Row 1 (0x10-0x1F)
		0x989698, 0x084CC4, 0x3032EC, 0x5C1EE4, 0x8814B0, 0xA01464, 0x982220, 0x783C00, 0x545A00, 0x287200, 0x087C00,
		0x007628, 0x006678, 0x000000, 0x000000, 0x000000,

		// Row 2 (0x20-0x2F)
		0xECEEEC, 0x4C9AEC, 0x787CEC, 0xB062EC, 0xE454EC, 0xEC58B4, 0xEC6A64, 0xD48820, 0xA0AA00, 0x74C400, 0x4CD020,
		0x38CC6C, 0x38B4CC, 0x3C3C3C, 0x000000, 0x000000,

		// Row 3 (0x30-0x3F)
		0xECEEEC, 0xA8CCEC, 0xBCBCEC, 0xD4B2EC, 0xECAEEC, 0xECAED4, 0xECB4B0, 0xE4C490, 0xCCD278, 0xB4DE78, 0xA8E290,
		0xA0E2B0, 0xA0D6E4, 0xA0A2A0, 0x000000, 0x000000};
};

} // namespace nes
