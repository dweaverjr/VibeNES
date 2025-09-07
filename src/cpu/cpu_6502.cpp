#include "cpu/cpu_6502.hpp"
#include "core/bus.hpp"
#include <format>
#include <iostream>

namespace nes {

CPU6502::CPU6502(SystemBus *bus)
	: accumulator_(0), x_register_(0), y_register_(0), stack_pointer_(0xFF) // Stack starts at top
	  ,
	  program_counter_(0), status_register_(0x20) // Unused flag always set
	  ,
	  bus_(bus), cycles_remaining_(0) {

	// Ensure unused flag is always set
	unused_flag_ = true;
}

void CPU6502::tick(CpuCycle cycles) {
	cycles_remaining_ += cycles;

	// Execute instructions while we have cycles
	while (cycles_remaining_ > CpuCycle{0}) {
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
	status_register_ = 0x20; // Unused flag set
	interrupt_flag_ = true;	 // Interrupts disabled

	cycles_remaining_ = CpuCycle{7}; // Reset takes 7 cycles
}

void CPU6502::power_on() {
	// Power-on reset
	accumulator_ = 0;
	x_register_ = 0;
	y_register_ = 0;
	stack_pointer_ = 0xFF;

	// Status register power-on state
	status_register_ = 0x20; // Only unused flag set
	interrupt_flag_ = true;	 // Interrupts disabled

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

	// Load Accumulator - Absolute,X
	case 0xBD:
		LDA_absolute_X();
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
	zero_flag_ = (value == 0);
}

void CPU6502::update_negative_flag(Byte value) noexcept {
	negative_flag_ = (value & 0x80) != 0;
}

void CPU6502::update_zero_and_negative_flags(Byte value) noexcept {
	update_zero_flag(value);
	update_negative_flag(value);
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

void CPU6502::NOP() {
	// Cycle 1: Fetch opcode (already consumed in execute_instruction)
	// Cycle 2: Do nothing
	consume_cycle(); // NOP still takes 1 cycle to execute
					 // Total: 2 cycles
}

} // namespace nes
