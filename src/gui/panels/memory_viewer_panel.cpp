#include "gui/panels/memory_viewer_panel.hpp"
#include "core/bus.hpp"
#include "gui/style/retro_theme.hpp"

#include <algorithm>
#include <cstdio>
namespace nes::gui {

namespace {

constexpr uint16_t kMinBytesPerRow = 1;
constexpr uint16_t kMaxBytesPerRow = 16;
constexpr float kAddressColumnWidth = 80.0f;
constexpr float kByteColumnWidth = 25.0f;
constexpr float kSpaceColumnWidth = 160.0f;

[[nodiscard]] uint16_t clamp_bytes_per_row(uint16_t bytes_per_row) {
	return std::clamp<uint16_t>(bytes_per_row, kMinBytesPerRow, kMaxBytesPerRow);
}

void render_cpu_address_space_label(uint16_t address) {
	if (address <= 0x00FF) {
		ImGui::TextUnformatted("Zero Page");
		return;
	}
	if (address <= 0x01FF) {
		ImGui::TextUnformatted("Stack");
		return;
	}
	if (address <= 0x07FF) {
		ImGui::TextUnformatted("RAM");
		return;
	}
	if (address <= 0x0FFF) {
		ImGui::TextUnformatted("RAM M1<-0000-07FF");
		return;
	}
	if (address <= 0x17FF) {
		ImGui::TextUnformatted("RAM M2<-0000-07FF");
		return;
	}
	if (address <= 0x1FFF) {
		ImGui::TextUnformatted("RAM M3<-0000-07FF");
		return;
	}
	if (address <= 0x2007) {
		ImGui::TextUnformatted("PPU Regs");
		return;
	}
	if (address <= 0x3FFF) {
		const uint16_t mirror_index = static_cast<uint16_t>(((address - 0x2008) / 0x0008) + 1);
		ImGui::Text("PPU M%u<-2000-2007", mirror_index);
		return;
	}
	if (address <= 0x4015) {
		ImGui::TextUnformatted(address == 0x4014 ? "OAM DMA" : "APU Regs");
		return;
	}
	if (address <= 0x4017) {
		ImGui::TextUnformatted("Ctrl I/O");
		return;
	}
	if (address <= 0x401F) {
		ImGui::TextUnformatted("Disabled/Test");
		return;
	}
	if (address <= 0x5FFF) {
		ImGui::TextUnformatted("Cart Exp");
		return;
	}
	if (address <= 0x7FFF) {
		ImGui::TextUnformatted("Cart PRG-RAM");
		return;
	}
	if (address <= 0xFFF9) {
		ImGui::TextUnformatted("Cart PRG-ROM");
		return;
	}
	ImGui::TextUnformatted("Vectors");
}

} // namespace

MemoryViewerPanel::MemoryViewerPanel()
	: visible_(true), start_address_(0x0000), bytes_per_row_(8), rows_to_show_(16), scroll_to_address_(false) {
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
	bytes_per_row_ = clamp_bytes_per_row(bytes_per_row_);

	if (ImGui::Button("Zero Page")) {
		start_address_ = 0x0000;
		scroll_to_address_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Stack")) {
		start_address_ = 0x0100;
		scroll_to_address_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("PPU")) {
		start_address_ = 0x2000;
		scroll_to_address_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("APU")) {
		start_address_ = 0x4000;
		scroll_to_address_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("ROM")) {
		start_address_ = 0x8000;
		scroll_to_address_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Vectors")) {
		start_address_ = 0xFFFA;
		scroll_to_address_ = true;
	}
}

void MemoryViewerPanel::render_memory_grid(const nes::SystemBus *bus) {
	bytes_per_row_ = clamp_bytes_per_row(bytes_per_row_);
	const uint16_t bytes_per_row = bytes_per_row_;

	// Calculate total rows for entire CPU address space (0x0000-0xFFFF = 64KB)
	const uint32_t total_memory = 0x10000; // 64KB full CPU address space
	const uint32_t total_rows = (total_memory + bytes_per_row - 1) / bytes_per_row;

	// Create scrollable child window for memory display
	if (ImGui::BeginChild("MemoryScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
		if (ImGui::BeginTable("MemoryTable", static_cast<int>(bytes_per_row + 3),
							  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {

			// Header row
			ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, kAddressColumnWidth);
			for (uint16_t i = 0; i < bytes_per_row; ++i) {
				char header[8];
				std::snprintf(header, sizeof(header), "%02X", i);
				ImGui::TableSetupColumn(header, ImGuiTableColumnFlags_WidthFixed, kByteColumnWidth);
			}
			ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, static_cast<float>(bytes_per_row * 8));
			ImGui::TableSetupColumn("Space", ImGuiTableColumnFlags_WidthFixed, kSpaceColumnWidth);
			ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row
			ImGui::TableHeadersRow();

			// Handle scroll jump to specific address
			if (scroll_to_address_) {
				uint32_t target_row = start_address_ / bytes_per_row;
				ImGui::SetScrollY(target_row * ImGui::GetTextLineHeightWithSpacing());
				scroll_to_address_ = false;
			}

			// Use clipper for efficient rendering of large lists
			ImGuiListClipper clipper;
			clipper.Begin(total_rows);
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					uint16_t row_address = static_cast<uint16_t>(row * bytes_per_row);

					ImGui::TableNextRow();

					// Address column
					ImGui::TableNextColumn();
					ImGui::TextColored(RetroTheme::get_address_color(), "$%04X", row_address);

					// Hex columns
					for (uint16_t col = 0; col < bytes_per_row; ++col) {
						ImGui::TableNextColumn();
						uint16_t addr = static_cast<uint16_t>(row_address + col);
						uint8_t value = bus->peek(addr); // Use peek to avoid side effects
						ImGui::TextColored(RetroTheme::get_hex_color(), "%02X", value);
					}

					// ASCII column
					ImGui::TableNextColumn();
					render_ascii_column(bus, row_address, bytes_per_row);

					// Address-space column
					ImGui::TableNextColumn();
					render_cpu_address_space_label(row_address);
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();
}

void MemoryViewerPanel::render_ascii_column(const nes::SystemBus *bus, uint16_t start_addr, uint16_t count) {
	char ascii_str[32] = {0}; // Max 16 bytes per row + null terminator

	for (uint16_t i = 0; i < count && i < 31; ++i) {
		uint8_t value = bus->peek(static_cast<uint16_t>(start_addr + i)); // Use peek to avoid side effects
		ascii_str[i] = (value >= 32 && value < 127) ? static_cast<char>(value) : '.';
	}

	ImGui::Text("%s", ascii_str);
}

} // namespace nes::gui
