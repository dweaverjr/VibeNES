#include "gui/panels/cpu_state_panel.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"

namespace nes::gui {

CPUStatePanel::CPUStatePanel() : visible_(true) {
}

void CPUStatePanel::render(nes::CPU6502 *cpu, std::function<void()> step_callback, std::function<void()> reset_callback,
						   std::function<void()> toggle_run_callback, bool is_running, bool run_enabled) {
	if (!cpu)
		return;

	render_controls(cpu, step_callback, reset_callback, toggle_run_callback, is_running, run_enabled);
	ImGui::Separator();

	// Create two-column layout for Registers and Flags side-by-side
	float column_width = ImGui::GetContentRegionAvail().x * 0.5f;

	// Left column: Registers
	if (ImGui::BeginChild("RegistersColumn", ImVec2(column_width, 0), false)) {
		render_registers(cpu);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right column: Status Flags
	if (ImGui::BeginChild("FlagsColumn", ImVec2(column_width, 0), true)) {
		render_flags(cpu);
	}
	ImGui::EndChild();
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

	ImGui::TextColored(RetroTheme::get_register_color(), "SP: $01%02X", cpu->get_stack_pointer());
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

void CPUStatePanel::render_controls(nes::CPU6502 *cpu, std::function<void()> step_callback,
									std::function<void()> reset_callback, std::function<void()> toggle_run_callback,
									bool is_running, bool run_enabled) {
	ImGui::Separator();
	ImGui::Text("CPU Debug Controls:");

	// First row of step buttons
	// Step button - execute next instruction (with repeat when held)
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step 1x")) {
		if (step_callback) {
			step_callback(); // Use the provided step callback for proper coordination
		} else {
			// Fallback to direct CPU execution if no callback provided
			(void)cpu->execute_instruction(); // Discard return value for manual stepping
		}
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();

	// Fast step button for rapid stepping (with repeat when held)
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step 100x")) {
		for (int i = 0; i < 100; ++i) {
			if (step_callback) {
				step_callback();
			} else {
				(void)cpu->execute_instruction(); // Discard return value for fast stepping
			}
		}
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();

	// Ultra fast step button for very rapid stepping (with repeat when held)
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step 1000x")) {
		for (int i = 0; i < 1000; ++i) {
			if (step_callback) {
				step_callback();
			} else {
				(void)cpu->execute_instruction(); // Discard return value for ultra fast stepping
			}
		}
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();

	const char *run_label = is_running ? "Pause" : "Run";
	if (!run_enabled) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button(run_label)) {
		if (toggle_run_callback) {
			toggle_run_callback();
		}
	}
	if (!run_enabled) {
		ImGui::EndDisabled();
	}

	// Second row of step buttons
	// Mega step button for extremely rapid stepping (with repeat when held)
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step 10,000x")) {
		for (int i = 0; i < 10000; ++i) {
			if (step_callback) {
				step_callback();
			} else {
				(void)cpu->execute_instruction(); // Discard return value for mega fast stepping
			}
		}
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();

	// Giga step button for massively rapid stepping (with repeat when held)
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step 100,000x")) {
		for (int i = 0; i < 100000; ++i) {
			if (step_callback) {
				step_callback();
			} else {
				(void)cpu->execute_instruction(); // Discard return value for giga fast stepping
			}
		}
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();

	// NES Reset button - trigger system-wide reset
	if (ImGui::Button("NES Reset")) {
		if (reset_callback) {
			reset_callback(); // Use the provided reset callback for proper system coordination
		} else {
			// Fallback to direct CPU reset if no callback provided
			cpu->trigger_reset();
		}
	}

	ImGui::SameLine();

	// Helper text
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step 1x: Execute one CPU instruction\n"
						  "Step 100x: Execute 100 CPU instructions\n"
						  "Step 1000x: Execute 1000 CPU instructions\n"
						  "Run/Pause: Toggle continuous execution at normal speed\n"
						  "Step 10,000x: Execute 10,000 CPU instructions\n"
						  "Step 100,000x: Execute 100,000 CPU instructions\n"
						  "NES Reset: Reset entire NES system to initial state\n"
						  "(Hold buttons to repeat rapidly)");
	}
}

} // namespace nes::gui
