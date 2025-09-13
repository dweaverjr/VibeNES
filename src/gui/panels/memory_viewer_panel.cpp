#include "gui/panels/memory_viewer_panel.hpp"
#include "core/bus.hpp"
#include "gui/style/retro_theme.hpp"

namespace nes::gui {

MemoryViewerPanel::MemoryViewerPanel() : visible_(true), start_address_(0x0000), bytes_per_row_(16), rows_to_show_(16) {
}

void MemoryViewerPanel::render(const nes::SystemBus *bus) {
	if (!bus)
		return;

	render_controls();
	ImGui::Separator();
	render_memory_grid(bus);
}

void MemoryViewerPanel::render_controls() {
	ImGui::Text("Start Address:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::InputScalar("##start", ImGuiDataType_U16, &start_address_, nullptr, nullptr, "%04X",
					   ImGuiInputTextFlags_CharsHexadecimal);

	ImGui::SameLine();
	ImGui::Text("Bytes/Row:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60);
	ImGui::InputScalar("##bpr", ImGuiDataType_U16, &bytes_per_row_, nullptr, nullptr, "%d");

	if (ImGui::Button("Zero Page"))
		start_address_ = 0x0000;
	ImGui::SameLine();
	if (ImGui::Button("Stack"))
		start_address_ = 0x0100;
	ImGui::SameLine();
	if (ImGui::Button("PPU"))
		start_address_ = 0x2000;
	ImGui::SameLine();
	if (ImGui::Button("APU"))
		start_address_ = 0x4000;
	ImGui::SameLine();
	if (ImGui::Button("ROM"))
		start_address_ = 0x8000;
	ImGui::SameLine();
	if (ImGui::Button("Vectors"))
		start_address_ = 0xFFFA;
}

void MemoryViewerPanel::render_memory_grid(const nes::SystemBus *bus) {
	if (ImGui::BeginTable("MemoryTable", static_cast<int>(bytes_per_row_ + 2),
						  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

		// Header row
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		for (uint16_t i = 0; i < bytes_per_row_; ++i) {
			char header[8];
			snprintf(header, sizeof(header), "%02X", i);
			ImGui::TableSetupColumn(header, ImGuiTableColumnFlags_WidthFixed, 25.0f);
		}
		ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, static_cast<float>(bytes_per_row_ * 8));
		ImGui::TableHeadersRow();

		// Data rows
		for (uint16_t row = 0; row < rows_to_show_; ++row) {
			uint16_t row_address = static_cast<uint16_t>(start_address_ + (row * bytes_per_row_));

			ImGui::TableNextRow();

			// Address column
			ImGui::TableNextColumn();
			ImGui::TextColored(RetroTheme::get_address_color(), "$%04X", row_address);

			// Hex columns
			for (uint16_t col = 0; col < bytes_per_row_; ++col) {
				ImGui::TableNextColumn();
				uint16_t addr = static_cast<uint16_t>(row_address + col);
				uint8_t value = bus->read(addr);
				ImGui::TextColored(RetroTheme::get_hex_color(), "%02X", value);
			}

			// ASCII column
			ImGui::TableNextColumn();
			render_ascii_column(bus, row_address, bytes_per_row_);
		}

		ImGui::EndTable();
	}
}

void MemoryViewerPanel::render_ascii_column(const nes::SystemBus *bus, uint16_t start_addr, uint16_t count) {
	char ascii_str[32] = {0}; // Max 16 bytes per row + null terminator

	for (uint16_t i = 0; i < count && i < 31; ++i) {
		uint8_t value = bus->read(static_cast<uint16_t>(start_addr + i));
		ascii_str[i] = (value >= 32 && value < 127) ? static_cast<char>(value) : '.';
	}

	ImGui::Text("%s", ascii_str);
}

} // namespace nes::gui
