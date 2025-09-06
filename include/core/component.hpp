#pragma once

#include "types.hpp"

namespace nes {

/// Base interface for all emulation components
/// Provides the fundamental contract that CPU, PPU, APU, and other components must implement
class Component {
  public:
	virtual ~Component() = default;

	/// Advance the component by the specified number of CPU cycles
	/// All components must implement cycle-accurate timing
	virtual void tick(CpuCycle cycles) = 0;

	/// Reset the component to its initial state
	/// Called when the NES reset button is pressed or system is reset
	virtual void reset() = 0;

	/// Power-on initialization
	/// Called when the system is first powered on (cold boot)
	/// Different from reset - sets initial power-on state
	virtual void power_on() = 0;

	/// Get a human-readable name for this component (useful for debugging)
	[[nodiscard]] virtual const char *get_name() const noexcept = 0;

  protected:
	// Protected constructor - this is an interface class
	Component() = default;

	// Non-copyable by default (components typically manage unique hardware state)
	Component(const Component &) = delete;
	Component &operator=(const Component &) = delete;

	// Movable by default (for container storage)
	Component(Component &&) = default;
	Component &operator=(Component &&) = default;
};

} // namespace nes
