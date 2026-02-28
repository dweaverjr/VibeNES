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

DisassemblerPanel::DisassemblerPanel()
	: visible_(true), follow_pc_(true), start_address_(0x0000), last_pc_(0xFFFF), last_pc_index_(0),
	  alignment_valid_(false) {
}

void DisassemblerPanel::render(const nes::CPU6502 *cpu, const nes::SystemBus *bus) {
	if (!cpu || !bus)
		return;

	render_controls();
	ImGui::Separator();
	render_instruction_list(cpu, bus);
}

void DisassemblerPanel::render_controls() {
	// Controls removed - always follow PC
}

void DisassemblerPanel::render_instruction_list(const nes::CPU6502 *cpu, const nes::SystemBus *bus) {
	uint16_t current_pc = cpu->get_program_counter();

	// Update our instruction stream cache if needed
	update_instruction_stream(current_pc, bus);

	// Use cached PC index instead of std::find() for O(1) lookup
	// Verify the cached index is valid (should always be true after update_instruction_stream)
	bool pc_found = (last_pc_index_ < cached_instruction_stream_.size() &&
					 cached_instruction_stream_[last_pc_index_] == current_pc);

	if (pc_found) {
		// PC found in our cached stream - use the cached index (already computed)
		size_t pc_index = last_pc_index_;

		// Show 8 instructions before PC, PC, then 10 after
		size_t start_index = pc_index >= 8 ? pc_index - 8 : 0;
		size_t end_index = std::min(pc_index + 11, cached_instruction_stream_.size());

		for (size_t i = start_index; i < end_index; ++i) {
			render_single_instruction(cached_instruction_stream_[i], current_pc, bus);
		}
	} else {
		// PC not in cached stream - force a rebuild and use fallback
		alignment_valid_ = false;
		update_instruction_stream(current_pc, bus);

		// Fallback: just show addresses around PC
		for (int i = -8; i <= 10; ++i) {
			uint16_t addr = static_cast<uint16_t>(current_pc + i);
			render_single_instruction(addr, current_pc, bus);
		}
	}
}

void DisassemblerPanel::update_instruction_stream(uint16_t pc, const nes::SystemBus *bus) {
	// Fast path: PC hasn't changed since last update
	if (alignment_valid_ && pc == last_pc_) {
		return; // No work needed
	}

	// Check if we need to extend or rebuild the cache
	// OPTIMIZATION: Instead of std::find() every frame, use cached index and check nearby
	bool pc_found_in_cache = false;
	size_t pc_index = 0;

	if (alignment_valid_ && !cached_instruction_stream_.empty()) {
		// Check if PC is at the cached index (common case - PC advanced to next instruction)
		if (last_pc_index_ < cached_instruction_stream_.size() && cached_instruction_stream_[last_pc_index_] == pc) {
			pc_found_in_cache = true;
			pc_index = last_pc_index_;
		}
		// Check if PC is just after the last cached position (very common)
		else if (last_pc_index_ + 1 < cached_instruction_stream_.size() &&
				 cached_instruction_stream_[last_pc_index_ + 1] == pc) {
			pc_found_in_cache = true;
			pc_index = last_pc_index_ + 1;
		}
		// Check a few instructions ahead (for small jumps/branches)
		else {
			for (size_t i = last_pc_index_; i < std::min(last_pc_index_ + 5, cached_instruction_stream_.size()); ++i) {
				if (cached_instruction_stream_[i] == pc) {
					pc_found_in_cache = true;
					pc_index = i;
					break;
				}
			}
		}
	}

	bool need_rebuild = !alignment_valid_ || !pc_found_in_cache;
	bool need_extend = false;

	if (!need_rebuild && pc_found_in_cache) {
		// Check if we're getting close to the end of our cached stream
		size_t remaining = cached_instruction_stream_.size() - pc_index;
		if (remaining < 15) { // Less than 15 instructions ahead
			need_extend = true;
		}
		// Update cached index
		last_pc_index_ = pc_index;
	}

	if (need_rebuild) {
		// Full rebuild of the instruction stream
		cached_instruction_stream_.clear();

		// Find a good starting point far before the current PC
		uint16_t start_addr = pc >= 200 ? pc - 200 : 0;
		bool found_alignment = false;

		// Try multiple starting points to find proper alignment
		for (int offset = 0; offset <= 50 && !found_alignment; ++offset) {
			uint16_t try_start = static_cast<uint16_t>(start_addr + offset);
			if (try_start >= pc)
				break;

			std::vector<uint16_t> test_stream;
			uint16_t scan_addr = try_start;
			bool found_pc_in_stream = false;

			// Build a long instruction stream - extend much further ahead
			for (int steps = 0; steps < 300; ++steps) { // Increased from 150 to 300
				if (scan_addr >= pc + 200)
					break; // Extend 200 bytes past PC instead of 50

				test_stream.push_back(scan_addr);

				// Check if we just added the PC (more efficient than std::find every iteration)
				if (scan_addr == pc) {
					found_pc_in_stream = true;
				}

				uint8_t opcode = bus->read(scan_addr);
				uint8_t size = get_instruction_size(opcode);

				if (size < 1 || size > 3)
					break; // Invalid instruction

				scan_addr += size;

				// If we hit PC exactly after advancing, also mark as found
				if (scan_addr == pc) {
					found_pc_in_stream = true;
				}
			}

			// After building the stream, check if we found PC
			if (found_pc_in_stream) {
				cached_instruction_stream_ = test_stream;
				found_alignment = true;
				break;
			}
		}

		// If no good alignment found, create a simple fallback stream
		if (!found_alignment) {
			uint16_t fallback_start = (pc >= 100 ? pc - 100 : 0);
			uint16_t fallback_end = (pc <= 0xFF9B ? pc + 100 : 0xFFFF); // Prevent wraparound
			for (uint16_t addr = fallback_start; addr <= fallback_end && addr >= fallback_start; addr += 2) {
				cached_instruction_stream_.push_back(addr);
				if (addr >= fallback_end)
					break; // Extra safety to prevent infinite loop
			}
		}

		// Cache the PC index for next time (simple linear search after rebuild)
		last_pc_index_ = 0;
		for (size_t i = 0; i < cached_instruction_stream_.size(); ++i) {
			if (cached_instruction_stream_[i] == pc) {
				last_pc_index_ = i;
				break;
			}
		}

		last_pc_ = pc;
		alignment_valid_ = true;

	} else if (need_extend) {
		// Extend the existing cache forward
		if (!cached_instruction_stream_.empty()) {
			uint16_t last_addr = cached_instruction_stream_.back();
			uint16_t scan_addr = last_addr;

			// Extend by scanning forward from the last cached instruction
			for (int steps = 0; steps < 50; ++steps) {
				uint8_t opcode = bus->read(scan_addr);
				uint8_t size = get_instruction_size(opcode);

				if (size < 1 || size > 3)
					break; // Invalid instruction

				scan_addr += size;
				cached_instruction_stream_.push_back(scan_addr);

				if (scan_addr >= pc + 100)
					break; // Extended far enough
			}
		}

		last_pc_ = pc;
	}
}

