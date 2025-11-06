#pragma once

#include "core/types.hpp"

// Forward declarations
namespace nes {
class CPU6502;
class SystemBus;
} // namespace nes

namespace nes::gui {

/**
 * Panel for displaying disassembled instructions
 * Shows current instruction and surrounding code
 */
class DisassemblerPanel {
  public:
	DisassemblerPanel();
	~DisassemblerPanel() = default;

	// Render the disassembler panel
	void render(const nes::CPU6502 *cpu, const nes::SystemBus *bus);

	// Show/hide panel
	void set_visible(bool visible) {
		visible_ = visible;
	}
	bool is_visible() const {
		return visible_;
	}

  private:
	bool visible_;
	uint16_t follow_pc_;	 // Whether to follow program counter
	uint16_t start_address_; // Starting address for disassembly

	// Instruction alignment tracking to maintain consistency
	uint16_t last_pc_;								  // Last PC we computed alignment for
	size_t last_pc_index_;							  // Index of last_pc_ in cached_instruction_stream_
	std::vector<uint16_t> cached_instruction_stream_; // Cached instruction addresses
	bool alignment_valid_;							  // Whether our cached alignment is still valid

	// Helper methods
	void render_controls();
	void render_instruction_list(const nes::CPU6502 *cpu, const nes::SystemBus *bus);
	void render_single_instruction(uint16_t addr, uint16_t current_pc, const nes::SystemBus *bus);
	std::vector<uint16_t> find_instructions_before_pc(uint16_t pc, const nes::SystemBus *bus, int count);
	void update_instruction_stream(uint16_t pc, const nes::SystemBus *bus);
};

} // namespace nes::gui
