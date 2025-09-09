#pragma once

#include "core/types.hpp"

namespace nes {

/// 6502 Interrupt Vector Addresses
/// These are the fixed addresses where the CPU looks for interrupt handler addresses
constexpr Address NMI_VECTOR = 0xFFFA;	 ///< Non-Maskable Interrupt vector
constexpr Address RESET_VECTOR = 0xFFFC; ///< Reset vector (power-on, reset button)
constexpr Address IRQ_VECTOR = 0xFFFE;	 ///< IRQ/BRK Interrupt vector

/// Interrupt Types
enum class InterruptType {
	NONE,  ///< No interrupt pending
	RESET, ///< Reset interrupt (highest priority)
	NMI,   ///< Non-Maskable Interrupt (second priority)
	IRQ	   ///< Maskable Interrupt (lowest priority)
};

/// Interrupt State
/// Tracks pending interrupts and their priority
struct InterruptState {
	bool nmi_pending = false;				 ///< NMI triggered by PPU VBlank, DMC, etc.
	bool irq_pending = false;				 ///< IRQ triggered by APU, mappers, etc.
	bool reset_pending = false;				 ///< Reset triggered by reset button, power-on
	bool irq_enabled_when_triggered = false; ///< Track if IRQ was triggered while interrupts were enabled

	/// Get the highest priority pending interrupt
	[[nodiscard]] InterruptType get_pending_interrupt() const noexcept {
		if (reset_pending)
			return InterruptType::RESET;
		if (nmi_pending)
			return InterruptType::NMI;
		if (irq_pending)
			return InterruptType::IRQ;
		return InterruptType::NONE;
	}

	/// Clear the specified interrupt
	void clear_interrupt(InterruptType type) noexcept {
		switch (type) {
		case InterruptType::RESET:
			reset_pending = false;
			break;
		case InterruptType::NMI:
			nmi_pending = false;
			break;
		case InterruptType::IRQ:
			irq_pending = false;
			irq_enabled_when_triggered = false;
			break;
		case InterruptType::NONE:
			break;
		}
	}

	/// Clear all pending interrupts
	void clear_all() noexcept {
		nmi_pending = false;
		irq_pending = false;
		reset_pending = false;
		irq_enabled_when_triggered = false;
	}
};

} // namespace nes
