#pragma once

#include "core/types.hpp"
#include <functional>

// Forward declarations
namespace nes {
class CPU6502;
}

namespace nes::gui {

/**
 * Panel for displaying CPU state information
 * Shows registers, flags, and current instruction
 */
class CPUStatePanel {
  public:
	CPUStatePanel();
	~CPUStatePanel() = default;

	// Render the CPU state panel
	void render(nes::CPU6502 *cpu, std::function<void()> step_callback = nullptr);

	// Show/hide panel
	void set_visible(bool visible) {
		visible_ = visible;
	}
	bool is_visible() const {
		return visible_;
	}

  private:
	bool visible_;

	// Helper methods
	void render_controls(nes::CPU6502 *cpu, std::function<void()> step_callback = nullptr);
	void render_registers(const nes::CPU6502 *cpu);
	void render_flags(const nes::CPU6502 *cpu);
	void render_stack_info(const nes::CPU6502 *cpu);
};

} // namespace nes::gui
