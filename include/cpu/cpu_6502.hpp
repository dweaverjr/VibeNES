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

	// Disable copy constructor and copy assignment operator (non-copyable)
	CPU6502(const CPU6502 &) = delete;
	CPU6502 &operator=(const CPU6502 &) = delete;

	// Default move constructor and move assignment operator
	CPU6502(CPU6502 &&) = default;
	CPU6502 &operator=(CPU6502 &&) = default;

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
		return status_.status_register_;
	}

	// Individual flag access
	[[nodiscard]] bool get_carry_flag() const noexcept {
		return status_.flags.carry_flag_;
	}
	[[nodiscard]] bool get_zero_flag() const noexcept {
		return status_.flags.zero_flag_;
	}
	[[nodiscard]] bool get_interrupt_flag() const noexcept {
		return status_.flags.interrupt_flag_;
	}
	[[nodiscard]] bool get_decimal_flag() const noexcept {
		return status_.flags.decimal_flag_;
	}
	[[nodiscard]] bool get_break_flag() const noexcept {
		return status_.flags.break_flag_;
	}
	[[nodiscard]] bool get_overflow_flag() const noexcept {
		return status_.flags.overflow_flag_;
	}
	[[nodiscard]] bool get_negative_flag() const noexcept {
		return status_.flags.negative_flag_;
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
	void set_carry_flag(bool value) noexcept {
		status_.flags.carry_flag_ = value;
	}
	void set_zero_flag(bool value) noexcept {
		status_.flags.zero_flag_ = value;
	}
	void set_interrupt_flag(bool value) noexcept {
		status_.flags.interrupt_flag_ = value;
	}
	void set_decimal_flag(bool value) noexcept {
		status_.flags.decimal_flag_ = value;
	}
	void set_break_flag(bool value) noexcept {
		status_.flags.break_flag_ = value;
	}
	void set_overflow_flag(bool value) noexcept {
		status_.flags.overflow_flag_ = value;
	}
	void set_negative_flag(bool value) noexcept {
		status_.flags.negative_flag_ = value;
	}

  private:
	// 6502 Registers
	Byte accumulator_;		  // A register
	Byte x_register_;		  // X register
	Byte y_register_;		  // Y register
	Byte stack_pointer_;	  // S register (points to stack location)
	Address program_counter_; // PC register

	// Status flags (P register) - using union for individual flag access
	union StatusRegister {
		Byte status_register_;
		struct Flags {
			bool carry_flag_ : 1;	  // C - Carry flag
			bool zero_flag_ : 1;	  // Z - Zero flag
			bool interrupt_flag_ : 1; // I - Interrupt disable flag
			bool decimal_flag_ : 1;	  // D - Decimal mode flag (unused on NES)
			bool break_flag_ : 1;	  // B - Break flag
			bool unused_flag_ : 1;	  // - Always set to 1
			bool overflow_flag_ : 1;  // V - Overflow flag
			bool negative_flag_ : 1;  // N - Negative flag
		} flags;
	} status_;

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

	// Arithmetic operation helpers
	void perform_adc(Byte value) noexcept;
	void perform_sbc(Byte value) noexcept;
	void perform_compare(Byte register_value, Byte memory_value) noexcept;

	// Cycle management helpers
	void consume_cycle() noexcept;
	void consume_cycles(int count) noexcept;

	// Addressing mode helpers
	[[nodiscard]] bool crosses_page_boundary(Address base_address, Byte offset) const noexcept;

	// Instruction implementations - Start with immediate mode instructions
	void LDA_immediate(); // Load Accumulator with immediate value
	void LDX_immediate(); // Load X Register with immediate value
	void LDY_immediate(); // Load Y Register with immediate value

	// Zero Page addressing mode instructions (fast 2-byte instructions)
	void LDA_zero_page(); // Load Accumulator from zero page address
	void LDX_zero_page(); // Load X Register from zero page address
	void LDY_zero_page(); // Load Y Register from zero page address
	void STA_zero_page(); // Store Accumulator to zero page address
	void STX_zero_page(); // Store X Register to zero page address
	void STY_zero_page(); // Store Y Register to zero page address

	// Zero Page,X addressing mode instructions (indexed zero page)
	void LDA_zero_page_X(); // Load Accumulator from zero page,X address
	void LDY_zero_page_X(); // Load Y Register from zero page,X address
	void STA_zero_page_X(); // Store Accumulator to zero page,X address
	void STY_zero_page_X(); // Store Y Register to zero page,X address

	// Zero Page,Y addressing mode instructions (indexed zero page)
	void LDX_zero_page_Y(); // Load X Register from zero page,Y address
	void STX_zero_page_Y(); // Store X Register to zero page,Y address

	// Absolute addressing mode instructions (3-byte instructions)
	void LDA_absolute(); // Load Accumulator from absolute address
	void LDX_absolute(); // Load X Register from absolute address
	void LDY_absolute(); // Load Y Register from absolute address
	void STA_absolute(); // Store Accumulator to absolute address
	void STX_absolute(); // Store X Register to absolute address
	void STY_absolute(); // Store Y Register to absolute address

	// Absolute,X addressing mode instructions (with page boundary crossing)
	void LDA_absolute_X(); // Load Accumulator from absolute,X address
	void LDY_absolute_X(); // Load Y Register from absolute,X address
	void STA_absolute_X(); // Store Accumulator to absolute,X address

	// Absolute,Y addressing mode instructions (with page boundary crossing)
	void LDA_absolute_Y(); // Load Accumulator from absolute,Y address
	void LDX_absolute_Y(); // Load X Register from absolute,Y address
	void STA_absolute_Y(); // Store Accumulator to absolute,Y address

	// Indexed Indirect (zp,X) addressing mode instructions
	void LDA_indexed_indirect(); // Load Accumulator from (zp,X) address
	void STA_indexed_indirect(); // Store Accumulator to (zp,X) address

	// Indirect Indexed (zp),Y addressing mode instructions
	void LDA_indirect_indexed(); // Load Accumulator from (zp),Y address
	void STA_indirect_indexed(); // Store Accumulator to (zp),Y address

	// Register transfer instructions
	void TAX(); // Transfer Accumulator to X
	void TAY(); // Transfer Accumulator to Y
	void TXA(); // Transfer X to Accumulator
	void TYA(); // Transfer Y to Accumulator

	// Arithmetic instructions - ADC (Add with Carry)
	void ADC_immediate();		 // ADC #value
	void ADC_zero_page();		 // ADC zp
	void ADC_zero_page_X();		 // ADC zp,X
	void ADC_absolute();		 // ADC abs
	void ADC_absolute_X();		 // ADC abs,X
	void ADC_absolute_Y();		 // ADC abs,Y
	void ADC_indexed_indirect(); // ADC (zp,X)
	void ADC_indirect_indexed(); // ADC (zp),Y

	// Arithmetic instructions - SBC (Subtract with Carry)
	void SBC_immediate();		 // SBC #value
	void SBC_zero_page();		 // SBC zp
	void SBC_zero_page_X();		 // SBC zp,X
	void SBC_absolute();		 // SBC abs
	void SBC_absolute_X();		 // SBC abs,X
	void SBC_absolute_Y();		 // SBC abs,Y
	void SBC_indexed_indirect(); // SBC (zp,X)
	void SBC_indirect_indexed(); // SBC (zp),Y

	// Compare instructions - CMP (Compare with Accumulator)
	void CMP_immediate();		 // CMP #value
	void CMP_zero_page();		 // CMP zp
	void CMP_zero_page_X();		 // CMP zp,X
	void CMP_absolute();		 // CMP abs
	void CMP_absolute_X();		 // CMP abs,X
	void CMP_absolute_Y();		 // CMP abs,Y
	void CMP_indexed_indirect(); // CMP (zp,X)
	void CMP_indirect_indexed(); // CMP (zp),Y

	// Compare instructions - CPX (Compare with X Register)
	void CPX_immediate(); // CPX #value
	void CPX_zero_page(); // CPX zp
	void CPX_absolute();  // CPX abs

	// Compare instructions - CPY (Compare with Y Register)
	void CPY_immediate(); // CPY #value
	void CPY_zero_page(); // CPY zp
	void CPY_absolute();  // CPY abs

	// Logical instructions - AND (Bitwise AND with Accumulator)
	void AND_immediate();		 // AND #value
	void AND_zero_page();		 // AND zp
	void AND_zero_page_X();		 // AND zp,X
	void AND_absolute();		 // AND abs
	void AND_absolute_X();		 // AND abs,X
	void AND_absolute_Y();		 // AND abs,Y
	void AND_indexed_indirect(); // AND (zp,X)
	void AND_indirect_indexed(); // AND (zp),Y

	// Logical instructions - ORA (Bitwise OR with Accumulator)
	void ORA_immediate();		 // ORA #value
	void ORA_zero_page();		 // ORA zp
	void ORA_zero_page_X();		 // ORA zp,X
	void ORA_absolute();		 // ORA abs
	void ORA_absolute_X();		 // ORA abs,X
	void ORA_absolute_Y();		 // ORA abs,Y
	void ORA_indexed_indirect(); // ORA (zp,X)
	void ORA_indirect_indexed(); // ORA (zp),Y

	// Logical instructions - EOR (Bitwise Exclusive OR with Accumulator)
	void EOR_immediate();		 // EOR #value
	void EOR_zero_page();		 // EOR zp
	void EOR_zero_page_X();		 // EOR zp,X
	void EOR_absolute();		 // EOR abs
	void EOR_absolute_X();		 // EOR abs,X
	void EOR_absolute_Y();		 // EOR abs,Y
	void EOR_indexed_indirect(); // EOR (zp,X)
	void EOR_indirect_indexed(); // EOR (zp),Y

	// Increment/Decrement instructions - Register operations
	void INX(); // Increment X Register
	void INY(); // Increment Y Register
	void DEX(); // Decrement X Register
	void DEY(); // Decrement Y Register

	// Increment/Decrement instructions - Memory operations
	void INC_zero_page();	// INC zp
	void INC_zero_page_X(); // INC zp,X
	void INC_absolute();	// INC abs
	void INC_absolute_X();	// INC abs,X
	void DEC_zero_page();	// DEC zp
	void DEC_zero_page_X(); // DEC zp,X
	void DEC_absolute();	// DEC abs
	void DEC_absolute_X();	// DEC abs,X

	// No operation
	void NOP(); // No Operation
};

} // namespace nes
