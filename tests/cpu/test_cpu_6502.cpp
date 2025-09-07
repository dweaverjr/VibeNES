// VibeNES - NES Emulator
// CPU 6502 Tests
// Tests for the 6502 CPU core implementation

#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

TEST_CASE("CPU Construction", "[cpu][core]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("CPU should be properly initialized") {
		REQUIRE(std::string(cpu.get_name()) == "6502 CPU");
	}

	SECTION("Registers should be initialized to zero") {
		REQUIRE(cpu.get_accumulator() == 0);
		REQUIRE(cpu.get_x_register() == 0);
		REQUIRE(cpu.get_y_register() == 0);
	}

	SECTION("Stack pointer should be initialized") {
		REQUIRE(cpu.get_stack_pointer() == 0xFF);
	}

	SECTION("Status register should have unused flag set") {
		// Unused flag (bit 5) should always be set
		REQUIRE((cpu.get_status_register() & 0x20) != 0);
	}
}

TEST_CASE("CPU Reset", "[cpu][reset]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("Reset should set PC from reset vector") {
		// For now, manually set the PC since we don't have ROM mapped
		// In a real NES, the reset vector would be in cartridge ROM
		cpu.set_program_counter(0x0200);
		cpu.reset();

		// After reset, PC should be set to the default test reset vector
		REQUIRE(cpu.get_program_counter() == 0x8000);
		REQUIRE(cpu.get_interrupt_flag() == true);
		REQUIRE(cpu.get_stack_pointer() == 0xFD);
	}
}

TEST_CASE("CPU Load Instructions - Immediate Mode", "[cpu][instructions][load]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Immediate - Load Accumulator") {
		// Set up: LDA #$42 in RAM
		cpu.set_program_counter(0x0200); // Use RAM address
		bus->write(0x0200, 0xA9);		 // LDA immediate opcode
		bus->write(0x0201, 0x42);		 // Immediate value

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDA Immediate - Zero flag test") {
		// Set up: LDA #$00
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xA9); // LDA immediate opcode
		bus->write(0x0201, 0x00); // Zero value

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDA Immediate - Negative flag test") {
		// Set up: LDA #$80 (negative value)
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xA9); // LDA immediate opcode
		bus->write(0x0201, 0x80); // Negative value

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x80);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("LDX Immediate - Load X Register") {
		// Set up: LDX #$33
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xA2); // LDX immediate opcode
		bus->write(0x0201, 0x33); // Immediate value

		cpu.execute_instruction();

		REQUIRE(cpu.get_x_register() == 0x33);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDY Immediate - Load Y Register") {
		// Set up: LDY #$44
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xA0); // LDY immediate opcode
		bus->write(0x0201, 0x44); // Immediate value

		cpu.execute_instruction();

		REQUIRE(cpu.get_y_register() == 0x44);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}
}

TEST_CASE("CPU Transfer Instructions", "[cpu][instructions][transfer]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("TAX - Transfer Accumulator to X") {
		// Set up: Load A with value, then TAX
		cpu.set_accumulator(0x55);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xAA); // TAX opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_x_register() == 0x55);
		REQUIRE(cpu.get_accumulator() == 0x55); // A unchanged
		REQUIRE(cpu.get_program_counter() == 0x0201);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("TAY - Transfer Accumulator to Y") {
		// Set up: Load A with value, then TAY
		cpu.set_accumulator(0x66);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xA8); // TAY opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_y_register() == 0x66);
		REQUIRE(cpu.get_accumulator() == 0x66); // A unchanged
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("TXA - Transfer X to Accumulator") {
		// Set up: Load X with value, then TXA
		cpu.set_x_register(0x77);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0x8A); // TXA opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x77);
		REQUIRE(cpu.get_x_register() == 0x77); // X unchanged
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("TYA - Transfer Y to Accumulator") {
		// Set up: Load Y with value, then TYA
		cpu.set_y_register(0x88);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0x98); // TYA opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x88);
		REQUIRE(cpu.get_y_register() == 0x88); // Y unchanged
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("Transfer sets flags correctly") {
		// TAX with zero value
		cpu.set_accumulator(0x00);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xAA); // TAX opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);

		// TAX with negative value
		cpu.set_accumulator(0x80);
		cpu.set_program_counter(0x0201);
		bus->write(0x0201, 0xAA); // TAX opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

