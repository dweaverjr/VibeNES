#pragma once

#include "core/bus.hpp"
#include <cstdint>

namespace nes {

/**
 * AudioPanel - Audio control interface
 *
 * Provides GUI controls for audio settings:
 * - Enable/disable audio
 * - Volume control
 * - Visual audio level meter
 * - Sample rate and buffer info
 */
class AudioPanel {
  public:
	AudioPanel();

	/**
	 * Render the audio panel (inline, not as a window)
	 * @param bus System bus for audio access
	 */
	void render(SystemBus *bus);

  private:
	float volume_slider_;	// Volume slider value [0.0, 1.0]
	bool audio_enabled_;	// Audio enable checkbox
	float audio_level_;		// Current audio level for meter
	int peak_hold_counter_; // Peak hold for level meter

	// Render sub-components
	void render_controls(SystemBus *bus);
	void render_level_meter(SystemBus *bus);
	void render_info(SystemBus *bus);
};

} // namespace nes
