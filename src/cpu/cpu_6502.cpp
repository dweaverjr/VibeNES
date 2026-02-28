#include "cpu/cpu_6502.hpp"
#include "core/bus.hpp"
#include <format>
#include <iostream>

namespace nes {

CPU6502::CPU6502(SystemBus *bus)
	: accumulator_(0), x_register_(0), y_register_(0), stack_pointer_(0xFF) // Stack starts at top
	  ,
	  program_counter_(0), status_({.status_register_ = 0x20}) // Unused flag always set
	  ,
	  bus_(bus), cycles_remaining_(0), interrupt_state_() {

	// Ensure unused flag is always set
	status_.flags.unused_flag_ = true;
}

void CPU6502::tick(CpuCycle cycles) {
	cycles_remaining_ += cycles;

	// Execute instructions while we have cycles.
	// OAM DMA is handled inside execute_instruction() — no special case needed.
	while (cycles_remaining_.count() > 0) {
		(void)execute_instruction();
	}
}
void CPU6502::reset() {
	// Manual reset (reset button pressed during operation)
	// This should trigger reset while preserving some operational context

	// Trigger a reset interrupt rather than immediately setting PC
	// This ensures proper reset vector handling
	trigger_reset();

	// Clear any other pending interrupts (reset takes priority)
	interrupt_state_.irq_pending = false;
	interrupt_state_.nmi_pending = false;

	// Reset will be processed in next execute_instruction() call
}

void CPU6502::power_on() {
	// On real NES, most CPU registers have undefined values on power-up
	// Only a few specific behaviors are guaranteed

	// Registers start with random/undefined values (simulate this)
	// Note: In real hardware these would be truly random, but we'll use
	// deterministic values for consistent debugging
	accumulator_ = 0x00; // Could be anything
	x_register_ = 0x00;	 // Could be anything
	y_register_ = 0x00;	 // Could be anything

	// Stack pointer: Real hardware behavior varies, but often starts around 0x00
	stack_pointer_ = 0x00; // Will be decremented by 3 during reset to reach 0xFD

	// Status register: Most bits undefined, but some have known behavior
	status_.status_register_ = 0x20;	  // Unused flag always set
	status_.flags.interrupt_flag_ = true; // Interrupts disabled on power-up

	// Program counter: Undefined until reset vector is read
	program_counter_ = 0x0000; // Will be set by reset sequence

	// Clear cycle counter
	cycles_remaining_ = CpuCycle{0};

	// Clear interrupt state
	interrupt_state_.clear_all();

	// *** CRITICAL: Power-on automatically triggers reset sequence ***
	// This is what actually sets PC from reset vector and completes initialization
	trigger_reset();
}

const char *CPU6502::get_name() const noexcept {
	return "6502 CPU";
}

// ============================================================================
// Interrupt Handling
// ============================================================================

void CPU6502::trigger_nmi() noexcept {
	// NMI is edge-triggered: only trigger on rising edge (0 -> 1 transition)
	if (!nmi_line_) {
		interrupt_state_.nmi_pending = true;
		nmi_line_ = true;
	}
}

void CPU6502::clear_nmi_line() noexcept {
	nmi_line_ = false;
}

void CPU6502::trigger_irq() noexcept {
	// IRQ line is now asserted
	irq_line_ = true;
	interrupt_state_.irq_pending = true;
}

void CPU6502::clear_irq_line() noexcept {
	irq_line_ = false;
	interrupt_state_.irq_pending = false;
}

void CPU6502::trigger_reset() noexcept {
	interrupt_state_.reset_pending = true;
}

bool CPU6502::has_pending_interrupt() const noexcept {
	return interrupt_state_.get_pending_interrupt() != InterruptType::NONE;
}

InterruptType CPU6502::get_pending_interrupt() const noexcept {
	return interrupt_state_.get_pending_interrupt();
}

void CPU6502::process_interrupts() {
	InterruptType pending = interrupt_state_.get_pending_interrupt();

	switch (pending) {
	case InterruptType::RESET:
		handle_reset();
		interrupt_state_.clear_interrupt(InterruptType::RESET);
		break;

	case InterruptType::NMI:
		handle_nmi();
		interrupt_state_.clear_interrupt(InterruptType::NMI);
		break;

	case InterruptType::IRQ:
		// Process IRQ only if interrupts are currently enabled (I flag is clear)
		if (!get_interrupt_flag()) {
			handle_irq();
			// NOTE: Do NOT clear irq_pending here - IRQ is level-triggered
			// The IRQ line stays asserted until software clears the source
			// (e.g., reading $4015 for APU frame IRQ)
		}
		break;

	case InterruptType::NONE:
		// No interrupt to process
		break;
	}
}

void CPU6502::handle_reset() {
	// Reset sequence takes 7 cycles total:
	// Cycles 1-2: Internal operations
	// Cycles 3-5: Suppressed pushes (PCH, PCL, P) — no bus writes, SP decrements
	// Cycles 6-7: Read reset vector low/high (via read_byte below)
	consume_cycles(5);

	// Read reset vector from $FFFC-$FFFD (2 cycles)
	Byte low_byte = read_byte(RESET_VECTOR);
	Byte high_byte = read_byte(RESET_VECTOR + 1);
	Address reset_address = make_word(low_byte, high_byte);

	program_counter_ = reset_address;

	// Reset CPU state
	stack_pointer_ -= 3;				  // Stack pointer decrements by 3 during reset (simulates 3 pushes)
	status_.flags.interrupt_flag_ = true; // Disable interrupts
	status_.flags.decimal_flag_ = false;  // Clear decimal mode

	// Note: Reset does NOT clear other pending interrupts, only the reset itself
	// Other interrupts remain pending and will be processed after reset
}

void CPU6502::handle_nmi() {
	// NMI sequence takes 7 cycles total:
	// Cycles 1-2: Internal operations (2 cycles below)
	// Cycles 3-4: Push PCH, PCL (via push_word — 2 write_byte calls)
	// Cycle 5:   Push P (via push_byte — 1 write_byte call)
	// Cycles 6-7: Read NMI vector (via read_word — 2 read_byte calls)
	consume_cycles(2);

	// Push return address (PC) onto stack
	push_word(program_counter_);

	// Push status register (with break flag clear for NMI)
	Byte status_to_push = status_.status_register_;
	status_to_push &= 0xEFu; // Clear break flag (bit 4)
	status_to_push |= 0x20u; // Set unused flag (bit 5)
	push_byte(status_to_push);

	// Set interrupt disable flag
	status_.flags.interrupt_flag_ = true;

	// Jump to NMI handler
	program_counter_ = read_word(NMI_VECTOR);
}

void CPU6502::handle_irq() {
	// IRQ/BRK sequence takes 7 cycles total:
	// Cycles 1-2: Internal operations (2 cycles below)
	// Cycles 3-4: Push PCH, PCL (via push_word — 2 write_byte calls)
	// Cycle 5:   Push P (via push_byte — 1 write_byte call)
	// Cycles 6-7: Read IRQ vector (via read_word — 2 read_byte calls)
	consume_cycles(2);

	// Push return address (PC) onto stack
	push_word(program_counter_);

	// Push status register (with break flag clear for IRQ)
	Byte status_to_push = status_.status_register_;
	status_to_push &= 0xEFu; // Clear break flag (bit 4)
	status_to_push |= 0x20u; // Set unused flag (bit 5)
	push_byte(status_to_push);

	// Set interrupt disable flag
	status_.flags.interrupt_flag_ = true;

	// Jump to IRQ handler
	program_counter_ = read_word(IRQ_VECTOR);
}

int CPU6502::execute_instruction() {
	// Reset per-instruction cycle counter (fat consume_cycle tracks this)
	cycles_consumed_ = 0;

	// OAM DMA takes priority — CPU is halted for the entire transfer
	if (bus_->is_oam_dma_pending()) {
		return execute_oam_dma();
	}

	// Check for pending interrupts before instruction fetch
	if (has_pending_interrupt()) {
		InterruptType pending = interrupt_state_.get_pending_interrupt();

		// Only process interrupts that will actually be handled
		bool should_process = false;
		switch (pending) {
		case InterruptType::RESET:
		case InterruptType::NMI:
			should_process = true; // Always processed
			break;
		case InterruptType::IRQ:
			// Process IRQ only if interrupts are currently enabled (I flag is clear)
			should_process = !get_interrupt_flag();
			break;
		case InterruptType::NONE:
			should_process = false;
			break;
		}
		if (should_process) {
			process_interrupts();
			// Return cycles consumed by interrupt processing
			return cycles_consumed_;
		}
	}

	// Fetch opcode
	Byte opcode = read_byte(program_counter_);
	program_counter_++;

	// Decode and execute
	switch (opcode) {
	// Load Accumulator - Immediate
	case 0xA9:
		LDA_immediate();
		break;

	// Load X Register - Immediate
	case 0xA2:
		LDX_immediate();
		break;

	// Load Y Register - Immediate
	case 0xA0:
		LDY_immediate();
		break;

	// Load Accumulator - Zero Page
	case 0xA5:
		LDA_zero_page();
		break;

	// Load X Register - Zero Page
	case 0xA6:
		LDX_zero_page();
		break;

	// Load Y Register - Zero Page
	case 0xA4:
		LDY_zero_page();
		break;

	// Store Accumulator - Zero Page
	case 0x85:
		STA_zero_page();
		break;

	// Store X Register - Zero Page
	case 0x86:
		STX_zero_page();
		break;

	// Store Y Register - Zero Page
	case 0x84:
		STY_zero_page();
		break;

	// Load Accumulator - Zero Page,X
	case 0xB5:
		LDA_zero_page_X();
		break;

	// Load Y Register - Zero Page,X
	case 0xB4:
		LDY_zero_page_X();
		break;

	// Store Accumulator - Zero Page,X
	case 0x95:
		STA_zero_page_X();
		break;

	// Store Y Register - Zero Page,X
	case 0x94:
		STY_zero_page_X();
		break;

	// Load X Register - Zero Page,Y
	case 0xB6:
		LDX_zero_page_Y();
		break;

	// Store X Register - Zero Page,Y
	case 0x96:
		STX_zero_page_Y();
		break;

	// Load Accumulator - Absolute
	case 0xAD:
		LDA_absolute();
		break;

	// Load X Register - Absolute
	case 0xAE:
		LDX_absolute();
		break;

	// Load Y Register - Absolute
	case 0xAC:
		LDY_absolute();
		break;

	// Store Accumulator - Absolute
	case 0x8D:
		STA_absolute();
		break;

	// Store X Register - Absolute
	case 0x8E:
		STX_absolute();
		break;

	// Store Y Register - Absolute
	case 0x8C:
		STY_absolute();
		break;

	// Load Accumulator - Absolute,X
	case 0xBD:
		LDA_absolute_X();
		break;

	// Load Y Register - Absolute,X
	case 0xBC:
		LDY_absolute_X();
		break;

	// Store Accumulator - Absolute,X
	case 0x9D:
		STA_absolute_X();
		break;

	// Load Accumulator - Absolute,Y
	case 0xB9:
		LDA_absolute_Y();
		break;

	// Load X Register - Absolute,Y
	case 0xBE:
		LDX_absolute_Y();
		break;

	// Store Accumulator - Absolute,Y
	case 0x99:
		STA_absolute_Y();
		break;

	// Load Accumulator - Indexed Indirect (zp,X)
	case 0xA1:
		LDA_indexed_indirect();
		break;

	// Store Accumulator - Indexed Indirect (zp,X)
	case 0x81:
		STA_indexed_indirect();
		break;

	// Load Accumulator - Indirect Indexed (zp),Y
	case 0xB1:
		LDA_indirect_indexed();
		break;

	// Store Accumulator - Indirect Indexed (zp),Y
	case 0x91:
		STA_indirect_indexed();
		break;

	// Transfer instructions
	case 0xAA:
		TAX();
		break;
	case 0xA8:
		TAY();
		break;
	case 0x8A:
		TXA();
		break;
	case 0x98:
		TYA();
		break;

	// ADC - Add with Carry
	case 0x69:
		ADC_immediate();
		break;
	case 0x65:
		ADC_zero_page();
		break;
	case 0x75:
		ADC_zero_page_X();
		break;
	case 0x6D:
		ADC_absolute();
		break;
	case 0x7D:
		ADC_absolute_X();
		break;
	case 0x79:
		ADC_absolute_Y();
		break;
	case 0x61:
		ADC_indexed_indirect();
		break;
	case 0x71:
		ADC_indirect_indexed();
		break;

	// SBC - Subtract with Carry
	case 0xE9:
	case 0xEB: // Undocumented duplicate of SBC immediate
		SBC_immediate();
		break;
	case 0xE5:
		SBC_zero_page();
		break;
	case 0xF5:
		SBC_zero_page_X();
		break;
	case 0xED:
		SBC_absolute();
		break;
	case 0xFD:
		SBC_absolute_X();
		break;
	case 0xF9:
		SBC_absolute_Y();
		break;
	case 0xE1:
		SBC_indexed_indirect();
		break;
	case 0xF1:
		SBC_indirect_indexed();
		break;

	// Compare Instructions - CMP (Compare with Accumulator)
	case 0xC9: // CMP - Immediate
		CMP_immediate();
		break;
	case 0xC5: // CMP - Zero Page
		CMP_zero_page();
		break;
	case 0xD5: // CMP - Zero Page,X
		CMP_zero_page_X();
		break;
	case 0xCD: // CMP - Absolute
		CMP_absolute();
		break;
	case 0xDD: // CMP - Absolute,X
		CMP_absolute_X();
		break;
	case 0xD9: // CMP - Absolute,Y
		CMP_absolute_Y();
		break;
	case 0xC1: // CMP - (Indirect,X)
		CMP_indexed_indirect();
		break;
	case 0xD1: // CMP - (Indirect),Y
		CMP_indirect_indexed();
		break;

	// Compare Instructions - CPX (Compare with X Register)
	case 0xE0: // CPX - Immediate
		CPX_immediate();
		break;
	case 0xE4: // CPX - Zero Page
		CPX_zero_page();
		break;
	case 0xEC: // CPX - Absolute
		CPX_absolute();
		break;

	// Compare Instructions - CPY (Compare with Y Register)
	case 0xC0: // CPY - Immediate
		CPY_immediate();
		break;
	case 0xC4: // CPY - Zero Page
		CPY_zero_page();
		break;
	case 0xCC: // CPY - Absolute
		CPY_absolute();
		break;

	// Logical Instructions - AND (Bitwise AND with Accumulator)
	case 0x29: // AND - Immediate
		AND_immediate();
		break;
	case 0x25: // AND - Zero Page
		AND_zero_page();
		break;
	case 0x35: // AND - Zero Page,X
		AND_zero_page_X();
		break;
	case 0x2D: // AND - Absolute
		AND_absolute();
		break;
	case 0x3D: // AND - Absolute,X
		AND_absolute_X();
		break;
	case 0x39: // AND - Absolute,Y
		AND_absolute_Y();
		break;
	case 0x21: // AND - (Indirect,X)
		AND_indexed_indirect();
		break;
	case 0x31: // AND - (Indirect),Y
		AND_indirect_indexed();
		break;

	// Logical Instructions - ORA (Bitwise OR with Accumulator)
	case 0x09: // ORA - Immediate
		ORA_immediate();
		break;
	case 0x05: // ORA - Zero Page
		ORA_zero_page();
		break;
	case 0x15: // ORA - Zero Page,X
		ORA_zero_page_X();
		break;
	case 0x0D: // ORA - Absolute
		ORA_absolute();
		break;
	case 0x1D: // ORA - Absolute,X
		ORA_absolute_X();
		break;
	case 0x19: // ORA - Absolute,Y
		ORA_absolute_Y();
		break;
	case 0x01: // ORA - (Indirect,X)
		ORA_indexed_indirect();
		break;
	case 0x11: // ORA - (Indirect),Y
		ORA_indirect_indexed();
		break;

	// Logical Instructions - EOR (Bitwise Exclusive OR with Accumulator)
	case 0x49: // EOR - Immediate
		EOR_immediate();
		break;
	case 0x45: // EOR - Zero Page
		EOR_zero_page();
		break;
	case 0x55: // EOR - Zero Page,X
		EOR_zero_page_X();
		break;
	case 0x4D: // EOR - Absolute
		EOR_absolute();
		break;
	case 0x5D: // EOR - Absolute,X
		EOR_absolute_X();
		break;
	case 0x59: // EOR - Absolute,Y
		EOR_absolute_Y();
		break;
	case 0x41: // EOR - (Indirect,X)
		EOR_indexed_indirect();
		break;
	case 0x51: // EOR - (Indirect),Y
		EOR_indirect_indexed();
		break;

	// Shift/Rotate Instructions - ASL (Arithmetic Shift Left)
	case 0x0A: // ASL - Accumulator
		ASL_accumulator();
		break;
	case 0x06: // ASL - Zero Page
		ASL_zero_page();
		break;
	case 0x16: // ASL - Zero Page,X
		ASL_zero_page_X();
		break;
	case 0x0E: // ASL - Absolute
		ASL_absolute();
		break;
	case 0x1E: // ASL - Absolute,X
		ASL_absolute_X();
		break;

	// Shift/Rotate Instructions - LSR (Logical Shift Right)
	case 0x4A: // LSR - Accumulator
		LSR_accumulator();
		break;
	case 0x46: // LSR - Zero Page
		LSR_zero_page();
		break;
	case 0x56: // LSR - Zero Page,X
		LSR_zero_page_X();
		break;
	case 0x4E: // LSR - Absolute
		LSR_absolute();
		break;
	case 0x5E: // LSR - Absolute,X
		LSR_absolute_X();
		break;

	// Shift/Rotate Instructions - ROL (Rotate Left)
	case 0x2A: // ROL - Accumulator
		ROL_accumulator();
		break;
	case 0x26: // ROL - Zero Page
		ROL_zero_page();
		break;
	case 0x36: // ROL - Zero Page,X
		ROL_zero_page_X();
		break;
	case 0x2E: // ROL - Absolute
		ROL_absolute();
		break;
	case 0x3E: // ROL - Absolute,X
		ROL_absolute_X();
		break;

	// Shift/Rotate Instructions - ROR (Rotate Right)
	case 0x6A: // ROR - Accumulator
		ROR_accumulator();
		break;
	case 0x66: // ROR - Zero Page
		ROR_zero_page();
		break;
	case 0x76: // ROR - Zero Page,X
		ROR_zero_page_X();
		break;
	case 0x6E: // ROR - Absolute
		ROR_absolute();
		break;
	case 0x7E: // ROR - Absolute,X
		ROR_absolute_X();
		break;

	// Increment/Decrement Instructions - Register Operations
	case 0xE8: // INX - Increment X Register
		INX();
		break;
	case 0xC8: // INY - Increment Y Register
		INY();
		break;
	case 0xCA: // DEX - Decrement X Register
		DEX();
		break;
	case 0x88: // DEY - Decrement Y Register
		DEY();
		break;

	// Increment/Decrement Instructions - Memory Operations
	case 0xE6: // INC Zero Page
		INC_zero_page();
		break;
	case 0xF6: // INC Zero Page,X
		INC_zero_page_X();
		break;
	case 0xEE: // INC Absolute
		INC_absolute();
		break;
	case 0xFE: // INC Absolute,X
		INC_absolute_X();
		break;
	case 0xC6: // DEC Zero Page
		DEC_zero_page();
		break;
	case 0xD6: // DEC Zero Page,X
		DEC_zero_page_X();
		break;
	case 0xCE: // DEC Absolute
		DEC_absolute();
		break;
	case 0xDE: // DEC Absolute,X
		DEC_absolute_X();
		break;

	// Branch Instructions
	case 0x10: // BPL - Branch if Plus/Positive
		BPL_relative();
		break;
	case 0x30: // BMI - Branch if Minus/Negative
		BMI_relative();
		break;
	case 0x50: // BVC - Branch if Overflow Clear
		BVC_relative();
		break;
	case 0x70: // BVS - Branch if Overflow Set
		BVS_relative();
		break;
	case 0x90: // BCC - Branch if Carry Clear
		BCC_relative();
		break;
	case 0xB0: // BCS - Branch if Carry Set
		BCS_relative();
		break;
	case 0xD0: // BNE - Branch if Not Equal/Zero Clear
		BNE_relative();
		break;
	case 0xF0: // BEQ - Branch if Equal/Zero Set
		BEQ_relative();
		break;

	// Jump and Subroutine Instructions
	case 0x4C: // JMP - Jump to absolute address
		JMP_absolute();
		break;
	case 0x6C: // JMP - Jump to indirect address
		JMP_indirect();
		break;
	case 0x20: // JSR - Jump to Subroutine
		JSR();
		break;
	case 0x60: // RTS - Return from Subroutine
		RTS();
		break;
	case 0x40: // RTI - Return from Interrupt
		RTI();
		break;

	// Stack Operations
	case 0x48: // PHA - Push Accumulator
		PHA();
		break;
	case 0x68: // PLA - Pull Accumulator
		PLA();
		break;
	case 0x08: // PHP - Push Processor Status
		PHP();
		break;
	case 0x28: // PLP - Pull Processor Status
		PLP();
		break;

	// Status Flag Instructions
	case 0x18: // CLC - Clear Carry Flag
		CLC();
		break;
	case 0x38: // SEC - Set Carry Flag
		SEC();
		break;
	case 0x58: // CLI - Clear Interrupt Flag
		CLI();
		break;
	case 0x78: // SEI - Set Interrupt Flag
		SEI();
		break;
	case 0xB8: // CLV - Clear Overflow Flag
		CLV();
		break;
	case 0xD8: // CLD - Clear Decimal Flag
		CLD();
		break;
	case 0xF8: // SED - Set Decimal Flag
		SED();
		break;

	// Transfer instructions (remaining)
	case 0x9A: // TXS - Transfer X to Stack Pointer
		TXS();
		break;
	case 0xBA: // TSX - Transfer Stack Pointer to X
		TSX();
		break;

	// Bit test instructions
	case 0x24: // BIT - Zero Page
		BIT_zero_page();
		break;
	case 0x2C: // BIT - Absolute
		BIT_absolute();
		break;

	// System instructions
	case 0x00: // BRK - Break
		BRK();
		break;

	// No Operation
	case 0xEA:
		NOP();
		break;

	// ===== UNDOCUMENTED/UNOFFICIAL OPCODES =====
	// LAX - Load Accumulator and X Register
	case 0xA7: // LAX zp
		LAX_zero_page();
		break;
	case 0xB7: // LAX zp,Y
		LAX_zero_page_Y();
		break;
	case 0xAF: // LAX abs
		LAX_absolute();
		break;
	case 0xBF: // LAX abs,Y
		LAX_absolute_Y();
		break;
	case 0xA3: // LAX (zp,X)
		LAX_indexed_indirect();
		break;
	case 0xB3: // LAX (zp),Y
		LAX_indirect_indexed();
		break;

	// SAX - Store Accumulator AND X Register
	case 0x87: // SAX zp
		SAX_zero_page();
		break;
	case 0x97: // SAX zp,Y
		SAX_zero_page_Y();
		break;
	case 0x8F: // SAX abs
		SAX_absolute();
		break;
	case 0x83: // SAX (zp,X)
		SAX_indexed_indirect();
		break;

	// DCP - Decrement and Compare
	case 0xC7: // DCP zp
		DCP_zero_page();
		break;
	case 0xD7: // DCP zp,X
		DCP_zero_page_X();
		break;
	case 0xCF: // DCP abs
		DCP_absolute();
		break;
	case 0xDF: // DCP abs,X
		DCP_absolute_X();
		break;
	case 0xDB: // DCP abs,Y
		DCP_absolute_Y();
		break;
	case 0xC3: // DCP (zp,X)
		DCP_indexed_indirect();
		break;
	case 0xD3: // DCP (zp),Y
		DCP_indirect_indexed();
		break;

	// ISC/ISB - Increment and Subtract with Carry
	case 0xE7: // ISC zp
		ISC_zero_page();
		break;
	case 0xF7: // ISC zp,X
		ISC_zero_page_X();
		break;
	case 0xEF: // ISC abs
		ISC_absolute();
		break;
	case 0xFF: // ISC abs,X
		ISC_absolute_X();
		break;
	case 0xFB: // ISC abs,Y
		ISC_absolute_Y();
		break;
	case 0xE3: // ISC (zp,X)
		ISC_indexed_indirect();
		break;
	case 0xF3: // ISC (zp),Y
		ISC_indirect_indexed();
		break;

	// SLO - Shift Left and OR
	case 0x07: // SLO zp
		SLO_zero_page();
		break;
	case 0x17: // SLO zp,X
		SLO_zero_page_X();
		break;
	case 0x0F: // SLO abs
		SLO_absolute();
		break;
	case 0x1F: // SLO abs,X
		SLO_absolute_X();
		break;
	case 0x1B: // SLO abs,Y
		SLO_absolute_Y();
		break;
	case 0x03: // SLO (zp,X)
		SLO_indexed_indirect();
		break;
	case 0x13: // SLO (zp),Y
		SLO_indirect_indexed();
		break;

	// RLA - Rotate Left and AND
	case 0x27: // RLA zp
		RLA_zero_page();
		break;
	case 0x37: // RLA zp,X
		RLA_zero_page_X();
		break;
	case 0x2F: // RLA abs
		RLA_absolute();
		break;
	case 0x3F: // RLA abs,X
		RLA_absolute_X();
		break;
	case 0x3B: // RLA abs,Y
		RLA_absolute_Y();
		break;
	case 0x23: // RLA (zp,X)
		RLA_indexed_indirect();
		break;
	case 0x33: // RLA (zp),Y
		RLA_indirect_indexed();
		break;

	// SRE - Shift Right and EOR
	case 0x47: // SRE zp
		SRE_zero_page();
		break;
	case 0x57: // SRE zp,X
		SRE_zero_page_X();
		break;
	case 0x4F: // SRE abs
		SRE_absolute();
		break;
	case 0x5F: // SRE abs,X
		SRE_absolute_X();
		break;
	case 0x5B: // SRE abs,Y
		SRE_absolute_Y();
		break;
	case 0x43: // SRE (zp,X)
		SRE_indexed_indirect();
		break;
	case 0x53: // SRE (zp),Y
		SRE_indirect_indexed();
		break;

	// RRA - Rotate Right and Add
	case 0x67: // RRA zp
		RRA_zero_page();
		break;
	case 0x77: // RRA zp,X
		RRA_zero_page_X();
		break;
	case 0x6F: // RRA abs
		RRA_absolute();
		break;
	case 0x7F: // RRA abs,X
		RRA_absolute_X();
		break;
	case 0x7B: // RRA abs,Y
		RRA_absolute_Y();
		break;
	case 0x63: // RRA (zp,X)
		RRA_indexed_indirect();
		break;
	case 0x73: // RRA (zp),Y
		RRA_indirect_indexed();
		break;

	// Undocumented NOPs
	case 0x80: // NOP #value (unofficial)
	case 0x82: // NOP #value (unofficial)
	case 0x89: // NOP #value (unofficial)
	case 0xC2: // NOP #value (unofficial)
	case 0xE2: // NOP #value (unofficial)
		NOP_immediate();
		break;

	case 0x04: // NOP zp (unofficial)
	case 0x44: // NOP zp (unofficial)
	case 0x64: // NOP zp (unofficial)
		NOP_zero_page();
		break;

	case 0x14: // NOP zp,X (unofficial)
	case 0x34: // NOP zp,X (unofficial)
	case 0x54: // NOP zp,X (unofficial)
	case 0x74: // NOP zp,X (unofficial)
	case 0xD4: // NOP zp,X (unofficial)
	case 0xF4: // NOP zp,X (unofficial)
		NOP_zero_page_X();
		break;

	case 0x0C: // NOP abs (unofficial)
		NOP_absolute();
		break;

	case 0x1C: // NOP abs,X (unofficial)
	case 0x3C: // NOP abs,X (unofficial)
	case 0x5C: // NOP abs,X (unofficial)
	case 0x7C: // NOP abs,X (unofficial)
	case 0xDC: // NOP abs,X (unofficial)
	case 0xFC: // NOP abs,X (unofficial)
		NOP_absolute_X();
		break;

	// Single-byte NOPs (same as 0xEA but different opcodes)
	case 0x1A: // NOP (unofficial)
	case 0x3A: // NOP (unofficial)
	case 0x5A: // NOP (unofficial)
	case 0x7A: // NOP (unofficial)
	case 0xDA: // NOP (unofficial)
	case 0xFA: // NOP (unofficial)
		NOP();
		break;

	// Highly unstable opcodes - these crash the CPU
	case 0x8B: // ANE/XAA - extremely unstable
	case 0xAB: // LAX #value - sometimes works, sometimes doesn't
	case 0x9F: // SHA/AHX abs,Y - unstable high byte
	case 0x93: // SHA/AHX (zp),Y - unstable high byte
	case 0x9E: // SHX abs,Y - unstable high byte
	case 0x9C: // SHY abs,X - unstable high byte
	case 0x9B: // TAS - transfers A&X to SP, unstable high byte in abs,Y
	case 0xBB: // LAS - load A&X&SP from memory AND stack pointer
		CRASH();
		break;

	default:
		std::cerr << std::format("Unknown opcode: 0x{:02X} at PC: 0x{:04X}\n", static_cast<int>(opcode),
								 static_cast<int>(program_counter_ - 1));
		// For now, treat unknown opcodes as NOP
		cycles_remaining_ -= CpuCycle{2};
		break;
	}

	// Return the number of cycles consumed by this instruction
	return cycles_consumed_;
}

// Memory access methods
Byte CPU6502::read_byte(Address address) {
	consume_cycle(); // Memory reads take 1 cycle
	return bus_->read(address);
}

void CPU6502::write_byte(Address address, Byte value) {
	consume_cycle(); // Memory writes take 1 cycle
	bus_->write(address, value);
}

Address CPU6502::read_word(Address address) {
	// 6502 is little-endian
	Byte low = read_byte(address);
	Byte high = read_byte(address + 1);
	return static_cast<Address>(low) | (static_cast<Address>(high) << 8);
}

// Stack operations
void CPU6502::push_byte(Byte value) {
	write_byte(0x0100 + stack_pointer_, value);
	stack_pointer_--;
}

Byte CPU6502::pull_byte() {
	stack_pointer_++;
	return read_byte(0x0100 + stack_pointer_);
}

void CPU6502::push_word(Address value) {
	// Push high byte first (little-endian)
	push_byte(static_cast<Byte>(value >> 8));
	push_byte(static_cast<Byte>(value & 0xFF));
}

Address CPU6502::pull_word() {
	// Pull low byte first (little-endian)
	Byte low = pull_byte();
	Byte high = pull_byte();
	return static_cast<Address>(low) | (static_cast<Address>(high) << 8);
}

// Flag update helpers
void CPU6502::update_zero_flag(Byte value) noexcept {
	status_.flags.zero_flag_ = (value == 0);
}

void CPU6502::update_negative_flag(Byte value) noexcept {
	status_.flags.negative_flag_ = (value & 0x80) != 0;
}

void CPU6502::update_zero_and_negative_flags(Byte value) noexcept {
	update_zero_flag(value);
	update_negative_flag(value);
}

// Arithmetic operation helpers
void CPU6502::perform_adc(Byte value) noexcept {
	// ADC adds the value to the accumulator along with the carry flag
	Word carry_value = status_.flags.carry_flag_ ? Word{1} : Word{0};
	// Perform addition with explicit casting to avoid integer promotion warnings
	Word accumulator_word = static_cast<Word>(accumulator_);
	Word value_word = static_cast<Word>(value);
	Word result = static_cast<Word>(accumulator_word + value_word + carry_value);

	// Set carry flag if result exceeds 8 bits
	status_.flags.carry_flag_ = (result > 0xFF);

	// Set overflow flag if signed addition overflowed
	// Overflow occurs when both operands have the same sign, but the result has a different sign
	bool accumulator_sign = (accumulator_ & 0x80) != 0;
	bool value_sign = (value & 0x80) != 0;
	bool result_sign = (result & 0x80) != 0;
	status_.flags.overflow_flag_ = (accumulator_sign == value_sign) && (accumulator_sign != result_sign);

	// Update accumulator and flags
	accumulator_ = static_cast<Byte>(result & 0xFF);
	update_zero_and_negative_flags(accumulator_);
}

void CPU6502::perform_sbc(Byte value) noexcept {
	// SBC subtracts the value from the accumulator with borrow (inverted carry)
	// This is equivalent to: A = A + (~value) + carry
	// In other words, SBC is implemented as ADC with the one's complement of the value
	perform_adc(static_cast<Byte>(~value));
}

void CPU6502::perform_compare(Byte register_value, Byte memory_value) noexcept {
	// Compare performs subtraction without storing the result
	// It sets flags as if we did: register_value - memory_value
	// This is equivalent to register_value + (~memory_value) + 1 (two's complement)
	Word reg_val = static_cast<Word>(register_value);
	Word mem_val_complement = static_cast<Word>(~memory_value);
	Word result = static_cast<Word>(static_cast<Word>(reg_val + mem_val_complement) + static_cast<Word>(1));

	// Set carry flag if register_value >= memory_value (no borrow needed)
	// In subtraction, carry is set when there's NO borrow
	status_.flags.carry_flag_ = (register_value >= memory_value);

	// Set zero flag if register_value == memory_value
	status_.flags.zero_flag_ = (register_value == memory_value);

	// Set negative flag based on bit 7 of the result
	status_.flags.negative_flag_ = (result & 0x80) != 0;

	// Note: Compare instructions do NOT affect the overflow flag
}

// =============================================================================
// OAM DMA — CPU halts while DMA controller transfers 256 bytes
// =============================================================================

int CPU6502::execute_oam_dma() {
	// Acknowledge and clear the pending flag
	Byte page = bus_->get_oam_dma_page();
	bus_->clear_oam_dma_pending();

	// Cycle 1: Dummy cycle for write-cycle alignment
	consume_cycle();

	// TODO: On real hardware, if the DMA starts on an odd CPU cycle there is
	// an additional alignment dummy cycle (514 total instead of 513).

	// 256 read+write pairs = 512 cycles
	for (int i = 0; i < 256; i++) {
		// Read cycle: fetch byte from CPU address space ($XX00 + i)
		Address src = (static_cast<Address>(page) << 8) | static_cast<Address>(i);
		Byte data = read_byte(src); // 1 cycle via consume_cycle()

		// Write cycle: store byte into PPU OAM at (OAMADDR + i) & 0xFF
		consume_cycle(); // 1 cycle
		bus_->write_oam_direct(static_cast<uint8_t>(i), data);
	}

	// Total: 1 dummy + 256×2 = 513 CPU cycles
	return cycles_consumed_;
}

// Cycle management helpers
// "Fat" consume_cycle: each CPU cycle advances PPU by 3 dots, APU by 1
// cycle, and checks mapper IRQs. This gives cycle-accurate interleaving
// without rewriting instruction logic — every read_byte/write_byte/
// consume_cycle call site remains unchanged.
void CPU6502::consume_cycle() {
	cycles_remaining_ -= CpuCycle{1};
	cycles_consumed_++;
	bus_->tick_single_cpu_cycle();
}

void CPU6502::consume_cycles(int count) {
	for (int i = 0; i < count; ++i) {
		consume_cycle();
	}
}

// Addressing mode helpers
bool CPU6502::crosses_page_boundary(Address base_address, Byte offset) const noexcept {
	// Page boundary crossing occurs when the high byte changes
	// For example: $20FF + $01 = $2100 (crosses from page $20 to page $21)
	return (base_address & 0xFF00) != ((base_address + offset) & 0xFF00);
}

// Instruction implementations
void CPU6502::LDA_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	accumulator_ = read_byte(program_counter_);
	program_counter_++;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles (1 for opcode fetch + 1 for operand fetch)
}