TEST_CASE("CPU NOP Instruction", "[cpu][instructions][nop]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("NOP should do nothing but advance PC") {
		// Save initial state
		Byte initial_a = cpu.get_accumulator();
		Byte initial_x = cpu.get_x_register();
		Byte initial_y = cpu.get_y_register();
		Byte initial_status = cpu.get_status_register();

		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xEA); // NOP opcode

		cpu.execute_instruction();

		// All registers should be unchanged
		REQUIRE(cpu.get_accumulator() == initial_a);
		REQUIRE(cpu.get_x_register() == initial_x);
		REQUIRE(cpu.get_y_register() == initial_y);
		REQUIRE(cpu.get_status_register() == initial_status);

		// Only PC should advance
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}
}

TEST_CASE("CPU Simple Program Execution", "[cpu][program]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("Execute simple 6502 program") {
		// Program: LDA #$42, TAX, LDY #$00
		cpu.set_program_counter(0x0200);

		// LDA #$42
		bus->write(0x0200, 0xA9); // LDA immediate
		bus->write(0x0201, 0x42); // Value $42

		// TAX
		bus->write(0x0202, 0xAA); // TAX

		// LDY #$00
		bus->write(0x0203, 0xA0); // LDY immediate
		bus->write(0x0204, 0x00); // Value $00

		// Execute LDA #$42
		cpu.execute_instruction();
		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0202);

		// Execute TAX
		cpu.execute_instruction();
		REQUIRE(cpu.get_x_register() == 0x42);
		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0203);

		// Execute LDY #$00
		cpu.execute_instruction();
		REQUIRE(cpu.get_y_register() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0205);
	}
}

TEST_CASE("CPU Page Boundary Crossing - LDA Absolute,X", "[cpu][instructions][addressing][timing]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Absolute,X - No page boundary crossing (4 cycles)") {
		// Set up: LDA $0200,X with X=0x10, no page boundary crossing
		cpu.set_program_counter(0x0100);
		cpu.set_x_register(0x10);

		// Store test value at target address $0210
		bus->write(0x0210, 0x42);

		// LDA $0200,X instruction
		bus->write(0x0100, 0xBD); // LDA absolute,X opcode
		bus->write(0x0101, 0x00); // Low byte of base address ($0200)
		bus->write(0x0102, 0x02); // High byte of base address

		// Give CPU enough cycles and execute
		cpu.tick(cpu_cycles(4)); // LDA absolute,X takes exactly 4 cycles without page crossing

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0103);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDA Absolute,X - Page boundary crossing (5 cycles)") {
		// Set up: LDA $00FF,X with X=0x01, crosses page boundary (00FF + 01 = 0100)
		cpu.set_program_counter(0x0200);
		cpu.set_x_register(0x01);

		// Store test value at target address $0100 (00FF + 01)
		bus->write(0x0100, 0x99);

		// LDA $00FF,X instruction at PC 0x0200
		bus->write(0x0200, 0xBD); // LDA absolute,X opcode
		bus->write(0x0201, 0xFF); // Low byte of base address ($00FF)
		bus->write(0x0202, 0x00); // High byte of base address

		// Give CPU enough cycles and execute
		cpu.tick(cpu_cycles(5)); // LDA absolute,X takes 5 cycles with page boundary crossing

		REQUIRE(cpu.get_accumulator() == 0x99);
		REQUIRE(cpu.get_program_counter() == 0x0203);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("Page boundary crossing detection edge cases") {
		// Test various boundary conditions
		cpu.set_program_counter(0x0200);

		// Case 1: $00FF + $01 = $0100 (page 0 to page 1)
		cpu.set_x_register(0x01);
		bus->write(0x0100, 0x77); // Target value at $00FF + $01 = $0100
		bus->write(0x0200, 0xBD); // LDA absolute,X at PC
		bus->write(0x0201, 0xFF); // $00FF
		bus->write(0x0202, 0x00);

		cpu.tick(cpu_cycles(5)); // Page boundary crossing: 5 cycles
		REQUIRE(cpu.get_accumulator() == 0x77);
		REQUIRE(cpu.get_program_counter() == 0x0203);

		// Reset for next test
		cpu.set_program_counter(0x0300);

		// Case 2: $01FF + $01 = $0200 (page 1 to page 2, within RAM)
		cpu.set_x_register(0x01);
		bus->write(0x0200, 0x33);
		bus->write(0x0300, 0xBD); // LDA absolute,X
		bus->write(0x0301, 0xFF); // $01FF
		bus->write(0x0302, 0x01);

		cpu.tick(cpu_cycles(5)); // Page boundary crossing: 5 cycles
		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_program_counter() == 0x0303);
	}
}

