#include "gui/panels/disassembler_panel.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"

namespace nes::gui {

DisassemblerPanel::DisassemblerPanel() : visible_(true), follow_pc_(true), start_address_(0x0000) {
}

void DisassemblerPanel::render(const nes::CPU6502 *cpu, const nes::SystemBus *bus) {
	if (!visible_ || !cpu || !bus)
		return;

	if (ImGui::Begin("Disassembler", &visible_)) {
		render_controls();
		ImGui::Separator();
		render_instruction_list(cpu, bus);
	}
	ImGui::End();
}

void DisassemblerPanel::render_controls() {
	ImGui::Checkbox("Follow PC", reinterpret_cast<bool *>(&follow_pc_));

	if (!follow_pc_) {
		ImGui::SameLine();
		ImGui::Text("Start:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		if (ImGui::InputScalar("##start", ImGuiDataType_U16, &start_address_, nullptr, nullptr, "%04X",
							   ImGuiInputTextFlags_CharsHexadecimal)) {
			// Address updated
		}
	}
}

void DisassemblerPanel::render_instruction_list(const nes::CPU6502 *cpu, const nes::SystemBus *bus) {
	uint16_t current_pc = cpu->get_program_counter();
	uint16_t display_start = follow_pc_ ? current_pc : start_address_;

	// Display instructions around the current PC
	for (int i = -5; i <= 10; ++i) {
		uint16_t addr = static_cast<uint16_t>(display_start + i);
		uint8_t opcode = bus->read(addr);

		// Highlight current instruction
		if (addr == current_pc) {
			ImGui::TextColored(RetroTheme::get_current_instruction_color(), ">");
		} else {
			ImGui::Text(" ");
		}

		ImGui::SameLine();
		ImGui::TextColored(RetroTheme::get_address_color(), "$%04X:", addr);
		ImGui::SameLine();
		ImGui::TextColored(RetroTheme::get_hex_color(), "%02X", opcode);
		ImGui::SameLine();

		// Simple disassembly - just show NOP for now as placeholder
		if (opcode == 0xEA) {
			ImGui::Text("NOP");
		} else {
			ImGui::Text("???");
		}
	}
}

} // namespace nes::gui
