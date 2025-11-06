#pragma once

#include "core/component.hpp"
#include "core/types.hpp"
#include "cpu/interrupts.hpp"

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
	[[nodiscard]] int execute_instruction(); // Returns number of cycles consumed

	// Interrupt handling
	void trigger_nmi() noexcept;	///< Trigger Non-Maskable Interrupt (PPU VBlank, etc.)
	void clear_nmi_line() noexcept; ///< Clear NMI line (when VBlank flag is cleared by reading $2002)
	void trigger_irq() noexcept;	///< Trigger Maskable Interrupt (APU, mappers, etc.)
	void trigger_reset() noexcept;	///< Trigger Reset (reset button, power-on)
	void clear_irq_line() noexcept; ///< Clear IRQ line (for edge detection when source is acknowledged)

	[[nodiscard]] bool has_pending_interrupt() const noexcept;
	[[nodiscard]] InterruptType get_pending_interrupt() const noexcept;

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
	void set_stack_pointer(Byte value) noexcept {
		stack_pointer_ = value;
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

	// Save state serialization
	void serialize_state(std::vector<uint8_t> &buffer) const;
	void deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset);

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

	// Interrupt state
	InterruptState interrupt_state_;
	bool irq_line_ = false; // External IRQ line state (for level-triggered IRQ)
	bool nmi_line_ = false; // External NMI line state (for edge-triggered NMI)

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

	// Interrupt handling
	void handle_nmi();		   ///< Handle Non-Maskable Interrupt
	void handle_irq();		   ///< Handle Maskable Interrupt (IRQ/BRK)
	void handle_reset();	   ///< Handle Reset interrupt
	void process_interrupts(); ///< Check and process pending interrupts

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

	// Shift/Rotate instructions - ASL (Arithmetic Shift Left)
	void ASL_accumulator(); // ASL A
	void ASL_zero_page();	// ASL zp
	void ASL_zero_page_X(); // ASL zp,X
	void ASL_absolute();	// ASL abs
	void ASL_absolute_X();	// ASL abs,X

	// Shift/Rotate instructions - LSR (Logical Shift Right)
	void LSR_accumulator(); // LSR A
	void LSR_zero_page();	// LSR zp
	void LSR_zero_page_X(); // LSR zp,X
	void LSR_absolute();	// LSR abs
	void LSR_absolute_X();	// LSR abs,X

	// Shift/Rotate instructions - ROL (Rotate Left)
	void ROL_accumulator(); // ROL A
	void ROL_zero_page();	// ROL zp
	void ROL_zero_page_X(); // ROL zp,X
	void ROL_absolute();	// ROL abs
	void ROL_absolute_X();	// ROL abs,X

	// Shift/Rotate instructions - ROR (Rotate Right)
	void ROR_accumulator(); // ROR A
	void ROR_zero_page();	// ROR zp
	void ROR_zero_page_X(); // ROR zp,X
	void ROR_absolute();	// ROR abs
	void ROR_absolute_X();	// ROR abs,X

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

	// Branch instructions (relative addressing mode)
	void BPL_relative(); // Branch if Plus/Positive (N = 0)
	void BMI_relative(); // Branch if Minus/Negative (N = 1)
	void BVC_relative(); // Branch if Overflow Clear (V = 0)
	void BVS_relative(); // Branch if Overflow Set (V = 1)
	void BCC_relative(); // Branch if Carry Clear (C = 0)
	void BCS_relative(); // Branch if Carry Set (C = 1)
	void BNE_relative(); // Branch if Not Equal/Zero Clear (Z = 0)
	void BEQ_relative(); // Branch if Equal/Zero Set (Z = 1)

	// Jump and Subroutine instructions
	void JMP_absolute(); // Jump to absolute address
	void JMP_indirect(); // Jump to address stored at given address (with page boundary bug)
	void JSR();			 // Jump to Subroutine
	void RTS();			 // Return from Subroutine
	void RTI();			 // Return from Interrupt

	// Stack operations
	void PHA(); // Push Accumulator
	void PLA(); // Pull Accumulator
	void PHP(); // Push Processor Status
	void PLP(); // Pull Processor Status

	// Status flag instructions
	void CLC(); // Clear Carry Flag
	void SEC(); // Set Carry Flag
	void CLI(); // Clear Interrupt Flag
	void SEI(); // Set Interrupt Flag
	void CLV(); // Clear Overflow Flag
	void CLD(); // Clear Decimal Flag
	void SED(); // Set Decimal Flag

	// Transfer instructions (remaining)
	void TXS(); // Transfer X to Stack Pointer
	void TSX(); // Transfer Stack Pointer to X

	// Bit test instructions
	void BIT_zero_page(); // BIT zp
	void BIT_absolute();  // BIT abs

	// System instructions
	void BRK(); // Break

	// No operation
	void NOP(); // No Operation

	// ===== UNDOCUMENTED/UNOFFICIAL OPCODES =====
	// These are stable undocumented opcodes that are safe to implement
	// and are actually used by some NES games

	// LAX - Load Accumulator and X Register (combination of LDA + TAX)
	void LAX_zero_page();		 // LAX zp
	void LAX_zero_page_Y();		 // LAX zp,Y
	void LAX_absolute();		 // LAX abs
	void LAX_absolute_Y();		 // LAX abs,Y
	void LAX_indexed_indirect(); // LAX (zp,X)
	void LAX_indirect_indexed(); // LAX (zp),Y

	// SAX - Store Accumulator AND X Register
	void SAX_zero_page();		 // SAX zp
	void SAX_zero_page_Y();		 // SAX zp,Y
	void SAX_absolute();		 // SAX abs
	void SAX_indexed_indirect(); // SAX (zp,X)

	// DCP - Decrement and Compare (combination of DEC + CMP)
	void DCP_zero_page();		 // DCP zp
	void DCP_zero_page_X();		 // DCP zp,X
	void DCP_absolute();		 // DCP abs
	void DCP_absolute_X();		 // DCP abs,X
	void DCP_absolute_Y();		 // DCP abs,Y
	void DCP_indexed_indirect(); // DCP (zp,X)
	void DCP_indirect_indexed(); // DCP (zp),Y

	// ISC/ISB - Increment and Subtract with Carry (combination of INC + SBC)
	void ISC_zero_page();		 // ISC zp
	void ISC_zero_page_X();		 // ISC zp,X
	void ISC_absolute();		 // ISC abs
	void ISC_absolute_X();		 // ISC abs,X
	void ISC_absolute_Y();		 // ISC abs,Y
	void ISC_indexed_indirect(); // ISC (zp,X)
	void ISC_indirect_indexed(); // ISC (zp),Y

	// SLO - Shift Left and OR (combination of ASL + ORA)
	void SLO_zero_page();		 // SLO zp
	void SLO_zero_page_X();		 // SLO zp,X
	void SLO_absolute();		 // SLO abs
	void SLO_absolute_X();		 // SLO abs,X
	void SLO_absolute_Y();		 // SLO abs,Y
	void SLO_indexed_indirect(); // SLO (zp,X)
	void SLO_indirect_indexed(); // SLO (zp),Y

	// RLA - Rotate Left and AND (combination of ROL + AND)
	void RLA_zero_page();		 // RLA zp
	void RLA_zero_page_X();		 // RLA zp,X
	void RLA_absolute();		 // RLA abs
	void RLA_absolute_X();		 // RLA abs,X
	void RLA_absolute_Y();		 // RLA abs,Y
	void RLA_indexed_indirect(); // RLA (zp,X)
	void RLA_indirect_indexed(); // RLA (zp),Y

	// SRE - Shift Right and EOR (combination of LSR + EOR)
	void SRE_zero_page();		 // SRE zp
	void SRE_zero_page_X();		 // SRE zp,X
	void SRE_absolute();		 // SRE abs
	void SRE_absolute_X();		 // SRE abs,X
	void SRE_absolute_Y();		 // SRE abs,Y
	void SRE_indexed_indirect(); // SRE (zp,X)
	void SRE_indirect_indexed(); // SRE (zp),Y

	// RRA - Rotate Right and Add (combination of ROR + ADC)
	void RRA_zero_page();		 // RRA zp
	void RRA_zero_page_X();		 // RRA zp,X
	void RRA_absolute();		 // RRA abs
	void RRA_absolute_X();		 // RRA abs,X
	void RRA_absolute_Y();		 // RRA abs,Y
	void RRA_indexed_indirect(); // RRA (zp,X)
	void RRA_indirect_indexed(); // RRA (zp),Y

	// Undocumented NOPs with different cycle counts and addressing modes
	void NOP_immediate();	// NOP #value (2 cycles)
	void NOP_zero_page();	// NOP zp (3 cycles)
	void NOP_zero_page_X(); // NOP zp,X (4 cycles)
	void NOP_absolute();	// NOP abs (4 cycles)
	void NOP_absolute_X();	// NOP abs,X (4/5 cycles)

	// Highly unstable opcodes - these will crash/halt the CPU
	void CRASH(); // For highly unstable opcodes that should halt execution
};

} // namespace nes
