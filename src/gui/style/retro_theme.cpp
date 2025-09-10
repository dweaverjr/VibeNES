#include "gui/style/retro_theme.hpp"

namespace nes::gui {

void RetroTheme::apply_retro_style() {
	ImGuiStyle &style = ImGui::GetStyle();
	ImVec4 *colors = style.Colors;

	// Window styling
	colors[ImGuiCol_WindowBg] = NES_DARK_BLUE;
	colors[ImGuiCol_ChildBg] = NES_DARK_BLUE;
	colors[ImGuiCol_PopupBg] = NES_MEDIUM_BLUE;

	// Frame styling
	colors[ImGuiCol_FrameBg] = NES_MEDIUM_BLUE;
	colors[ImGuiCol_FrameBgHovered] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_FrameBgActive] = NES_LIGHT_GRAY;

	// Title bar
	colors[ImGuiCol_TitleBg] = NES_DARK_GRAY;
	colors[ImGuiCol_TitleBgActive] = NES_MEDIUM_BLUE;
	colors[ImGuiCol_TitleBgCollapsed] = NES_DARK_GRAY;

	// Menu bar
	colors[ImGuiCol_MenuBarBg] = NES_DARK_GRAY;

	// Scrollbar
	colors[ImGuiCol_ScrollbarBg] = NES_DARK_BLUE;
	colors[ImGuiCol_ScrollbarGrab] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_ScrollbarGrabHovered] = NES_LIGHT_GRAY;
	colors[ImGuiCol_ScrollbarGrabActive] = NES_WHITE;

	// Button
	colors[ImGuiCol_Button] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_ButtonHovered] = NES_LIGHT_GRAY;
	colors[ImGuiCol_ButtonActive] = NES_WHITE;

	// Header
	colors[ImGuiCol_Header] = NES_MEDIUM_BLUE;
	colors[ImGuiCol_HeaderHovered] = NES_LIGHT_GRAY;
	colors[ImGuiCol_HeaderActive] = NES_WHITE;

	// Separator
	colors[ImGuiCol_Separator] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_SeparatorHovered] = NES_LIGHT_GRAY;
	colors[ImGuiCol_SeparatorActive] = NES_WHITE;

	// Text
	colors[ImGuiCol_Text] = NES_LIGHT_GRAY;
	colors[ImGuiCol_TextDisabled] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_TextSelectedBg] = NES_MEDIUM_BLUE;

	// Checkbox
	colors[ImGuiCol_CheckMark] = NES_GREEN;

	// Tab
	colors[ImGuiCol_Tab] = NES_DARK_GRAY;
	colors[ImGuiCol_TabHovered] = NES_MEDIUM_BLUE;
	colors[ImGuiCol_TabActive] = NES_MEDIUM_BLUE;
	colors[ImGuiCol_TabUnfocused] = NES_DARK_GRAY;
	colors[ImGuiCol_TabUnfocusedActive] = NES_DARK_BLUE;

	// Borders
	colors[ImGuiCol_Border] = NES_MEDIUM_GRAY;
	colors[ImGuiCol_BorderShadow] = NES_BLACK;

	// Style properties for retro look
	style.WindowRounding = 0.0f; // Sharp, pixelated edges
	style.ChildRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 1.0f;

	// Padding for better spacing
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.ItemSpacing = ImVec2(4.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
}

ImVec4 RetroTheme::get_register_color() {
	return NES_YELLOW;
}

ImVec4 RetroTheme::get_flag_active_color() {
	return NES_GREEN;
}

ImVec4 RetroTheme::get_flag_inactive_color() {
	return NES_RED;
}

ImVec4 RetroTheme::get_current_instruction_color() {
	return NES_YELLOW;
}

ImVec4 RetroTheme::get_address_color() {
	return NES_LIGHT_GRAY;
}

ImVec4 RetroTheme::get_hex_color() {
	return NES_WHITE;
}

} // namespace nes::gui
