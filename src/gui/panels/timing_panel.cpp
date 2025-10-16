#include "gui/panels/timing_panel.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "ppu/ppu.hpp"

#include <imgui.h>
#include <iomanip>
#include <sstream>

namespace nes::gui {

TimingPanel::TimingPanel()
	: last_cpu_cycles_(0), last_ppu_cycles_(0), frame_count_(0), cpu_frequency_hz_(0.0f), ppu_frequency_hz_(0.0f),
	  frame_rate_fps_(0.0f) {
}

void TimingPanel::render(nes::CPU6502 *cpu, nes::PPU *ppu, nes::SystemBus *bus) {
	(void)bus; // Unused parameter - reserved for future bus timing analysis

	if (ImGui::BeginChild("TimingInfo", ImVec2(0, 0), true)) {
		ImGui::Text("TIMING & SYNCHRONIZATION");
		ImGui::Separator();

		// CPU Timing Section
		if (ImGui::CollapsingHeader("CPU Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
			render_cpu_timing(cpu);
		}

		ImGui::Spacing();

		// PPU Timing Section
		if (ImGui::CollapsingHeader("PPU Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
			render_ppu_timing(ppu);
		}

		ImGui::Spacing();
	}
	ImGui::EndChild();
}

void TimingPanel::render_cpu_timing(nes::CPU6502 *cpu) {
	if (!cpu) {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CPU not connected");
		return;
	}

	// We need to add a method to get total cycles from CPU
	// For now, show what we can access
	ImGui::Text("Status: Connected");

	// Show CPU registers and state
	ImGui::Text("PC: $%04X", cpu->get_program_counter());
	ImGui::Text("A: $%02X  X: $%02X  Y: $%02X", cpu->get_accumulator(), cpu->get_x_register(), cpu->get_y_register());
	ImGui::Text("SP: $%02X  P: $%02X", cpu->get_stack_pointer(), cpu->get_status_register());

	// Show target frequency
	ImGui::Separator();
	ImGui::Text("Target Frequency: 1.789773 MHz");
	ImGui::Text("Target Period: 558.73 ns");

	// TODO: Add total cycle counter to CPU class
	ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Note: Total cycle counter needed in CPU");
}

void TimingPanel::render_ppu_timing(nes::PPU *ppu) {
	if (!ppu) {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "PPU not connected");
		return;
	}

	ImGui::Text("Status: Connected");

	// Show PPU timing information
	uint16_t current_scanline = ppu->get_current_scanline();
	uint16_t current_cycle = ppu->get_current_cycle();

	ImGui::Text("Current Scanline: %d", current_scanline);
	ImGui::Text("Current Cycle: %d", current_cycle);

	// Debug output to see if we're getting valid values
	if (current_scanline == 0 && current_cycle == 0) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "WARNING: PPU appears to be stuck at 0,0");
		ImGui::Text("This suggests PPU is not being ticked");
	}

	// Calculate total PPU cycles (approximate)
	std::uint64_t total_ppu_cycles = static_cast<std::uint64_t>(current_scanline) * 341 + current_cycle;
	ImGui::Text("Total PPU Cycles: %s", format_cycles(total_ppu_cycles).c_str());

	// Show target frequency
	ImGui::Separator();
	ImGui::Text("Target Frequency: 5.369318 MHz");
	ImGui::Text("Target Period: 186.24 ns");
	ImGui::Text("Cycles per Scanline: 341");
	ImGui::Text("Scanlines per Frame: 262");

	// Frame information
	if (ppu->get_current_scanline() <= 239) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Visible Scanline");
	} else if (ppu->get_current_scanline() == 240) {
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Post-render Scanline");
	} else if (ppu->get_current_scanline() >= 241 && ppu->get_current_scanline() <= 260) {
		ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "VBlank Period");
	} else if (ppu->get_current_scanline() == 261) {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Pre-render Scanline");
	}
}

void TimingPanel::render_synchronization_info(nes::CPU6502 *cpu, nes::PPU *ppu) {
	if (!cpu || !ppu) {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CPU or PPU not connected");
		return;
	}

	// Show synchronization status
	ImGui::Text("CPU-PPU Ratio: 1:3 (CPU:PPU)");

	// Calculate theoretical PPU cycles based on CPU
	// This would need actual CPU cycle counter
	ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Sync analysis requires CPU cycle counter");

	// Show frame timing
	ImGui::Separator();
	ImGui::Text("Frame Timing:");
	ImGui::Text("CPU cycles/frame: ~29,780");
	ImGui::Text("PPU cycles/frame: ~89,342");
	ImGui::Text("Target FPS: 60.0988");

	// VBlank status
	if (ppu->get_current_scanline() >= 241 && ppu->get_current_scanline() <= 260) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● VBlank Active");
	} else {
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ VBlank Inactive");
	}
}

void TimingPanel::render_performance_metrics() {
	// Show real-time performance
	ImGui::Text("Real-time Performance:");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

	// Show ImGui metrics
	ImGui::Separator();
	ImGui::Text("Render Statistics:");
	ImGui::Text("Draw Calls: %d", ImGui::GetIO().MetricsRenderVertices / 3); // Rough estimate
	ImGui::Text("Vertices: %d", ImGui::GetIO().MetricsRenderVertices);
	ImGui::Text("Windows: %d", ImGui::GetIO().MetricsRenderWindows);
}

std::string TimingPanel::format_cycles(std::uint64_t cycles) const {
	if (cycles >= 1000000) {
		return std::to_string(cycles / 1000000) + "." + std::to_string((cycles / 100000) % 10) + "M";
	} else if (cycles >= 1000) {
		return std::to_string(cycles / 1000) + "." + std::to_string((cycles / 100) % 10) + "K";
	} else {
		return std::to_string(cycles);
	}
}

std::string TimingPanel::format_frequency(float frequency) const {
	std::stringstream ss;
	if (frequency >= 1000000.0f) {
		ss << std::fixed << std::setprecision(2) << (frequency / 1000000.0f) << " MHz";
	} else if (frequency >= 1000.0f) {
		ss << std::fixed << std::setprecision(2) << (frequency / 1000.0f) << " kHz";
	} else {
		ss << std::fixed << std::setprecision(2) << frequency << " Hz";
	}
	return ss.str();
}

} // namespace nes::gui