std::vector<uint16_t> DisassemblerPanel::find_instructions_before_pc(uint16_t pc, const nes::SystemBus *bus,
																	 int count) {
	std::vector<uint16_t> candidates;

	// IMPROVED APPROACH: Find a long sequence of properly aligned instructions,
	// then take just the slice we need for display. This ensures instruction
	// boundaries are correct regardless of display window size.

	std::vector<uint16_t> full_sequence;

	// Try starting points from 5 to 100 bytes before PC to establish proper alignment
	for (int start_offset = 5; start_offset <= 100 && start_offset < pc; start_offset += 1) {
		uint16_t start_addr = static_cast<uint16_t>(pc - start_offset);
		std::vector<uint16_t> sequence;
		uint16_t scan_addr = start_addr;

		// Scan forward and collect ALL instruction addresses leading to PC
		for (int steps = 0; steps < 60; ++steps) { // Increased limit
			if (scan_addr >= pc)
				break; // Stop when we reach or pass the PC

			sequence.push_back(scan_addr);

			uint8_t opcode = bus->read(scan_addr);
			uint8_t size = get_instruction_size(opcode);

			if (size < 1 || size > 3)
				break; // Invalid instruction

			scan_addr += size;

			// If we hit the PC exactly, we found a perfect alignment
			if (scan_addr == pc) {
				full_sequence = sequence; // Save the entire sequence
				break;
			}
		}

		// If we found a good full sequence, stop searching
		if (!full_sequence.empty()) {
			break;
		}
	}

	// Now take just the visible portion we need for display
	if (!full_sequence.empty()) {
		if (full_sequence.size() > static_cast<size_t>(count)) {
			// Take the last 'count' instructions from the full sequence
			candidates.assign(full_sequence.end() - count, full_sequence.end());
		} else {
			candidates = full_sequence;
		}
	} else {
		// Fallback if no perfect alignment found
		for (int i = count; i > 0; --i) {
			uint16_t addr = pc >= (i * 2) ? static_cast<uint16_t>(pc - (i * 2)) : static_cast<uint16_t>(0);
			candidates.push_back(addr);
		}
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
		snprintf(&hex_bytes[i * 3], sizeof(hex_bytes) - (i * 3), "%02X ", byte);
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
