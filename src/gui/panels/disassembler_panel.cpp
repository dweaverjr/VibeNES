#include "gui/panels/disassembler_panel.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "gui/style/retro_theme.hpp"
#include <algorithm>
#include <vector>

namespace nes::gui {

// 6502 instruction sizes lookup table (indexed by opcode)
static constexpr uint8_t INSTRUCTION_SIZES[256] = {
	// 0x00-0x0F
	1, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0x10-0x1F
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0x20-0x2F
	3, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0x30-0x3F
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0x40-0x4F
	1, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0x50-0x5F
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0x60-0x6F
	1, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0x70-0x7F
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0x80-0x8F
	2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0x90-0x9F
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0xA0-0xAF
	2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0xB0-0xBF
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0xC0-0xCF
	2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0xD0-0xDF
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
	// 0xE0-0xEF
	2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
	// 0xF0-0xFF
	2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3};

static uint8_t get_instruction_size(uint8_t opcode) {
	return INSTRUCTION_SIZES[opcode];
}

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

	// Fixed layout: always show exactly 8 instructions before PC, PC, then 10 after
	std::vector<uint16_t> instruction_addresses;

	// Try to find proper instruction boundaries before PC
	std::vector<uint16_t> before_pc = find_instructions_before_pc(current_pc, bus, 8);

	// FORCE exactly 8 addresses before PC for consistent layout
	std::vector<uint16_t> fixed_before_pc;
	if (before_pc.size() >= 8) {
		// Take the last 8
		fixed_before_pc.assign(before_pc.end() - 8, before_pc.end());
	} else {
		// Pad with estimated addresses to always have 8
		int needed = 8 - static_cast<int>(before_pc.size());
		for (int i = needed; i > 0; --i) {
			uint16_t addr = current_pc >= (i * 2) ? current_pc - (i * 2) : 0;
			fixed_before_pc.push_back(addr);
		}
		// Add whatever we found from the proper scan
		fixed_before_pc.insert(fixed_before_pc.end(), before_pc.begin(), before_pc.end());
	}

	// Add the fixed 8 addresses before PC
	instruction_addresses.insert(instruction_addresses.end(), fixed_before_pc.begin(), fixed_before_pc.end());

	// Add the current PC (this will always be at position 8 in the display)
	instruction_addresses.push_back(current_pc);

	// Add exactly 10 instructions after the PC (this is straightforward)
	uint16_t scan_addr = current_pc;
	for (int i = 0; i < 10; ++i) {
		uint8_t opcode = bus->read(scan_addr);
		uint8_t size = get_instruction_size(opcode);

		// Check for wraparound before adding
		if (scan_addr > 0xFFFF - size)
			break; // Would wraparound

		scan_addr += size;
		instruction_addresses.push_back(scan_addr);
	}

	// Render each instruction - PC will always be at the same vertical position
	for (uint16_t addr : instruction_addresses) {
		render_single_instruction(addr, current_pc, bus);
	}
}

std::vector<uint16_t> DisassemblerPanel::find_instructions_before_pc(uint16_t pc, const nes::SystemBus *bus,
																	 int count) {
	std::vector<uint16_t> candidates;

	// Simple approach: try a few different starting points and pick the best one
	std::vector<uint16_t> best_sequence;

	// Try starting points from 3 to 30 bytes before PC
	for (int start_offset = 3; start_offset <= 30 && start_offset < pc; start_offset += 1) {
		uint16_t start_addr = pc - start_offset;
		std::vector<uint16_t> sequence;
		uint16_t scan_addr = start_addr;

		// Scan forward and collect instruction addresses
		for (int steps = 0; steps < 25; ++steps) {
			if (scan_addr >= pc)
				break; // Stop when we reach or pass the PC

			sequence.push_back(scan_addr);

			uint8_t opcode = bus->read(scan_addr);
			uint8_t size = get_instruction_size(opcode);

			if (size < 1 || size > 3)
				break; // Invalid instruction

			scan_addr += size;

			// If we hit the PC exactly, this is a perfect alignment
			if (scan_addr == pc) {
				// Take the last 'count' instructions
				if (sequence.size() > static_cast<size_t>(count)) {
					best_sequence.assign(sequence.end() - count, sequence.end());
				} else {
					best_sequence = sequence;
				}
				return best_sequence; // Return immediately - perfect match
			}
		}
	}

	// If no perfect alignment found, create a simple fallback
	// Just go back by estimated instruction sizes
	for (int i = count; i > 0; --i) {
		uint16_t addr = pc >= (i * 2) ? pc - (i * 2) : 0;
		candidates.push_back(addr);
	}

	return candidates;
}