void CPU6502::LDX_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	x_register_ = read_byte(program_counter_);
	program_counter_++;
	update_zero_and_negative_flags(x_register_);
	// Total: 2 cycles
}

void CPU6502::LDY_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	y_register_ = read_byte(program_counter_);
	program_counter_++;
	update_zero_and_negative_flags(y_register_);
	// Total: 2 cycles
}

void CPU6502::LDX_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Read from zero page address (0x00nn)
	x_register_ = read_byte(static_cast<Address>(zero_page_address));
	update_zero_and_negative_flags(x_register_);
	// Total: 3 cycles
}

void CPU6502::LDY_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Read from zero page address (0x00nn)
	y_register_ = read_byte(static_cast<Address>(zero_page_address));
	update_zero_and_negative_flags(y_register_);
	// Total: 3 cycles
}

void CPU6502::LDY_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Add X register to base address (stays in zero page, wraps around)
	consume_cycle();									 // Internal operation
	Byte effective_address = base_address + x_register_; // 8-bit addition, wraps automatically

	// Cycle 4: Read from effective zero page address
	y_register_ = read_byte(static_cast<Address>(effective_address));
	update_zero_and_negative_flags(y_register_);
	// Total: 4 cycles
}

void CPU6502::LDX_zero_page_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Add Y register to base address (stays in zero page, wraps around)
	consume_cycle();									 // Internal operation
	Byte effective_address = base_address + y_register_; // 8-bit addition, wraps automatically

	// Cycle 4: Read from effective zero page address
	x_register_ = read_byte(static_cast<Address>(effective_address));
	update_zero_and_negative_flags(x_register_);
	// Total: 4 cycles
}

