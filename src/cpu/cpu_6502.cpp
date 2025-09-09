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
	  bus_(bus), cycles_remaining_(0) {

	// Ensure unused flag is always set
	status_.flags.unused_flag_ = true;
}

void CPU6502::tick(CpuCycle cycles) {
	cycles_remaining_ += cycles;

	// Execute instructions while we have cycles
	while (cycles_remaining_.count() > 0) {
		execute_instruction();
	}
}
void CPU6502::reset() {
	// For testing, use a known reset vector
	// In a real NES, this would read from cartridge ROM at $FFFC-$FFFD
	program_counter_ = 0x8000;

	// Reset registers to known state
	accumulator_ = 0;
	x_register_ = 0;
	y_register_ = 0;
	stack_pointer_ = 0xFD; // Stack pointer decremented by 3 on reset

	// Set status flags
	status_.status_register_ = 0x20;	  // Unused flag set
	status_.flags.interrupt_flag_ = true; // Interrupts disabled

	cycles_remaining_ = CpuCycle{7}; // Reset takes 7 cycles
}

void CPU6502::power_on() {
	// Power-on reset
	accumulator_ = 0;
	x_register_ = 0;
	y_register_ = 0;
	stack_pointer_ = 0xFF;

	// Status register power-on state
	status_.status_register_ = 0x20;	  // Only unused flag set
	status_.flags.interrupt_flag_ = true; // Interrupts disabled

	// Program counter will be set by reset
	program_counter_ = 0;
	cycles_remaining_ = CpuCycle{0};
}

const char *CPU6502::get_name() const noexcept {
	return "6502 CPU";
}

void CPU6502::execute_instruction() {
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

	// No Operation
	case 0xEA:
		NOP();
		break;

	default:
		std::cerr << std::format("Unknown opcode: 0x{:02X} at PC: 0x{:04X}\n", static_cast<int>(opcode),
								 static_cast<int>(program_counter_ - 1));
		// For now, treat unknown opcodes as NOP
		cycles_remaining_ -= CpuCycle{2};
		break;
	}
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

// Cycle management helpers
void CPU6502::consume_cycle() noexcept {
	cycles_remaining_ -= CpuCycle{1};
}

void CPU6502::consume_cycles(int count) noexcept {
	cycles_remaining_ -= CpuCycle{count};
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
}

// Stack Operations
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
	// Cycle 3: Increment stack pointer (internal)
	consume_cycle();
	// Cycle 4: Pull accumulator from stack
	accumulator_ = pull_byte();

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
	// Cycle 3: Increment stack pointer (internal)
	consume_cycle();
	// Cycle 4: Pull status register from stack
	status_.status_register_ = pull_byte();
	// Note: Bit 5 (unused flag) is always set, bit 4 (B flag) is ignored
	status_.status_register_ |= 0x20;					  // Ensure unused flag is set
	status_.status_register_ &= static_cast<Byte>(~0x10); // Clear B flag (it doesn't exist in the actual register)
														  // Total: 4 cycles
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

} // namespace nes
