#pragma once

#include "core/types.hpp"

// Forward declarations
namespace nes {
class SystemBus;
}

namespace nes::gui {

/**
 * Panel for viewing memory contents in hex format
 * Allows browsing different memory regions
 */
class MemoryViewerPanel {
  public:
	MemoryViewerPanel();
	~MemoryViewerPanel() = default;

	// Render the memory viewer panel
	void render(const nes::SystemBus *bus);

	// Show/hide panel
	void set_visible(bool visible) {
		visible_ = visible;
	}
	bool is_visible() const {
		return visible_;
	}

  private:
	bool visible_;
	uint16_t start_address_;
	uint16_t bytes_per_row_;
	uint16_t rows_to_show_;

	// Helper methods
	void render_controls();
	void render_memory_grid(const nes::SystemBus *bus);
	void render_ascii_column(const nes::SystemBus *bus, uint16_t start_addr, uint16_t count);
};

} // namespace nes::gui