TEST_CASE("CPU Zero Page Addressing - LDA/STA", "[cpu][instructions][addressing][zero_page]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Zero Page - Load from zero page address") {
		// Set up: LDA $42 (zero page)
		cpu.set_program_counter(0x0100);

		// Store test value at zero page address $0042
		bus->write(0x0042, 0x99);

		// LDA $42 instruction
		bus->write(0x0100, 0xA5); // LDA zero page opcode
		bus->write(0x0101, 0x42); // Zero page address

		// Execute - should take exactly 3 cycles
		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_accumulator() == 0x99);
		REQUIRE(cpu.get_program_counter() == 0x0102);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0x99 has bit 7 set
	}

	SECTION("STA Zero Page - Store to zero page address") {
		// Set up: STA $55 (zero page)
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x77);

		// STA $55 instruction
		bus->write(0x0200, 0x85); // STA zero page opcode
		bus->write(0x0201, 0x55); // Zero page address

		// Execute - should take exactly 3 cycles
		cpu.tick(cpu_cycles(3));

		REQUIRE(bus->read(0x0055) == 0x77);		// Value stored at zero page address
		REQUIRE(cpu.get_accumulator() == 0x77); // Accumulator unchanged
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("LDA/STA Zero Page - Round trip test") {
		// Test that we can store and load back the same value
		cpu.set_program_counter(0x0300);
		cpu.set_accumulator(0xAB);

		// First: STA $88 (store 0xAB to zero page $88)
		bus->write(0x0300, 0x85); // STA zero page
		bus->write(0x0301, 0x88); // Zero page address

		// Second: LDA #$00 (clear accumulator)
		bus->write(0x0302, 0xA9); // LDA immediate
		bus->write(0x0303, 0x00); // Load 0

		// Third: LDA $88 (load back from zero page)
		bus->write(0x0304, 0xA5); // LDA zero page
		bus->write(0x0305, 0x88); // Zero page address

		// Execute all instructions
		cpu.tick(cpu_cycles(8)); // 3 + 2 + 3 cycles

		REQUIRE(cpu.get_accumulator() == 0xAB); // Original value restored
		REQUIRE(bus->read(0x0088) == 0xAB);		// Value preserved in memory
		REQUIRE(cpu.get_program_counter() == 0x0306);
	}

	SECTION("Zero Page boundary behavior") {
		// Test edge cases with zero page addressing
		cpu.set_program_counter(0x0400);

		// Test accessing address $00FF (highest zero page address)
		bus->write(0x00FF, 0x33);
		bus->write(0x0400, 0xA5); // LDA zero page
		bus->write(0x0401, 0xFF); // Address $FF

		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_program_counter() == 0x0402);

		// Test accessing address $0000 (lowest zero page address)
		cpu.set_accumulator(0x44);
		bus->write(0x0402, 0x85); // STA zero page
		bus->write(0x0403, 0x00); // Address $00

		cpu.tick(cpu_cycles(3));

		REQUIRE(bus->read(0x0000) == 0x44);
		REQUIRE(cpu.get_program_counter() == 0x0404);
	}
}
