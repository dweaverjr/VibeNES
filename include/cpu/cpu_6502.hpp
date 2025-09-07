#pragma once

#include "core/component.hpp"
#include "core/types.hpp"

namespace nes {

// Forward declaration
class SystemBus;

/// 6502 CPU Core
/// Implements the MOS Technology 6502 microprocessor used in the NES
class CPU6502 final : public Component {
  public:
	explicit CPU6502(SystemBus *bus);
	~CPU6502() = default;

	// Component interface
	void tick(CpuCycle cycles) override;
	void reset() override;
	void power_on() override;
	[[nodiscard]] const char *get_name() const noexcept override;

	// CPU execution
	void execute_instruction();

	// Register access (for debugging and testing)
	[[nodiscard]] Byte get_accumulator() const noexcept {
		return accumulator_;
	}
	[[nodiscard]] Byte get_x_register() const noexcept {
		return x_register_;
	}
	[[nodiscard]] Byte get_y_register() const noexcept {
		return y_register_;
	}
	[[nodiscard]] Byte get_stack_pointer() const noexcept {
		return stack_pointer_;
	}
	[[nodiscard]] Address get_program_counter() const noexcept {
		return program_counter_;
	}
	[[nodiscard]] Byte get_status_register() const noexcept {
		return status_register_;
	}

	// Individual flag access
	[[nodiscard]] bool get_carry_flag() const noexcept {
		return carry_flag_;
	}
	[[nodiscard]] bool get_zero_flag() const noexcept {
		return zero_flag_;
	}
	[[nodiscard]] bool get_interrupt_flag() const noexcept {
		return interrupt_flag_;
	}
	[[nodiscard]] bool get_decimal_flag() const noexcept {
		return decimal_flag_;
	}
	[[nodiscard]] bool get_break_flag() const noexcept {
		return break_flag_;
	}
	[[nodiscard]] bool get_overflow_flag() const noexcept {
		return overflow_flag_;
	}
	[[nodiscard]] bool get_negative_flag() const noexcept {
		return negative_flag_;
	}

	// Test interface - allows setting registers for testing
	void set_accumulator(Byte value) noexcept {
		accumulator_ = value;
	}
	void set_x_register(Byte value) noexcept {
		x_register_ = value;
	}
	void set_y_register(Byte value) noexcept {
		y_register_ = value;
	}
	void set_program_counter(Address value) noexcept {
		program_counter_ = value;
	}

  private:
	// 6502 Registers
	Byte accumulator_;		  // A register
	Byte x_register_;		  // X register
	Byte y_register_;		  // Y register
	Byte stack_pointer_;	  // S register (points to stack location)
	Address program_counter_; // PC register

	// Status flags (P register) - using union for individual flag access
	union {
		Byte status_register_;
		struct {
			bool carry_flag_ : 1;	  // C - Carry flag
			bool zero_flag_ : 1;	  // Z - Zero flag
			bool interrupt_flag_ : 1; // I - Interrupt disable flag
			bool decimal_flag_ : 1;	  // D - Decimal mode flag (unused on NES)
			bool break_flag_ : 1;	  // B - Break flag
			bool unused_flag_ : 1;	  // - Always set to 1
			bool overflow_flag_ : 1;  // V - Overflow flag
			bool negative_flag_ : 1;  // N - Negative flag
		};
	};

	// Bus connection
	SystemBus *bus_;

	// Cycle tracking
	CpuCycle cycles_remaining_;

	// Memory access methods
	[[nodiscard]] Byte read_byte(Address address);
	void write_byte(Address address, Byte value);
	[[nodiscard]] Address read_word(Address address); // Little-endian 16-bit read

	// Stack operations
	void push_byte(Byte value);
	[[nodiscard]] Byte pull_byte();
	void push_word(Address value);
	[[nodiscard]] Address pull_word();

	// Flag update helpers
	void update_zero_flag(Byte value) noexcept;
	void update_negative_flag(Byte value) noexcept;
	void update_zero_and_negative_flags(Byte value) noexcept;

	// Instruction implementations - Start with immediate mode instructions
	void LDA_immediate(); // Load Accumulator with immediate value
	void LDX_immediate(); // Load X Register with immediate value
	void LDY_immediate(); // Load Y Register with immediate value

	// Register transfer instructions
	void TAX(); // Transfer Accumulator to X
	void TAY(); // Transfer Accumulator to Y
	void TXA(); // Transfer X to Accumulator
	void TYA(); // Transfer Y to Accumulator

	// No operation
	void NOP(); // No Operation
};

} // namespace nes
