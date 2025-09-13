#include "gui/panels/cpu_state_panel.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"

namespace nes::gui {

CPUStatePanel::CPUStatePanel() : visible_(true) {
}

void CPUStatePanel::render(nes::CPU6502 *cpu) {
	if (!cpu)
		return;

	render_controls(cpu);
	ImGui::Separator();
	render_registers(cpu);
	ImGui::Separator();
	render_flags(cpu);
	ImGui::Separator();
	render_stack_info(cpu);
}

void CPUStatePanel::render_registers(const nes::CPU6502 *cpu) {
	ImGui::Text("Registers:");

	ImGui::TextColored(RetroTheme::get_register_color(), "A:  $%02X (%3d)", cpu->get_accumulator(),
					   cpu->get_accumulator());

	ImGui::TextColored(RetroTheme::get_register_color(), "X:  $%02X (%3d)", cpu->get_x_register(),
					   cpu->get_x_register());

	ImGui::TextColored(RetroTheme::get_register_color(), "Y:  $%02X (%3d)", cpu->get_y_register(),
					   cpu->get_y_register());

	ImGui::TextColored(RetroTheme::get_register_color(), "PC: $%04X", cpu->get_program_counter());

	ImGui::TextColored(RetroTheme::get_register_color(), "SP: $%02X", cpu->get_stack_pointer());
}

void CPUStatePanel::render_flags(const nes::CPU6502 *cpu) {
	ImGui::Text("Status Flags:");

	uint8_t status = cpu->get_status_register();

	ImGui::TextColored((status & 0x80) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "N");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x40) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "V");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x20) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "-");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x10) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "B");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x08) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "D");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x04) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "I");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x02) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "Z");
	ImGui::SameLine();
	ImGui::TextColored((status & 0x01) ? RetroTheme::get_flag_active_color() : RetroTheme::get_flag_inactive_color(),
					   "C");

	ImGui::Text("Status: $%02X", status);
}

void CPUStatePanel::render_stack_info(const nes::CPU6502 *cpu) {
	ImGui::Text("Stack Info:");
	ImGui::TextColored(RetroTheme::get_address_color(), "Stack Pointer: $01%02X", cpu->get_stack_pointer());
	ImGui::TextColored(RetroTheme::get_address_color(), "Stack Top: $01FF");
}

void CPUStatePanel::render_controls(nes::CPU6502 *cpu) {
	ImGui::Separator();
	ImGui::Text("CPU Debug Controls:");

	// Step button - execute next instruction
	if (ImGui::Button("Step Instruction")) {
		cpu->execute_instruction();
	}

	ImGui::SameLine();

	// Reset button - trigger CPU reset
	if (ImGui::Button("Reset CPU")) {
		cpu->trigger_reset();
	}

	ImGui::SameLine();

	// Helper text
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step: Execute one CPU instruction\nReset: Restart CPU from reset vector");
	}
}

} // namespace nes::gui