void CPU6502::LDX_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Cycle 4: Read from absolute address (little-endian)
	Address absolute_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	x_register_ = read_byte(absolute_address);
	update_zero_and_negative_flags(x_register_);
	// Total: 4 cycles
}

void CPU6502::LDY_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Cycle 4: Read from absolute address (little-endian)
	Address absolute_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	y_register_ = read_byte(absolute_address);
	update_zero_and_negative_flags(y_register_);
	// Total: 4 cycles
}

void CPU6502::LDY_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + x_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle (total becomes 5 cycles)
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	y_register_ = read_byte(effective_address);
	update_zero_and_negative_flags(y_register_);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::LDX_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + y_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle (total becomes 5 cycles)
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	x_register_ = read_byte(effective_address);
	update_zero_and_negative_flags(x_register_);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

// STX Instructions
void CPU6502::STX_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Write X register to zero page address
	write_byte(zero_page_address, x_register_);
	// Total: 3 cycles
}

void CPU6502::STX_zero_page_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add Y register to base address (internal operation)
	consume_cycle();											  // Internal operation
	Byte effective_address = (base_address + y_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Write X register to effective address
	write_byte(effective_address, x_register_);
	// Total: 4 cycles
}

void CPU6502::STX_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte address_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte address_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Write X register to absolute address
	Address absolute_address = (static_cast<Address>(address_high) << 8) | address_low;
	write_byte(absolute_address, x_register_);
	// Total: 4 cycles
}