void DisassemblerPanel::render_single_instruction(uint16_t addr, uint16_t current_pc, const nes::SystemBus *bus) {
	uint8_t opcode = bus->read(addr);
	uint8_t size = get_instruction_size(opcode);

	// Create a fixed-width layout using ImGui columns or careful spacing
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0)); // Tighter spacing

	// Column 1: Current instruction indicator (fixed width)
	if (addr == current_pc) {
		ImGui::TextColored(RetroTheme::get_current_instruction_color(), ">");
	} else {
		ImGui::Text(" ");
	}
	ImGui::SameLine();

	// Column 2: Address (fixed width)
	ImGui::TextColored(RetroTheme::get_address_color(), "$%04X:", addr);
	ImGui::SameLine();

	// Column 3: Hex bytes (fixed width - always show 3 bytes worth of space)
	char hex_bytes[10] = "         "; // 9 spaces for padding
	for (uint8_t i = 0; i < size && i < 3; ++i) {
		uint8_t byte = bus->read(addr + i);
		sprintf(&hex_bytes[i * 3], "%02X ", byte);
	}
	hex_bytes[9] = '\0'; // Ensure null termination

	ImGui::TextColored(RetroTheme::get_hex_color(), "%-9s", hex_bytes);
	ImGui::SameLine();

	// Column 4: Separator
	ImGui::TextColored(RetroTheme::get_address_color(), "|");
	ImGui::SameLine();

	// Column 5: Disassembly
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White text for instructions

	// Complete 6502 disassembly - all 256 opcodes
	switch (opcode) {
	// Control Instructions
	case 0x00:
		ImGui::Text("BRK");
		break;
	case 0x40:
		ImGui::Text("RTI");
		break;
	case 0x60:
		ImGui::Text("RTS");
		break;
	case 0xEA:
		ImGui::Text("NOP");
		break;

	// Jump/Call Instructions
	case 0x4C: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("JMP $%04X", target);
		break;
	}
	case 0x6C: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("JMP ($%04X)", target);
		break;
	}
	case 0x20: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("JSR $%04X", target);
		break;
	}

	// Branch Instructions
	case 0x10:
		ImGui::Text("BPL $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0x30:
		ImGui::Text("BMI $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0x50:
		ImGui::Text("BVC $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0x70:
		ImGui::Text("BVS $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0x90:
		ImGui::Text("BCC $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0xB0:
		ImGui::Text("BCS $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0xD0:
		ImGui::Text("BNE $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;
	case 0xF0:
		ImGui::Text("BEQ $%04X", (addr + 2 + (int8_t)bus->read(addr + 1)) & 0xFFFF);
		break;

	// Load Accumulator (LDA)
	case 0xA9:
		ImGui::Text("LDA #$%02X", bus->read(addr + 1));
		break;
	case 0xA5:
		ImGui::Text("LDA $%02X", bus->read(addr + 1));
		break;
	case 0xB5:
		ImGui::Text("LDA $%02X,X", bus->read(addr + 1));
		break;
	case 0xAD: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDA $%04X", target);
		break;
	}
	case 0xBD: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDA $%04X,X", target);
		break;
	}
	case 0xB9: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDA $%04X,Y", target);
		break;
	}
	case 0xA1:
		ImGui::Text("LDA ($%02X,X)", bus->read(addr + 1));
		break;
	case 0xB1:
		ImGui::Text("LDA ($%02X),Y", bus->read(addr + 1));
		break;

	// Load X Register (LDX)
	case 0xA2:
		ImGui::Text("LDX #$%02X", bus->read(addr + 1));
		break;
	case 0xA6:
		ImGui::Text("LDX $%02X", bus->read(addr + 1));
		break;
	case 0xB6:
		ImGui::Text("LDX $%02X,Y", bus->read(addr + 1));
		break;
	case 0xAE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDX $%04X", target);
		break;
	}
	case 0xBE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDX $%04X,Y", target);
		break;
	}

	// Load Y Register (LDY)
	case 0xA0:
		ImGui::Text("LDY #$%02X", bus->read(addr + 1));
		break;
	case 0xA4:
		ImGui::Text("LDY $%02X", bus->read(addr + 1));
		break;
	case 0xB4:
		ImGui::Text("LDY $%02X,X", bus->read(addr + 1));
		break;
	case 0xAC: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDY $%04X", target);
		break;
	}
	case 0xBC: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LDY $%04X,X", target);
		break;
	}

	// Store Accumulator (STA)
	case 0x85:
		ImGui::Text("STA $%02X", bus->read(addr + 1));
		break;
	case 0x95:
		ImGui::Text("STA $%02X,X", bus->read(addr + 1));
		break;
	case 0x8D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("STA $%04X", target);
		break;
	}
	case 0x9D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("STA $%04X,X", target);
		break;
	}
	case 0x99: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("STA $%04X,Y", target);
		break;
	}
	case 0x81:
		ImGui::Text("STA ($%02X,X)", bus->read(addr + 1));
		break;
	case 0x91:
		ImGui::Text("STA ($%02X),Y", bus->read(addr + 1));
		break;

	// Store X Register (STX)
	case 0x86:
		ImGui::Text("STX $%02X", bus->read(addr + 1));
		break;
	case 0x96:
		ImGui::Text("STX $%02X,Y", bus->read(addr + 1));
		break;
	case 0x8E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("STX $%04X", target);
		break;
	}

	// Store Y Register (STY)
	case 0x84:
		ImGui::Text("STY $%02X", bus->read(addr + 1));
		break;
	case 0x94:
		ImGui::Text("STY $%02X,X", bus->read(addr + 1));
		break;
	case 0x8C: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("STY $%04X", target);
		break;
	}

	// Transfer Instructions
	case 0xAA:
		ImGui::Text("TAX");
		break;
	case 0xA8:
		ImGui::Text("TAY");
		break;
	case 0xBA:
		ImGui::Text("TSX");
		break;
	case 0x8A:
		ImGui::Text("TXA");
		break;
	case 0x98:
		ImGui::Text("TYA");
		break;
	case 0x9A:
		ImGui::Text("TXS");
		break;

	// Stack Instructions
	case 0x48:
		ImGui::Text("PHA");
		break;
	case 0x68:
		ImGui::Text("PLA");
		break;
	case 0x08:
		ImGui::Text("PHP");
		break;
	case 0x28:
		ImGui::Text("PLP");
		break;

	// Flag Instructions
	case 0x18:
		ImGui::Text("CLC");
		break;
	case 0x38:
		ImGui::Text("SEC");
		break;
	case 0x58:
		ImGui::Text("CLI");
		break;
	case 0x78:
		ImGui::Text("SEI");
		break;
	case 0xB8:
		ImGui::Text("CLV");
		break;
	case 0xD8:
		ImGui::Text("CLD");
		break;
	case 0xF8:
		ImGui::Text("SED");
		break;

	// Increment/Decrement
	case 0xE8:
		ImGui::Text("INX");
		break;
	case 0xC8:
		ImGui::Text("INY");
		break;
	case 0xCA:
		ImGui::Text("DEX");
		break;
	case 0x88:
		ImGui::Text("DEY");
		break;

	// Arithmetic - Add with Carry (ADC)
	case 0x69:
		ImGui::Text("ADC #$%02X", bus->read(addr + 1));
		break;
	case 0x65:
		ImGui::Text("ADC $%02X", bus->read(addr + 1));
		break;
	case 0x75:
		ImGui::Text("ADC $%02X,X", bus->read(addr + 1));
		break;
	case 0x6D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ADC $%04X", target);
		break;
	}
	case 0x7D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ADC $%04X,X", target);
		break;
	}
	case 0x79: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ADC $%04X,Y", target);
		break;
	}
	case 0x61:
		ImGui::Text("ADC ($%02X,X)", bus->read(addr + 1));
		break;
	case 0x71:
		ImGui::Text("ADC ($%02X),Y", bus->read(addr + 1));
		break;

	// Arithmetic - Subtract with Carry (SBC)
	case 0xE9:
		ImGui::Text("SBC #$%02X", bus->read(addr + 1));
		break;
	case 0xE5:
		ImGui::Text("SBC $%02X", bus->read(addr + 1));
		break;
	case 0xF5:
		ImGui::Text("SBC $%02X,X", bus->read(addr + 1));
		break;
	case 0xED: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("SBC $%04X", target);
		break;
	}
	case 0xFD: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("SBC $%04X,X", target);
		break;
	}
	case 0xF9: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("SBC $%04X,Y", target);
		break;
	}
	case 0xE1:
		ImGui::Text("SBC ($%02X,X)", bus->read(addr + 1));
		break;
	case 0xF1:
		ImGui::Text("SBC ($%02X),Y", bus->read(addr + 1));
		break;

	// Logic - AND
	case 0x29:
		ImGui::Text("AND #$%02X", bus->read(addr + 1));
		break;
	case 0x25:
		ImGui::Text("AND $%02X", bus->read(addr + 1));
		break;
	case 0x35:
		ImGui::Text("AND $%02X,X", bus->read(addr + 1));
		break;
	case 0x2D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("AND $%04X", target);
		break;
	}
	case 0x3D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("AND $%04X,X", target);
		break;
	}
	case 0x39: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("AND $%04X,Y", target);
		break;
	}
	case 0x21:
		ImGui::Text("AND ($%02X,X)", bus->read(addr + 1));
		break;
	case 0x31:
		ImGui::Text("AND ($%02X),Y", bus->read(addr + 1));
		break;

	// Logic - OR (ORA)
	case 0x09:
		ImGui::Text("ORA #$%02X", bus->read(addr + 1));
		break;
	case 0x05:
		ImGui::Text("ORA $%02X", bus->read(addr + 1));
		break;
	case 0x15:
		ImGui::Text("ORA $%02X,X", bus->read(addr + 1));
		break;
	case 0x0D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ORA $%04X", target);
		break;
	}
	case 0x1D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ORA $%04X,X", target);
		break;
	}
	case 0x19: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ORA $%04X,Y", target);
		break;
	}
	case 0x01:
		ImGui::Text("ORA ($%02X,X)", bus->read(addr + 1));
		break;
	case 0x11:
		ImGui::Text("ORA ($%02X),Y", bus->read(addr + 1));
		break;

	// Logic - Exclusive OR (EOR)
	case 0x49:
		ImGui::Text("EOR #$%02X", bus->read(addr + 1));
		break;
	case 0x45:
		ImGui::Text("EOR $%02X", bus->read(addr + 1));
		break;
	case 0x55:
		ImGui::Text("EOR $%02X,X", bus->read(addr + 1));
		break;
	case 0x4D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("EOR $%04X", target);
		break;
	}
	case 0x5D: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("EOR $%04X,X", target);
		break;
	}
	case 0x59: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("EOR $%04X,Y", target);
		break;
	}
	case 0x41:
		ImGui::Text("EOR ($%02X,X)", bus->read(addr + 1));
		break;
	case 0x51:
		ImGui::Text("EOR ($%02X),Y", bus->read(addr + 1));
		break;

	// Compare - CMP
	case 0xC9:
		ImGui::Text("CMP #$%02X", bus->read(addr + 1));
		break;
	case 0xC5:
		ImGui::Text("CMP $%02X", bus->read(addr + 1));
		break;
	case 0xD5:
		ImGui::Text("CMP $%02X,X", bus->read(addr + 1));
		break;
	case 0xCD: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("CMP $%04X", target);
		break;
	}
	case 0xDD: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("CMP $%04X,X", target);
		break;
	}
	case 0xD9: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("CMP $%04X,Y", target);
		break;
	}
	case 0xC1:
		ImGui::Text("CMP ($%02X,X)", bus->read(addr + 1));
		break;
	case 0xD1:
		ImGui::Text("CMP ($%02X),Y", bus->read(addr + 1));
		break;

	// Compare - CPX
	case 0xE0:
		ImGui::Text("CPX #$%02X", bus->read(addr + 1));
		break;
	case 0xE4:
		ImGui::Text("CPX $%02X", bus->read(addr + 1));
		break;
	case 0xEC: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("CPX $%04X", target);
		break;
	}

	// Compare - CPY
	case 0xC0:
		ImGui::Text("CPY #$%02X", bus->read(addr + 1));
		break;
	case 0xC4:
		ImGui::Text("CPY $%02X", bus->read(addr + 1));
		break;
	case 0xCC: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("CPY $%04X", target);
		break;
	}

	// Bit Test
	case 0x24:
		ImGui::Text("BIT $%02X", bus->read(addr + 1));
		break;
	case 0x2C: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("BIT $%04X", target);
		break;
	}

	// Shift Left (ASL)
	case 0x0A:
		ImGui::Text("ASL A");
		break;
	case 0x06:
		ImGui::Text("ASL $%02X", bus->read(addr + 1));
		break;
	case 0x16:
		ImGui::Text("ASL $%02X,X", bus->read(addr + 1));
		break;
	case 0x0E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ASL $%04X", target);
		break;
	}
	case 0x1E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ASL $%04X,X", target);
		break;
	}

	// Shift Right (LSR)
	case 0x4A:
		ImGui::Text("LSR A");
		break;
	case 0x46:
		ImGui::Text("LSR $%02X", bus->read(addr + 1));
		break;
	case 0x56:
		ImGui::Text("LSR $%02X,X", bus->read(addr + 1));
		break;
	case 0x4E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LSR $%04X", target);
		break;
	}
	case 0x5E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("LSR $%04X,X", target);
		break;
	}

	// Rotate Left (ROL)
	case 0x2A:
		ImGui::Text("ROL A");
		break;
	case 0x26:
		ImGui::Text("ROL $%02X", bus->read(addr + 1));
		break;
	case 0x36:
		ImGui::Text("ROL $%02X,X", bus->read(addr + 1));
		break;
	case 0x2E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ROL $%04X", target);
		break;
	}
	case 0x3E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ROL $%04X,X", target);
		break;
	}

	// Rotate Right (ROR)
	case 0x6A:
		ImGui::Text("ROR A");
		break;
	case 0x66:
		ImGui::Text("ROR $%02X", bus->read(addr + 1));
		break;
	case 0x76:
		ImGui::Text("ROR $%02X,X", bus->read(addr + 1));
		break;
	case 0x6E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ROR $%04X", target);
		break;
	}
	case 0x7E: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("ROR $%04X,X", target);
		break;
	}

	// Increment Memory (INC)
	case 0xE6:
		ImGui::Text("INC $%02X", bus->read(addr + 1));
		break;
	case 0xF6:
		ImGui::Text("INC $%02X,X", bus->read(addr + 1));
		break;
	case 0xEE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("INC $%04X", target);
		break;
	}
	case 0xFE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("INC $%04X,X", target);
		break;
	}

	// Decrement Memory (DEC)
	case 0xC6:
		ImGui::Text("DEC $%02X", bus->read(addr + 1));
		break;
	case 0xD6:
		ImGui::Text("DEC $%02X,X", bus->read(addr + 1));
		break;
	case 0xCE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("DEC $%04X", target);
		break;
	}
	case 0xDE: {
		uint16_t target = bus->read(addr + 1) | (bus->read(addr + 2) << 8);
		ImGui::Text("DEC $%04X,X", target);
		break;
	}

	// Illegal/Undocumented opcodes (common ones)
	case 0x04:
	case 0x44:
	case 0x64: // NOP zp
		ImGui::Text("*NOP $%02X", bus->read(addr + 1));
		break;
	case 0x0C: // NOP abs
		ImGui::Text("*NOP $%04X", bus->read(addr + 1) | (bus->read(addr + 2) << 8));
		break;
	case 0x14:
	case 0x34:
	case 0x54:
	case 0x74:
	case 0xD4:
	case 0xF4: // NOP zp,X
		ImGui::Text("*NOP $%02X,X", bus->read(addr + 1));
		break;
	case 0x1A:
	case 0x3A:
	case 0x5A:
	case 0x7A:
	case 0xDA:
	case 0xFA: // NOP implied
		ImGui::Text("*NOP");
		break;
	case 0x1C:
	case 0x3C:
	case 0x5C:
	case 0x7C:
	case 0xDC:
	case 0xFC: // NOP abs,X
		ImGui::Text("*NOP $%04X,X", bus->read(addr + 1) | (bus->read(addr + 2) << 8));
		break;
	case 0x80:
	case 0x82:
	case 0x89:
	case 0xC2:
	case 0xE2: // NOP #imm
		ImGui::Text("*NOP #$%02X", bus->read(addr + 1));
		break;

	default:
		ImGui::TextColored(RetroTheme::get_flag_inactive_color(), "???");
		break;
	}

	// Restore ImGui styles
	ImGui::PopStyleColor(); // Restore text color
	ImGui::PopStyleVar();	// Restore item spacing
}

} // namespace nes::gui
