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

	// Helper methods
	void render_controls();
	void render_instruction_list(const nes::CPU6502 *cpu, const nes::SystemBus *bus);
};

} // namespace nes::gui