// STY Instructions
void CPU6502::STY_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Write Y register to zero page address
	write_byte(zero_page_address, y_register_);
	// Total: 3 cycles
}

void CPU6502::STY_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address (internal operation)
	consume_cycle();											  // Internal operation
	Byte effective_address = (base_address + x_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Write Y register to effective address
	write_byte(effective_address, y_register_);
	// Total: 4 cycles
}

void CPU6502::STY_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte address_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte address_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Write Y register to absolute address
	Address absolute_address = (static_cast<Address>(address_high) << 8) | address_low;
	write_byte(absolute_address, y_register_);
	// Total: 4 cycles
}

void CPU6502::LDA_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + x_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle (total becomes 5 cycles)
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	accumulator_ = read_byte(effective_address);
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::STA_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + x_register_;

	// Cycle 4: Internal operation (address calculation)
	// Cycle 5: Store to effective address
	// STA always takes 5 cycles regardless of page boundary crossing
	consume_cycle(); // Always consume extra cycle for STA absolute,X
	write_byte(effective_address, accumulator_);
	// Total: 5 cycles (always)
}

void CPU6502::LDA_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + y_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle (total becomes 5 cycles)
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	accumulator_ = read_byte(effective_address);
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::STA_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Assemble base address (little-endian)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);

	// Calculate effective address
	Address effective_address = base_address + y_register_;

	// Cycle 4: Internal operation (address calculation)
	// Cycle 5: Store to effective address
	// STA always takes 5 cycles regardless of page boundary crossing
	consume_cycle(); // Always consume extra cycle for STA absolute,Y
	write_byte(effective_address, accumulator_);
	// Total: 5 cycles (always)
}

void CPU6502::LDA_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Read from zero page address (0x00nn)
	accumulator_ = read_byte(static_cast<Address>(zero_page_address));
	update_zero_and_negative_flags(accumulator_);
	// Total: 3 cycles
}

void CPU6502::STA_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Store accumulator to zero page address (0x00nn)
	write_byte(static_cast<Address>(zero_page_address), accumulator_);
	// Total: 3 cycles
}

void CPU6502::LDA_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Add X register to base address (stays in zero page, wraps around)
	consume_cycle();									 // Internal operation
	Byte effective_address = base_address + x_register_; // 8-bit addition, wraps automatically

	// Cycle 4: Read from effective zero page address
	accumulator_ = read_byte(static_cast<Address>(effective_address));
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::STA_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Add X register to base address (stays in zero page, wraps around)
	consume_cycle();									 // Internal operation
	Byte effective_address = base_address + x_register_; // 8-bit addition, wraps automatically

	// Cycle 4: Store accumulator to effective zero page address
	write_byte(static_cast<Address>(effective_address), accumulator_);
	// Total: 4 cycles
}

void CPU6502::LDA_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Cycle 4: Read from absolute address (little-endian)
	Address absolute_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	accumulator_ = read_byte(absolute_address);
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::STA_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Cycle 4: Store accumulator to absolute address (little-endian)
	Address absolute_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	write_byte(absolute_address, accumulator_);
	// Total: 4 cycles
}

void CPU6502::TAX() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer and update flags
	consume_cycle(); // Internal operation takes 1 cycle
	x_register_ = accumulator_;
	update_zero_and_negative_flags(x_register_);
	// Total: 2 cycles
}

void CPU6502::TAY() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer and update flags
	consume_cycle(); // Internal operation takes 1 cycle
	y_register_ = accumulator_;
	update_zero_and_negative_flags(y_register_);
	// Total: 2 cycles
}

void CPU6502::TXA() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer and update flags
	consume_cycle(); // Internal operation takes 1 cycle
	accumulator_ = x_register_;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::TYA() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer and update flags
	consume_cycle(); // Internal operation takes 1 cycle
	accumulator_ = y_register_;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

// Branch Instructions - All use relative addressing mode
void CPU6502::BPL_relative() {
	// Branch if Plus/Positive (N = 0)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (!status_.flags.negative_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BMI_relative() {
	// Branch if Minus/Negative (N = 1)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (status_.flags.negative_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BVC_relative() {
	// Branch if Overflow Clear (V = 0)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (!status_.flags.overflow_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BVS_relative() {
	// Branch if Overflow Set (V = 1)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (status_.flags.overflow_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BCC_relative() {
	// Branch if Carry Clear (C = 0)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (!status_.flags.carry_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BCS_relative() {
	// Branch if Carry Set (C = 1)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (status_.flags.carry_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BNE_relative() {
	// Branch if Not Equal/Zero Clear (Z = 0)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (!status_.flags.zero_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

void CPU6502::BEQ_relative() {
	// Branch if Equal/Zero Set (Z = 1)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch offset
	SignedByte offset = static_cast<SignedByte>(read_byte(program_counter_));
	program_counter_++;

	if (status_.flags.zero_flag_) {
		// Branch taken
		// Cycle 3: Internal operation (branch decision)
		consume_cycle();
		Address old_pc = program_counter_;
		program_counter_ = static_cast<Address>(program_counter_ + offset);

		// Check for page boundary crossing (cycle 4 if crossed)
		if ((old_pc & 0xFF00) != (program_counter_ & 0xFF00)) {
			consume_cycle(); // Extra cycle for page boundary crossing
		}
	}
	// Total: 2 cycles (no branch), 3 cycles (branch same page), 4 cycles (branch different page)
}

// Jump and Subroutine Instructions
void CPU6502::JMP_absolute() {
	// Jump to absolute address
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	// Don't increment PC here - we're jumping to the new address

	// Set program counter to the new address
	program_counter_ = (static_cast<Address>(high) << 8) | low;
	// Total: 3 cycles
}

void CPU6502::JMP_indirect() {
	// Jump to address stored at given address (with 6502 page boundary bug)
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of indirect address
	Byte indirect_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of indirect address
	Byte indirect_high = read_byte(program_counter_);
	program_counter_++;

	Address indirect_address = (static_cast<Address>(indirect_high) << 8) | indirect_low;

	// Cycle 4: Read low byte of target address
	Byte target_low = read_byte(indirect_address);

	// Cycle 5: Read high byte of target address (with page boundary bug)
	// The 6502 has a bug where if the indirect address is at a page boundary,
	// it reads the high byte from the same page instead of crossing to the next page
	Address high_byte_address;
	if ((indirect_address & 0xFF) == 0xFF) {
		// Page boundary bug: wrap around within the same page
		high_byte_address = indirect_address & 0xFF00;
	} else {
		// Normal case: read from next address
		high_byte_address = indirect_address + 1;
	}
	Byte target_high = read_byte(high_byte_address);

	// Set program counter to the target address
	program_counter_ = (static_cast<Address>(target_high) << 8) | target_low;
	// Total: 5 cycles
}

void CPU6502::JSR() {
	// Jump to Subroutine
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of subroutine address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Internal operation (stack pointer operation)
	consume_cycle();

	// Cycle 4: Push high byte of return address (PC-1) to stack
	Address return_address = program_counter_; // This points to the high byte we haven't read yet
	push_byte(static_cast<Byte>((return_address) >> 8));

	// Cycle 5: Push low byte of return address (PC-1) to stack
	push_byte(static_cast<Byte>(return_address & 0xFF));

	// Cycle 6: Fetch high byte of subroutine address
	Byte high = read_byte(program_counter_);

	// Set program counter to the subroutine address
	program_counter_ = (static_cast<Address>(high) << 8) | low;
	// Total: 6 cycles
}

void CPU6502::RTS() {
	// Return from Subroutine
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Cycle 3: Pull low byte of return address from stack
	Byte low = pull_byte();

	// Cycle 4: Pull high byte of return address from stack
	Byte high = pull_byte();

	// Cycle 5: Internal operation (increment PC)
	consume_cycle();

	// Restore program counter and increment by 1 (RTS returns to instruction after JSR)
	program_counter_ = (static_cast<Address>(high) << 8) | low;
	program_counter_++; // RTS increments PC by 1 after restoring it
						// Total: 6 cycles
}

void CPU6502::RTI() {
	// Return from Interrupt
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Cycle 3: Pull status register from stack
	status_.status_register_ = pull_byte();
	// Clear break flag and set unused flag (as per 6502 behavior)
	status_.flags.break_flag_ = false;
	status_.flags.unused_flag_ = true;

	// Cycle 4: Pull low byte of return address from stack
	Byte low = pull_byte();

	// Cycle 5: Pull high byte of return address from stack
	Byte high = pull_byte();

	// Restore program counter (RTI doesn't increment PC - returns to exact interrupt address)
	program_counter_ = (static_cast<Address>(high) << 8) | low;
	// Total: 6 cycles
}

void CPU6502::NOP() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Do nothing
	consume_cycle(); // NOP still takes 1 cycle to execute
					 // Total: 2 cycles
} // Stack Operations
void CPU6502::PHA() {
	// Push Accumulator - Store accumulator on stack
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	// Cycle 3: Push accumulator to stack
	push_byte(accumulator_);
	// Total: 3 cycles
}

