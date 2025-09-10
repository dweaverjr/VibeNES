#pragma once

#include "imgui.h"

namespace nes::gui {

/// Retro NES-era color palette and styling
class RetroTheme {
  public:
	// NES-inspired color palette
	static constexpr ImVec4 NES_DARK_BLUE = ImVec4(0.0f, 0.0f, 0.2f, 1.0f);
	static constexpr ImVec4 NES_MEDIUM_BLUE = ImVec4(0.0f, 0.0f, 0.4f, 1.0f);
	static constexpr ImVec4 NES_LIGHT_GRAY = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
	static constexpr ImVec4 NES_MEDIUM_GRAY = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	static constexpr ImVec4 NES_DARK_GRAY = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
	static constexpr ImVec4 NES_GREEN = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
	static constexpr ImVec4 NES_RED = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);
	static constexpr ImVec4 NES_YELLOW = ImVec4(0.8f, 0.8f, 0.0f, 1.0f);
	static constexpr ImVec4 NES_WHITE = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	static constexpr ImVec4 NES_BLACK = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

	/// Apply retro theme to ImGui
	static void apply_retro_style();

	/// Get color for CPU register display
	static ImVec4 get_register_color();

	/// Get color for active flags
	static ImVec4 get_flag_active_color();

	/// Get color for inactive flags
	static ImVec4 get_flag_inactive_color();

	/// Get color for current instruction highlight
	static ImVec4 get_current_instruction_color();

	/// Get color for memory addresses
	static ImVec4 get_address_color();

	/// Get color for hexadecimal values
	static ImVec4 get_hex_color();
};

} // namespace nes::gui