void CPU6502::PLA() {
	// Pull Accumulator - Load accumulator from stack
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	// Cycle 3: Increment stack pointer
	consume_cycle();
	stack_pointer_++;
	// Cycle 4: Read accumulator from stack
	accumulator_ = read_byte(0x0100 + stack_pointer_);

	// Update flags based on pulled value
	update_zero_flag(accumulator_);
	update_negative_flag(accumulator_);
	// Total: 4 cycles
}

void CPU6502::PHP() {
	// Push Processor Status - Store status register on stack
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	// Cycle 3: Push status register to stack
	// Note: When PHP is executed, the B flag is set in the pushed value
	Byte status_to_push = status_.status_register_ | 0x10; // Set B flag
	push_byte(status_to_push);
	// Total: 3 cycles
}

void CPU6502::PLP() {
	// Pull Processor Status - Load status register from stack
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	// Cycle 3: Increment stack pointer
	consume_cycle();
	stack_pointer_++;
	// Cycle 4: Read status register from stack
	status_.status_register_ = read_byte(0x0100 + stack_pointer_);
	// Note: Bit 5 (unused flag) is always set, bit 4 (B flag) is ignored
	status_.status_register_ |= 0x20u; // Ensure unused flag is set
	status_.status_register_ &= 0xEFu; // Clear B flag (it doesn't exist in the actual register)
									   // Total: 4 cycles
}

// Status Flag Instructions
void CPU6502::CLC() {
	// Clear Carry Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Clear carry flag
	consume_cycle();
	status_.flags.carry_flag_ = false;
	// Total: 2 cycles
}

void CPU6502::SEC() {
	// Set Carry Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Set carry flag
	consume_cycle();
	status_.flags.carry_flag_ = true;
	// Total: 2 cycles
}

void CPU6502::CLI() {
	// Clear Interrupt Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Clear interrupt flag
	consume_cycle();
	status_.flags.interrupt_flag_ = false;
	// Total: 2 cycles
}

void CPU6502::SEI() {
	// Set Interrupt Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Set interrupt flag
	consume_cycle();
	status_.flags.interrupt_flag_ = true;
	// Total: 2 cycles
}

void CPU6502::CLV() {
	// Clear Overflow Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Clear overflow flag
	consume_cycle();
	status_.flags.overflow_flag_ = false;
	// Total: 2 cycles
}

void CPU6502::CLD() {
	// Clear Decimal Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Clear decimal flag
	consume_cycle();
	status_.flags.decimal_flag_ = false;
	// Total: 2 cycles
}

void CPU6502::SED() {
	// Set Decimal Flag
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Set decimal flag
	consume_cycle();
	status_.flags.decimal_flag_ = true;
	// Total: 2 cycles
}

// Transfer instructions (remaining)
void CPU6502::TXS() {
	// Transfer X to Stack Pointer
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer X to SP
	consume_cycle();
	stack_pointer_ = x_register_;
	// No flags affected
	// Total: 2 cycles
}

void CPU6502::TSX() {
	// Transfer Stack Pointer to X
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Transfer SP to X and update flags
	consume_cycle();
	x_register_ = stack_pointer_;
	update_zero_and_negative_flags(x_register_);
	// Total: 2 cycles
}

// Bit test instructions
void CPU6502::BIT_zero_page() {
	// BIT Zero Page - Test bits in memory with accumulator
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_address = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Read from zero page address
	Byte memory_value = read_byte(static_cast<Address>(zero_page_address));

	// Perform BIT operation:
	// Z flag = (A AND M) == 0
	// N flag = bit 7 of M
	// V flag = bit 6 of M
	Byte result = accumulator_ & memory_value;
	status_.flags.zero_flag_ = (result == 0);
	status_.flags.negative_flag_ = (memory_value & 0x80) != 0;
	status_.flags.overflow_flag_ = (memory_value & 0x40) != 0;
	// Total: 3 cycles
}

void CPU6502::BIT_absolute() {
	// BIT Absolute - Test bits in memory with accumulator
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;

	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	// Cycle 4: Read from absolute address
	Address absolute_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte memory_value = read_byte(absolute_address);

	// Perform BIT operation:
	// Z flag = (A AND M) == 0
	// N flag = bit 7 of M
	// V flag = bit 6 of M
	Byte result = accumulator_ & memory_value;
	status_.flags.zero_flag_ = (result == 0);
	status_.flags.negative_flag_ = (memory_value & 0x80) != 0;
	status_.flags.overflow_flag_ = (memory_value & 0x40) != 0;
	// Total: 4 cycles
}

// System instructions
void CPU6502::BRK() {
	// Break - Force interrupt
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Read next instruction byte (ignored, but PC incremented)
	program_counter_++; // BRK increments PC by 2
	consume_cycle();

	// Cycle 3: Push high byte of PC to stack
	push_byte(static_cast<Byte>(program_counter_ >> 8));

	// Cycle 4: Push low byte of PC to stack
	push_byte(static_cast<Byte>(program_counter_ & 0xFF));

	// Cycle 5: Push status register to stack (with B flag set)
	Byte status_with_b = status_.status_register_ | 0x10; // Set B flag
	push_byte(status_with_b);

	// Cycle 6: Fetch IRQ vector low byte
	Byte irq_vector_low = read_byte(0xFFFE);

	// Cycle 7: Fetch IRQ vector high byte and set I flag
	Byte irq_vector_high = read_byte(0xFFFF);
	status_.flags.interrupt_flag_ = true; // Set interrupt disable flag

	// Set PC to IRQ vector
	program_counter_ = static_cast<Address>(irq_vector_low) | (static_cast<Address>(irq_vector_high) << 8);
	// Total: 7 cycles
}

void CPU6502::LDA_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address (internal operation)
	consume_cycle();											// Internal indexing operation
	Byte indexed_address = (base_address + x_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Fetch low byte of indirect address
	Byte low = read_byte(indexed_address);
	// Cycle 5: Fetch high byte of indirect address
	Byte high = read_byte((indexed_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 6: Load accumulator from final address
	Address final_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	accumulator_ = read_byte(final_address);
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::STA_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address (internal operation)
	consume_cycle();											// Internal indexing operation
	Byte indexed_address = (base_address + x_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Fetch low byte of indirect address
	Byte low = read_byte(indexed_address);
	// Cycle 5: Fetch high byte of indirect address
	Byte high = read_byte((indexed_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 6: Store accumulator to final address
	Address final_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	write_byte(final_address, accumulator_);
	// Total: 6 cycles
}

void CPU6502::LDA_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(zp_address);
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 5: Add Y register to base address and load
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	accumulator_ = read_byte(final_address);
	update_zero_and_negative_flags(accumulator_);
	// Total: 5-6 cycles (5 normally, 6 if page boundary crossed)
}

void CPU6502::STA_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(zp_address);
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 5: Add Y register to base address (internal operation)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	consume_cycle(); // Internal operation for indexing
	Address final_address = base_address + y_register_;
	// Cycle 6: Store accumulator to final address
	write_byte(final_address, accumulator_);
	// Total: 6 cycles (store always takes extra cycle for indexing)
}

// ADC - Add with Carry implementations
void CPU6502::ADC_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value
	Byte value = read_byte(program_counter_);
	program_counter_++;

	// Perform addition with carry
	perform_adc(value);
	// Total: 2 cycles
}

void CPU6502::ADC_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(address);

	perform_adc(value);
	// Total: 3 cycles
}

void CPU6502::ADC_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register (with zero page wrap)
	consume_cycle(); // Internal indexing operation
	Byte final_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Read value from effective address
	Byte value = read_byte(final_address);

	perform_adc(value);
	// Total: 4 cycles
}

void CPU6502::ADC_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);

	perform_adc(value);
	// Total: 4 cycles
}

void CPU6502::ADC_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	Byte value = read_byte(effective_address);
	perform_adc(value);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::ADC_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	Byte value = read_byte(effective_address);
	perform_adc(value);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::ADC_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address (internal operation)
	consume_cycle();											// Internal indexing operation
	Byte indexed_address = (base_address + x_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Fetch low byte of indirect address
	Byte low = read_byte(indexed_address);
	// Cycle 5: Fetch high byte of indirect address
	Byte high = read_byte((indexed_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 6: Read value from final address
	Address final_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(final_address);

	perform_adc(value);
	// Total: 6 cycles
}

void CPU6502::ADC_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(zp_address);
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 5: Add Y register to base address and read
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	perform_adc(value);
	// Total: 5-6 cycles (5 normally, 6 if page boundary crossed)
}

// SBC - Subtract with Carry implementations
void CPU6502::SBC_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value
	Byte value = read_byte(program_counter_);
	program_counter_++;

	// Perform subtraction with carry
	perform_sbc(value);
	// Total: 2 cycles
}

void CPU6502::SBC_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(address);

	perform_sbc(value);
	// Total: 3 cycles
}

void CPU6502::SBC_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register (with zero page wrap)
	consume_cycle(); // Internal indexing operation
	Byte final_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Read value from effective address
	Byte value = read_byte(final_address);

	perform_sbc(value);
	// Total: 4 cycles
}

void CPU6502::SBC_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);

	perform_sbc(value);
	// Total: 4 cycles
}

void CPU6502::SBC_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	Byte value = read_byte(effective_address);
	perform_sbc(value);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::SBC_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;

	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;

	// Cycle 4: Read from effective address
	// Page boundary crossing adds 1 cycle
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Additional cycle for page boundary crossing
	}

	Byte value = read_byte(effective_address);
	perform_sbc(value);
	// Total: 4 cycles (normal) or 5 cycles (page boundary crossed)
}

void CPU6502::SBC_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address (internal operation)
	consume_cycle();											// Internal indexing operation
	Byte indexed_address = (base_address + x_register_) & 0xFF; // Wrap within zero page
	// Cycle 4: Fetch low byte of indirect address
	Byte low = read_byte(indexed_address);
	// Cycle 5: Fetch high byte of indirect address
	Byte high = read_byte((indexed_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 6: Read value from final address
	Address final_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(final_address);

	perform_sbc(value);
	// Total: 6 cycles
}

void CPU6502::SBC_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(zp_address);
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 5: Add Y register to base address and read
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	perform_sbc(value);
	// Total: 5-6 cycles (5 normally, 6 if page boundary crossed)
}

// Compare Instructions - CMP (Compare with Accumulator)
void CPU6502::CMP_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	Byte value = read_byte(program_counter_);
	program_counter_++;
	perform_compare(accumulator_, value);
	// Total: 2 cycles
}

void CPU6502::CMP_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	perform_compare(accumulator_, value);
	// Total: 3 cycles
}

void CPU6502::CMP_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X to address (wraps within zero page)
	address = static_cast<Byte>(address + x_register_);
	// Cycle 4: Read value from calculated address
	Byte value = read_byte(address);
	perform_compare(accumulator_, value);
	// Total: 4 cycles
}

void CPU6502::CMP_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	perform_compare(accumulator_, value);
	// Total: 4 cycles
}

void CPU6502::CMP_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to low byte and check for page crossing
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + x_register_;
	if ((base_address & 0xFF00) != (final_address & 0xFF00)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}
	// Cycle 4-5: Read value from final address
	Byte value = read_byte(final_address);
	perform_compare(accumulator_, value);
	// Total: 4-5 cycles (4 normally, 5 if page boundary crossed)
}

void CPU6502::CMP_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to low byte and check for page crossing
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + y_register_;
	if ((base_address & 0xFF00) != (final_address & 0xFF00)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}
	// Cycle 4-5: Read value from final address
	Byte value = read_byte(final_address);
	perform_compare(accumulator_, value);
	// Total: 4-5 cycles (4 normally, 5 if page boundary crossed)
}

void CPU6502::CMP_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X to zero page address (wraps within zero page)
	zp_address = static_cast<Byte>(zp_address + x_register_);
	// Cycle 4: Fetch low byte of indirect address
	Byte low = read_byte(zp_address);
	// Cycle 5: Fetch high byte of indirect address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 6: Read value from final address
	Word final_address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(final_address);
	perform_compare(accumulator_, value);
	// Total: 6 cycles
}

void CPU6502::CMP_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zp_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(zp_address);
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte((zp_address + 1) & 0xFF); // Wrap within zero page
	// Cycle 5: Add Y to base address and check for page crossing
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + y_register_;
	if ((base_address & 0xFF00) != (final_address & 0xFF00)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}
	// Cycle 5-6: Read value from final address
	Byte value = read_byte(final_address);
	perform_compare(accumulator_, value);
	// Total: 5-6 cycles (5 normally, 6 if page boundary crossed)
}

// Compare Instructions - CPX (Compare with X Register)
void CPU6502::CPX_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	Byte value = read_byte(program_counter_);
	program_counter_++;
	perform_compare(x_register_, value);
	// Total: 2 cycles
}

void CPU6502::CPX_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	perform_compare(x_register_, value);
	// Total: 3 cycles
}

void CPU6502::CPX_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	perform_compare(x_register_, value);
	// Total: 4 cycles
}

// Compare Instructions - CPY (Compare with Y Register)
void CPU6502::CPY_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch operand
	Byte value = read_byte(program_counter_);
	program_counter_++;
	perform_compare(y_register_, value);
	// Total: 2 cycles
}

void CPU6502::CPY_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	perform_compare(y_register_, value);
	// Total: 3 cycles
}

void CPU6502::CPY_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	perform_compare(y_register_, value);
	// Total: 4 cycles
}

// Logical Instructions - AND (Bitwise AND with Accumulator)
void CPU6502::AND_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value
	Byte value = read_byte(program_counter_);
	program_counter_++;
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::AND_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 3 cycles
}

void CPU6502::AND_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to address (wraps in zero page)
	Byte address = static_cast<Byte>(base_address + x_register_);
	consume_cycle();
	// Cycle 4: Read value from zero page,X address
	Byte value = read_byte(address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::AND_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::AND_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + x_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::AND_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::AND_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	Byte indexed_addr = static_cast<Byte>(zero_page_addr + x_register_);
	consume_cycle();
	// Cycle 4: Read low byte of target address from indexed zero page
	Byte target_low = read_byte(indexed_addr);
	// Cycle 5: Read high byte of target address from indexed zero page + 1
	Byte target_high = read_byte(static_cast<Byte>(indexed_addr + 1));
	// Cycle 6: Read value from target address
	Word target_address = (static_cast<Word>(target_high) << 8) | target_low;
	Byte value = read_byte(target_address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::AND_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read low byte of base address from zero page
	Byte base_low = read_byte(zero_page_addr);
	// Cycle 4: Read high byte of base address from zero page + 1
	Byte base_high = read_byte(static_cast<Byte>(zero_page_addr + 1));
	// Cycle 5: Add Y register to base address and read value
	Word base_address = (static_cast<Word>(base_high) << 8) | base_low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles (6 if page boundary crossed)
}

// Logical Instructions - ORA (Bitwise OR with Accumulator)
void CPU6502::ORA_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value
	Byte value = read_byte(program_counter_);
	program_counter_++;
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::ORA_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 3 cycles
}

void CPU6502::ORA_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to address (wraps in zero page)
	Byte address = static_cast<Byte>(base_address + x_register_);
	consume_cycle();
	// Cycle 4: Read value from zero page,X address
	Byte value = read_byte(address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::ORA_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::ORA_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + x_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::ORA_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::ORA_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	Byte indexed_addr = static_cast<Byte>(zero_page_addr + x_register_);
	consume_cycle();
	// Cycle 4: Read low byte of target address from indexed zero page
	Byte target_low = read_byte(indexed_addr);
	// Cycle 5: Read high byte of target address from indexed zero page + 1
	Byte target_high = read_byte(static_cast<Byte>(indexed_addr + 1));
	// Cycle 6: Read value from target address
	Word target_address = (static_cast<Word>(target_high) << 8) | target_low;
	Byte value = read_byte(target_address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::ORA_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read low byte of base address from zero page
	Byte base_low = read_byte(zero_page_addr);
	// Cycle 4: Read high byte of base address from zero page + 1
	Byte base_high = read_byte(static_cast<Byte>(zero_page_addr + 1));
	// Cycle 5: Add Y register to base address and read value
	Word base_address = (static_cast<Word>(base_high) << 8) | base_low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles (6 if page boundary crossed)
}

// Logical Instructions - EOR (Bitwise Exclusive OR with Accumulator)
void CPU6502::EOR_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value
	Byte value = read_byte(program_counter_);
	program_counter_++;
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::EOR_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 3 cycles
}

void CPU6502::EOR_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to address (wraps in zero page)
	Byte address = static_cast<Byte>(base_address + x_register_);
	consume_cycle();
	// Cycle 4: Read value from zero page,X address
	Byte value = read_byte(address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::EOR_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(high) << 8) | low;
	Byte value = read_byte(address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles
}

void CPU6502::EOR_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + x_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, x_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::EOR_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y register to address and read value
	Word base_address = (static_cast<Word>(high) << 8) | low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::EOR_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	Byte indexed_addr = static_cast<Byte>(zero_page_addr + x_register_);
	consume_cycle();
	// Cycle 4: Read low byte of target address from indexed zero page
	Byte target_low = read_byte(indexed_addr);
	// Cycle 5: Read high byte of target address from indexed zero page + 1
	Byte target_high = read_byte(static_cast<Byte>(indexed_addr + 1));
	// Cycle 6: Read value from target address
	Word target_address = (static_cast<Word>(target_high) << 8) | target_low;
	Byte value = read_byte(target_address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::EOR_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read low byte of base address from zero page
	Byte base_low = read_byte(zero_page_addr);
	// Cycle 4: Read high byte of base address from zero page + 1
	Byte base_high = read_byte(static_cast<Byte>(zero_page_addr + 1));
	// Cycle 5: Add Y register to base address and read value
	Word base_address = (static_cast<Word>(base_high) << 8) | base_low;
	Word final_address = base_address + y_register_;

	// Check for page boundary crossing
	if (crosses_page_boundary(base_address, y_register_)) {
		consume_cycle(); // Extra cycle for page boundary crossing
	}

	Byte value = read_byte(final_address);
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles (6 if page boundary crossed)
}

// Shift/Rotate Instructions - ASL (Arithmetic Shift Left)
void CPU6502::ASL_accumulator() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Set carry flag to bit 7 before shifting
	status_.flags.carry_flag_ = (accumulator_ & 0x80) != 0;

	// Shift left (multiply by 2)
	accumulator_ = static_cast<Byte>(accumulator_ << 1);

	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::ASL_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(zero_page_addr);
	// Cycle 4: Write original value back (during operation)
	write_byte(zero_page_addr, value);
	// Cycle 5: Write modified value

	// Set carry flag to bit 7 before shifting
	status_.flags.carry_flag_ = (value & 0x80) != 0;

	// Shift left (multiply by 2)
	value = static_cast<Byte>(value << 1);

	write_byte(zero_page_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::ASL_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	consume_cycle();
	Byte final_addr = static_cast<Byte>(zero_page_addr + x_register_);
	// Cycle 4: Read value from final address
	Byte value = read_byte(final_addr);
	// Cycle 5: Write original value back (during operation)
	write_byte(final_addr, value);
	// Cycle 6: Write modified value

	// Set carry flag to bit 7 before shifting
	status_.flags.carry_flag_ = (value & 0x80) != 0;

	// Shift left (multiply by 2)
	value = static_cast<Byte>(value << 1);

	write_byte(final_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ASL_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Byte value = read_byte(address);
	// Cycle 5: Write original value back (during operation)
	write_byte(address, value);
	// Cycle 6: Write modified value

	// Set carry flag to bit 7 before shifting
	status_.flags.carry_flag_ = (value & 0x80) != 0;

	// Shift left (multiply by 2)
	value = static_cast<Byte>(value << 1);

	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ASL_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address
	Word base_address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Word final_address = base_address + x_register_;
	consume_cycle(); // Always takes extra cycle for indexing
	// Cycle 5: Read value from final address
	Byte value = read_byte(final_address);
	// Cycle 6: Write original value back (during operation)
	write_byte(final_address, value);
	// Cycle 7: Write modified value

	// Set carry flag to bit 7 before shifting
	status_.flags.carry_flag_ = (value & 0x80) != 0;

	// Shift left (multiply by 2)
	value = static_cast<Byte>(value << 1);

	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles
}

// Shift/Rotate Instructions - LSR (Logical Shift Right)
void CPU6502::LSR_accumulator() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Set carry flag to bit 0 before shifting
	status_.flags.carry_flag_ = (accumulator_ & 0x01) != 0;

	// Shift right (divide by 2)
	accumulator_ = static_cast<Byte>(accumulator_ >> 1);

	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::LSR_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(zero_page_addr);
	// Cycle 4: Write original value back (during operation)
	write_byte(zero_page_addr, value);
	// Cycle 5: Write modified value

	// Set carry flag to bit 0 before shifting
	status_.flags.carry_flag_ = (value & 0x01) != 0;

	// Shift right (divide by 2)
	value = static_cast<Byte>(value >> 1);

	write_byte(zero_page_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::LSR_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	consume_cycle();
	Byte final_addr = static_cast<Byte>(zero_page_addr + x_register_);
	// Cycle 4: Read value from final address
	Byte value = read_byte(final_addr);
	// Cycle 5: Write original value back (during operation)
	write_byte(final_addr, value);
	// Cycle 6: Write modified value

	// Set carry flag to bit 0 before shifting
	status_.flags.carry_flag_ = (value & 0x01) != 0;

	// Shift right (divide by 2)
	value = static_cast<Byte>(value >> 1);

	write_byte(final_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::LSR_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Byte value = read_byte(address);
	// Cycle 5: Write original value back (during operation)
	write_byte(address, value);
	// Cycle 6: Write modified value

	// Set carry flag to bit 0 before shifting
	status_.flags.carry_flag_ = (value & 0x01) != 0;

	// Shift right (divide by 2)
	value = static_cast<Byte>(value >> 1);

	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::LSR_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address
	Word base_address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Word final_address = base_address + x_register_;
	consume_cycle(); // Always takes extra cycle for indexing
	// Cycle 5: Read value from final address
	Byte value = read_byte(final_address);
	// Cycle 6: Write original value back (during operation)
	write_byte(final_address, value);
	// Cycle 7: Write modified value

	// Set carry flag to bit 0 before shifting
	status_.flags.carry_flag_ = (value & 0x01) != 0;

	// Shift right (divide by 2)
	value = static_cast<Byte>(value >> 1);

	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles
}

// Shift/Rotate Instructions - ROL (Rotate Left)
void CPU6502::ROL_accumulator() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Save bit 7 for carry flag
	bool new_carry = (accumulator_ & 0x80) != 0;

	// Rotate left: shift left and add old carry to bit 0
	accumulator_ = static_cast<Byte>((accumulator_ << 1) | (status_.flags.carry_flag_ ? 1 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::ROL_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(zero_page_addr);
	// Cycle 4: Write original value back (during operation)
	write_byte(zero_page_addr, value);
	// Cycle 5: Write modified value

	// Save bit 7 for carry flag
	bool new_carry = (value & 0x80) != 0;

	// Rotate left: shift left and add old carry to bit 0
	value = static_cast<Byte>((value << 1) | (status_.flags.carry_flag_ ? 1 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(zero_page_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::ROL_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	consume_cycle();
	Byte final_addr = static_cast<Byte>(zero_page_addr + x_register_);
	// Cycle 4: Read value from final address
	Byte value = read_byte(final_addr);
	// Cycle 5: Write original value back (during operation)
	write_byte(final_addr, value);
	// Cycle 6: Write modified value

	// Save bit 7 for carry flag
	bool new_carry = (value & 0x80) != 0;

	// Rotate left: shift left and add old carry to bit 0
	value = static_cast<Byte>((value << 1) | (status_.flags.carry_flag_ ? 1 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(final_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ROL_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Byte value = read_byte(address);
	// Cycle 5: Write original value back (during operation)
	write_byte(address, value);
	// Cycle 6: Write modified value

	// Save bit 7 for carry flag
	bool new_carry = (value & 0x80) != 0;

	// Rotate left: shift left and add old carry to bit 0
	value = static_cast<Byte>((value << 1) | (status_.flags.carry_flag_ ? 1 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ROL_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address
	Word base_address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Word final_address = base_address + x_register_;
	consume_cycle(); // Always takes extra cycle for indexing
	// Cycle 5: Read value from final address
	Byte value = read_byte(final_address);
	// Cycle 6: Write original value back (during operation)
	write_byte(final_address, value);
	// Cycle 7: Write modified value

	// Save bit 7 for carry flag
	bool new_carry = (value & 0x80) != 0;

	// Rotate left: shift left and add old carry to bit 0
	value = static_cast<Byte>((value << 1) | (status_.flags.carry_flag_ ? 1 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles
}

// Shift/Rotate Instructions - ROR (Rotate Right)
void CPU6502::ROR_accumulator() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();

	// Save bit 0 for carry flag
	bool new_carry = (accumulator_ & 0x01) != 0;

	// Rotate right: shift right and add old carry to bit 7
	accumulator_ = static_cast<Byte>((accumulator_ >> 1) | (status_.flags.carry_flag_ ? 0x80 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	update_zero_and_negative_flags(accumulator_);
	// Total: 2 cycles
}

void CPU6502::ROR_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page address
	Byte value = read_byte(zero_page_addr);
	// Cycle 4: Write original value back (during operation)
	write_byte(zero_page_addr, value);
	// Cycle 5: Write modified value

	// Save bit 0 for carry flag
	bool new_carry = (value & 0x01) != 0;

	// Rotate right: shift right and add old carry to bit 7
	value = static_cast<Byte>((value >> 1) | (status_.flags.carry_flag_ ? 0x80 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(zero_page_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::ROR_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte zero_page_addr = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to zero page address
	consume_cycle();
	Byte final_addr = static_cast<Byte>(zero_page_addr + x_register_);
	// Cycle 4: Read value from final address
	Byte value = read_byte(final_addr);
	// Cycle 5: Write original value back (during operation)
	write_byte(final_addr, value);
	// Cycle 6: Write modified value

	// Save bit 0 for carry flag
	bool new_carry = (value & 0x01) != 0;

	// Rotate right: shift right and add old carry to bit 7
	value = static_cast<Byte>((value >> 1) | (status_.flags.carry_flag_ ? 0x80 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(final_addr, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ROR_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Word address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Byte value = read_byte(address);
	// Cycle 5: Write original value back (during operation)
	write_byte(address, value);
	// Cycle 6: Write modified value

	// Save bit 0 for carry flag
	bool new_carry = (value & 0x01) != 0;

	// Rotate right: shift right and add old carry to bit 7
	value = static_cast<Byte>((value >> 1) | (status_.flags.carry_flag_ ? 0x80 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::ROR_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte addr_low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte addr_high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X register to address
	Word base_address = (static_cast<Word>(addr_high) << 8) | addr_low;
	Word final_address = base_address + x_register_;
	consume_cycle(); // Always takes extra cycle for indexing
	// Cycle 5: Read value from final address
	Byte value = read_byte(final_address);
	// Cycle 6: Write original value back (during operation)
	write_byte(final_address, value);
	// Cycle 7: Write modified value

	// Save bit 0 for carry flag
	bool new_carry = (value & 0x01) != 0;

	// Rotate right: shift right and add old carry to bit 7
	value = static_cast<Byte>((value >> 1) | (status_.flags.carry_flag_ ? 0x80 : 0));

	// Set new carry flag
	status_.flags.carry_flag_ = new_carry;

	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles
}

// Increment/Decrement Instructions - Register Operations
void CPU6502::INX() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	x_register_++;
	update_zero_and_negative_flags(x_register_);
	// Total: 2 cycles
}

void CPU6502::INY() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	y_register_++;
	update_zero_and_negative_flags(y_register_);
	// Total: 2 cycles
}

void CPU6502::DEX() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	x_register_--;
	update_zero_and_negative_flags(x_register_);
	// Total: 2 cycles
}

void CPU6502::DEY() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Internal operation
	consume_cycle();
	y_register_--;
	update_zero_and_negative_flags(y_register_);
	// Total: 2 cycles
}

// Increment/Decrement Instructions - Memory Operations
void CPU6502::INC_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(address);
	// Cycle 4: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 5: Write incremented value back
	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::INC_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register (with zero page wrap)
	consume_cycle(); // Internal indexing operation
	Byte final_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Read value from effective address
	Byte value = read_byte(final_address);
	// Cycle 5: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 6: Write incremented value back
	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::INC_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 6: Write incremented value back
	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::INC_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 7: Write incremented value back
	write_byte(effective_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles (RMW instructions always take extra cycle)
}

void CPU6502::DEC_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(address);
	// Cycle 4: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 5: Write decremented value back
	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 5 cycles
}

void CPU6502::DEC_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register (with zero page wrap)
	consume_cycle(); // Internal indexing operation
	Byte final_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Read value from effective address
	Byte value = read_byte(final_address);
	// Cycle 5: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 6: Write decremented value back
	write_byte(final_address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::DEC_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 6: Write decremented value back
	write_byte(address, value);
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::DEC_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 7: Write decremented value back
	write_byte(effective_address, value);
	update_zero_and_negative_flags(value);
	// Total: 7 cycles (RMW instructions always take extra cycle)
}

// ===== UNDOCUMENTED/UNOFFICIAL OPCODES =====
// These are stable undocumented opcodes that are safe to implement

// LAX - Load Accumulator and X Register (LDA + TAX combined)
void CPU6502::LAX_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 3 cycles
}

void CPU6502::LAX_zero_page_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add Y register to base address
	Address effective_address = static_cast<Address>((base_address + y_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 4 cycles
}

void CPU6502::LAX_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 4 cycles
}

void CPU6502::LAX_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Calculate effective address
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;

	// Cycle 4: Read from effective address (always, even if page crossed)
	if (crosses_page_boundary(base_address, y_register_)) {
		// Cycle 4: Read from wrong page first (dummy read)
		consume_cycle();
	}
	Byte value = read_byte(effective_address);
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 4 cycles (5 if page boundary crossed)
}

void CPU6502::LAX_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 6 cycles
}

void CPU6502::LAX_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Calculate effective address
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;

	// Cycle 5: Read from effective address
	if (crosses_page_boundary(base_address, y_register_)) {
		// Cycle 5: Read from wrong page first (dummy read)
		consume_cycle();
	}
	Byte value = read_byte(effective_address);
	// Load into both A and X registers
	accumulator_ = value;
	x_register_ = value;
	update_zero_and_negative_flags(value);
	// Total: 5 cycles (6 if page boundary crossed)
}

// SAX - Store Accumulator AND X Register
void CPU6502::SAX_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Store A AND X to zero page
	Byte value = accumulator_ & x_register_;
	write_byte(static_cast<Address>(address), value);
	// Total: 3 cycles
}

void CPU6502::SAX_zero_page_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add Y register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + y_register_) & 0xFF);
	// Cycle 4: Store A AND X to effective address
	Byte value = accumulator_ & x_register_;
	write_byte(effective_address, value);
	// Total: 4 cycles
}

void CPU6502::SAX_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Store A AND X to absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = accumulator_ & x_register_;
	write_byte(address, value);
	// Total: 4 cycles
}

void CPU6502::SAX_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Store A AND X to target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = accumulator_ & x_register_;
	write_byte(target_address, value);
	// Total: 6 cycles
}

// DCP - Decrement and Compare (DEC + CMP combined)
void CPU6502::DCP_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 5: Write decremented value back
	write_byte(static_cast<Address>(address), value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 5 cycles
}

void CPU6502::DCP_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 6: Write decremented value back
	write_byte(effective_address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 6 cycles
}

void CPU6502::DCP_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 6: Write decremented value back
	write_byte(address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 6 cycles
}

void CPU6502::DCP_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 7: Write decremented value back
	write_byte(effective_address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 7 cycles
}

void CPU6502::DCP_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 7: Write decremented value back
	write_byte(effective_address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 7 cycles
}

void CPU6502::DCP_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 8: Write decremented value back
	write_byte(target_address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 8 cycles
}

void CPU6502::DCP_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (decrement)
	consume_cycle();
	value--;
	// Cycle 8: Write decremented value back
	write_byte(effective_address, value);
	// Then perform compare with accumulator
	perform_compare(accumulator_, value);
	// Total: 8 cycles
}

// ISC/ISB - Increment and Subtract with Carry (INC + SBC combined)
void CPU6502::ISC_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 5: Write incremented value back
	write_byte(static_cast<Address>(address), value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 5 cycles
}

void CPU6502::ISC_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 6: Write incremented value back
	write_byte(effective_address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 6 cycles
}

void CPU6502::ISC_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 6: Write incremented value back
	write_byte(address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 6 cycles
}

void CPU6502::ISC_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 7: Write incremented value back
	write_byte(effective_address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 7 cycles
}

void CPU6502::ISC_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 7: Write incremented value back
	write_byte(effective_address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 7 cycles
}

void CPU6502::ISC_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 8: Write incremented value back
	write_byte(target_address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 8 cycles
}

void CPU6502::ISC_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (increment)
	consume_cycle();
	value++;
	// Cycle 8: Write incremented value back
	write_byte(effective_address, value);
	// Then perform SBC with the incremented value
	perform_sbc(value);
	// Total: 8 cycles
}

// SLO - Shift Left and OR (ASL + ORA combined)
void CPU6502::SLO_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 5: Write shifted value back
	write_byte(static_cast<Address>(address), value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles
}

void CPU6502::SLO_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 6: Write shifted value back
	write_byte(effective_address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::SLO_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 6: Write shifted value back
	write_byte(address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::SLO_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 7: Write shifted value back
	write_byte(effective_address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::SLO_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 7: Write shifted value back
	write_byte(effective_address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::SLO_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 8: Write shifted value back
	write_byte(target_address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

void CPU6502::SLO_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (shift left)
	consume_cycle();
	// Perform ASL operation
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value <<= 1;
	// Cycle 8: Write shifted value back
	write_byte(effective_address, value);
	// Then perform ORA with the shifted value
	accumulator_ |= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

// RLA - Rotate Left and AND (ROL + AND combined)
void CPU6502::RLA_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 5: Write rotated value back
	write_byte(static_cast<Address>(address), value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles
}

void CPU6502::RLA_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 6: Write rotated value back
	write_byte(effective_address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::RLA_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 6: Write rotated value back
	write_byte(address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::RLA_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 7: Write rotated value back
	write_byte(effective_address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::RLA_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 7: Write rotated value back
	write_byte(effective_address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::RLA_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 8: Write rotated value back
	write_byte(target_address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

void CPU6502::RLA_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (rotate left)
	consume_cycle();
	// Perform ROL operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x80) != 0;
	value = (value << 1) | (old_carry ? 1 : 0);
	// Cycle 8: Write rotated value back
	write_byte(effective_address, value);
	// Then perform AND with the rotated value
	accumulator_ &= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

// Continue with the remaining undocumented opcodes in the next part...
// (SRE, RRA, undocumented NOPs, and CRASH)

// SRE - Shift Right and EOR (LSR + EOR combined)
void CPU6502::SRE_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 5: Write shifted value back
	write_byte(static_cast<Address>(address), value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 5 cycles
}

void CPU6502::SRE_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 6: Write shifted value back
	write_byte(effective_address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::SRE_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 6: Write shifted value back
	write_byte(address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 6 cycles
}

void CPU6502::SRE_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 7: Write shifted value back
	write_byte(effective_address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::SRE_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 7: Write shifted value back
	write_byte(effective_address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 7 cycles
}

void CPU6502::SRE_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 8: Write shifted value back
	write_byte(target_address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

void CPU6502::SRE_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (shift right)
	consume_cycle();
	// Perform LSR operation
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value >>= 1;
	// Cycle 8: Write shifted value back
	write_byte(effective_address, value);
	// Then perform EOR with the shifted value
	accumulator_ ^= value;
	update_zero_and_negative_flags(accumulator_);
	// Total: 8 cycles
}

// RRA - Rotate Right and Add (ROR + ADC combined)
void CPU6502::RRA_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read value from zero page
	Byte value = read_byte(static_cast<Address>(address));
	// Cycle 4: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 5: Write rotated value back
	write_byte(static_cast<Address>(address), value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 5 cycles
}

void CPU6502::RRA_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Address effective_address = static_cast<Address>((base_address + x_register_) & 0xFF);
	// Cycle 4: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 5: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 6: Write rotated value back
	write_byte(effective_address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 6 cycles
}

void CPU6502::RRA_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Read value from absolute address
	Address address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(address);
	// Cycle 5: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 6: Write rotated value back
	write_byte(address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 6 cycles
}

void CPU6502::RRA_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add X to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + x_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 7: Write rotated value back
	write_byte(effective_address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 7 cycles
}

void CPU6502::RRA_absolute_Y() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 5: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 6: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 7: Write rotated value back
	write_byte(effective_address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 7 cycles
}

void CPU6502::RRA_indexed_indirect() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	Byte base_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register to base address
	consume_cycle();
	Byte pointer_address = (base_address + x_register_) & 0xFF;
	// Cycle 4: Fetch low byte of target address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 5: Fetch high byte of target address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 6: Read value from target address
	Address target_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Byte value = read_byte(target_address);
	// Cycle 7: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 8: Write rotated value back
	write_byte(target_address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 8 cycles
}

void CPU6502::RRA_indirect_indexed() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	Byte pointer_address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch low byte of base address
	Byte low = read_byte(static_cast<Address>(pointer_address));
	// Cycle 4: Fetch high byte of base address
	Byte high = read_byte(static_cast<Address>((pointer_address + 1) & 0xFF));
	// Cycle 5: Add Y to base address (internal operation)
	consume_cycle(); // Always takes extra cycle for RMW operations
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	Address effective_address = base_address + y_register_;
	// Cycle 6: Read value from effective address
	Byte value = read_byte(effective_address);
	// Cycle 7: Internal operation (rotate right)
	consume_cycle();
	// Perform ROR operation
	bool old_carry = status_.flags.carry_flag_;
	status_.flags.carry_flag_ = (value & 0x01) != 0;
	value = (value >> 1) | (old_carry ? 0x80 : 0);
	// Cycle 8: Write rotated value back
	write_byte(effective_address, value);
	// Then perform ADC with the rotated value
	perform_adc(value);
	// Total: 8 cycles
}

// Undocumented NOPs with different cycle counts and addressing modes
void CPU6502::NOP_immediate() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch immediate value (but ignore it)
	[[maybe_unused]] Byte value = read_byte(program_counter_);
	program_counter_++;
	// Total: 2 cycles
}

void CPU6502::NOP_zero_page() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page address
	[[maybe_unused]] Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Read from zero page (dummy read)
	consume_cycle();
	// Total: 3 cycles
}

void CPU6502::NOP_zero_page_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch zero page base address
	[[maybe_unused]] Byte address = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Add X register (dummy operation)
	consume_cycle();
	// Cycle 4: Dummy read
	consume_cycle();
	// Total: 4 cycles
}

void CPU6502::NOP_absolute() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of address
	[[maybe_unused]] Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of address
	[[maybe_unused]] Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Dummy read
	consume_cycle();
	// Total: 4 cycles
}

void CPU6502::NOP_absolute_X() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Fetch low byte of base address
	Byte low = read_byte(program_counter_);
	program_counter_++;
	// Cycle 3: Fetch high byte of base address
	Byte high = read_byte(program_counter_);
	program_counter_++;
	// Cycle 4: Calculate effective address and read (may cross page boundary)
	Address base_address = static_cast<Address>(low) | (static_cast<Address>(high) << 8);
	if (crosses_page_boundary(base_address, x_register_)) {
		// Cycle 4: Read from wrong page first (dummy read)
		consume_cycle();
	}
	// Cycle 4/5: Read from correct address (dummy read)
	consume_cycle();
	// Total: 4 cycles (5 if page boundary crossed)
}

// Highly unstable opcodes - these will crash/halt the CPU
void CPU6502::CRASH() {
	// These opcodes cause the CPU to enter an undefined state
	// In real hardware, this could cause the CPU to hang, crash, or behave unpredictably
	std::cerr << std::format("CPU CRASH: Highly unstable opcode executed at PC: 0x{:04X}\n",
							 static_cast<int>(program_counter_ - 1));

	// In a real emulator, you might want to:
	// 1. Halt execution completely
	// 2. Reset the CPU
	// 3. Generate an exception
	// For now, we'll just consume some cycles and continue (treating as a very slow NOP)
	consume_cycles(2);

	// Note: Some highly unstable opcodes like ANE/XAA have unpredictable results
	// that depend on analog effects and manufacturing variations
}

// Save state serialization
void CPU6502::serialize_state(std::vector<uint8_t> &buffer) const {
	// Serialize all registers
	buffer.push_back(accumulator_);
	buffer.push_back(x_register_);
	buffer.push_back(y_register_);
	buffer.push_back(stack_pointer_);

	// Program counter (16-bit, little-endian)
	buffer.push_back(static_cast<uint8_t>(program_counter_ & 0xFF));
	buffer.push_back(static_cast<uint8_t>((program_counter_ >> 8) & 0xFF));

	// Status register
	buffer.push_back(status_.status_register_);

	// Cycle count (64-bit, little-endian)
	uint64_t cycle_count = cycles_remaining_.count();
	for (int i = 0; i < 8; ++i) {
		buffer.push_back(static_cast<uint8_t>((cycle_count >> (i * 8)) & 0xFF));
	}

	// Interrupt state (struct with 3 bool flags)
	buffer.push_back(interrupt_state_.nmi_pending ? 1 : 0);
	buffer.push_back(interrupt_state_.irq_pending ? 1 : 0);
	buffer.push_back(interrupt_state_.reset_pending ? 1 : 0);
	buffer.push_back(irq_line_ ? 1 : 0);
	buffer.push_back(nmi_line_ ? 1 : 0);
}

void CPU6502::deserialize_state(const std::vector<uint8_t> &buffer, size_t &offset) {
	// Deserialize all registers
	accumulator_ = buffer[offset++];
	x_register_ = buffer[offset++];
	y_register_ = buffer[offset++];
	stack_pointer_ = buffer[offset++];

	// Program counter (16-bit, little-endian)
	program_counter_ = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;

	// Status register
	status_.status_register_ = buffer[offset++];

	// Cycle count (64-bit, little-endian)
	uint64_t cycle_count = 0;
	for (int i = 0; i < 8; ++i) {
		cycle_count |= static_cast<uint64_t>(buffer[offset++]) << (i * 8);
	}
	cycles_remaining_ = CpuCycle(cycle_count);

	// Interrupt state (struct with 3 bool flags)
	interrupt_state_.nmi_pending = buffer[offset++] != 0;
	interrupt_state_.irq_pending = buffer[offset++] != 0;
	interrupt_state_.reset_pending = buffer[offset++] != 0;
	irq_line_ = buffer[offset++] != 0;
	nmi_line_ = buffer[offset++] != 0;
}

} // namespace nes
