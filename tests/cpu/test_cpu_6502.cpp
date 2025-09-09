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

TEST_CASE("CPU Absolute Addressing - LDA/STA", "[cpu][instructions][addressing][absolute]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Absolute - Load from absolute address") {
		// Set up: LDA $1234 (absolute)
		cpu.set_program_counter(0x0100);

		// Store test value at absolute address $1234
		bus->write(0x1234, 0xAB);

		// LDA $1234 instruction
		bus->write(0x0100, 0xAD); // LDA absolute opcode
		bus->write(0x0101, 0x34); // Low byte of address (little-endian)
		bus->write(0x0102, 0x12); // High byte of address

		// Execute - should take exactly 4 cycles
		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0xAB);
		REQUIRE(cpu.get_program_counter() == 0x0103);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0xAB has bit 7 set
	}

	SECTION("STA Absolute - Store to absolute address") {
		// Set up: STA $1800 (absolute)
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x55);

		// STA $1800 instruction
		bus->write(0x0200, 0x8D); // STA absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address (little-endian)
		bus->write(0x0202, 0x18); // High byte of address

		// Execute - should take exactly 4 cycles
		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x1800) == 0x55);		// Value stored at absolute address
		REQUIRE(cpu.get_accumulator() == 0x55); // Accumulator unchanged
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("LDA/STA Absolute - Round trip test") {
		// Test that we can store and load back the same value across full address space
		cpu.set_program_counter(0x0300);
		cpu.set_accumulator(0xCD);

		// First: STA $1999 (store 0xCD to absolute $1999)
		bus->write(0x0300, 0x8D); // STA absolute
		bus->write(0x0301, 0x99); // Low byte
		bus->write(0x0302, 0x19); // High byte

		// Second: LDA #$00 (clear accumulator)
		bus->write(0x0303, 0xA9); // LDA immediate
		bus->write(0x0304, 0x00); // Load 0

		// Third: LDA $1999 (load back from absolute)
		bus->write(0x0305, 0xAD); // LDA absolute
		bus->write(0x0306, 0x99); // Low byte
		bus->write(0x0307, 0x19); // High byte

		// Execute all instructions: 4 + 2 + 4 = 10 cycles
		cpu.tick(cpu_cycles(10));

		REQUIRE(cpu.get_accumulator() == 0xCD); // Original value restored
		REQUIRE(bus->read(0x1999) == 0xCD);		// Value preserved in memory
		REQUIRE(cpu.get_program_counter() == 0x0308);
	}

	SECTION("Absolute addressing full range test") {
		// Test accessing various addresses across the memory map
		cpu.set_program_counter(0x0400);

		// Test high RAM address (but still in RAM range)
		bus->write(0x1FFF, 0x77); // Highest RAM address
		bus->write(0x0400, 0xAD); // LDA absolute
		bus->write(0x0401, 0xFF); // Low byte
		bus->write(0x0402, 0x1F); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x77);
		REQUIRE(cpu.get_program_counter() == 0x0403);

		// Test storing to different high address
		cpu.set_accumulator(0x88);
		bus->write(0x0403, 0x8D); // STA absolute
		bus->write(0x0404, 0x00); // Low byte
		bus->write(0x0405, 0x1E); // High byte ($1E00)

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x1E00) == 0x88);
		REQUIRE(cpu.get_program_counter() == 0x0406);
	}

	SECTION("Little-endian address handling") {
		// Verify that little-endian address encoding works correctly
		cpu.set_program_counter(0x0500);

		// Test address $ABCD encoded as $CD $AB (little-endian)
		bus->write(0x1ACD, 0x42); // Note: using $1ACD instead of $ABCD to stay in RAM
		bus->write(0x0500, 0xAD); // LDA absolute
		bus->write(0x0501, 0xCD); // Low byte first
		bus->write(0x0502, 0x1A); // High byte second

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0503);
	}
}

TEST_CASE("CPU Zero Page,X Addressing - LDA/STA", "[cpu][instructions][addressing][zero_page_x]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Zero Page,X - Basic indexed access") {
		// Set up: LDA $50,X with X=0x10, so effective address = $60
		cpu.set_program_counter(0x0100);
		cpu.set_x_register(0x10);

		// Store test value at effective address $0060
		bus->write(0x0060, 0xAB);

		// LDA $50,X instruction
		bus->write(0x0100, 0xB5); // LDA zero page,X opcode
		bus->write(0x0101, 0x50); // Base address

		// Execute - should take exactly 4 cycles
		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0xAB);
		REQUIRE(cpu.get_x_register() == 0x10); // X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0102);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0xAB has bit 7 set
	}

	SECTION("STA Zero Page,X - Basic indexed store") {
		// Set up: STA $80,X with X=0x20, so effective address = $A0
		cpu.set_program_counter(0x0200);
		cpu.set_x_register(0x20);
		cpu.set_accumulator(0x55);

		// STA $80,X instruction
		bus->write(0x0200, 0x95); // STA zero page,X opcode
		bus->write(0x0201, 0x80); // Base address

		// Execute - should take exactly 4 cycles
		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x00A0) == 0x55);		// Value stored at effective address
		REQUIRE(cpu.get_accumulator() == 0x55); // Accumulator unchanged
		REQUIRE(cpu.get_x_register() == 0x20);	// X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("Zero Page,X wrap-around behavior") {
		// When base + X > 0xFF, it wraps around within zero page
		cpu.set_program_counter(0x0300);
		cpu.set_x_register(0x80); // Large X value
		cpu.set_accumulator(0xCD);

		// Test: STA $A0,X -> effective address = ($A0 + $80) & $FF = $20
		bus->write(0x0300, 0x95); // STA zero page,X
		bus->write(0x0301, 0xA0); // Base address

		cpu.tick(cpu_cycles(4));

		// Should store at $0020, not $0120
		REQUIRE(bus->read(0x0020) == 0xCD);
		REQUIRE(bus->read(0x0120) == 0x00); // Should be unchanged
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}

	SECTION("LDA/STA Zero Page,X - Round trip test") {
		// Test storing and loading back with indexing
		cpu.set_program_counter(0x0400);
		cpu.set_x_register(0x05);
		cpu.set_accumulator(0x99);

		// First: STA $70,X (store 0x99 to $75)
		bus->write(0x0400, 0x95); // STA zero page,X
		bus->write(0x0401, 0x70); // Base address

		// Second: LDA #$00 (clear accumulator)
		bus->write(0x0402, 0xA9); // LDA immediate
		bus->write(0x0403, 0x00); // Load 0

		// Third: LDA $70,X (load back from $75)
		bus->write(0x0404, 0xB5); // LDA zero page,X
		bus->write(0x0405, 0x70); // Base address

		// Execute all instructions: 4 + 2 + 4 = 10 cycles
		cpu.tick(cpu_cycles(10));

		REQUIRE(cpu.get_accumulator() == 0x99); // Original value restored
		REQUIRE(bus->read(0x0075) == 0x99);		// Value preserved in memory
		REQUIRE(cpu.get_program_counter() == 0x0406);
	}

	SECTION("Zero Page,X boundary cases") {
		cpu.set_program_counter(0x0500);

		// Test with X=0 (no indexing)
		cpu.set_x_register(0x00);
		bus->write(0x0030, 0x42); // Store test value
		bus->write(0x0500, 0xB5); // LDA zero page,X
		bus->write(0x0501, 0x30); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0502);

		// Test with maximum wrap-around: $FF + $01 = $00
		cpu.set_x_register(0x01);
		cpu.set_accumulator(0x88);
		bus->write(0x0502, 0x95); // STA zero page,X
		bus->write(0x0503, 0xFF); // Base address $FF

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x0000) == 0x88); // Stored at $00 (wrapped)
		REQUIRE(cpu.get_program_counter() == 0x0504);
	}

	SECTION("Zero Page,X flag behavior") {
		// Test zero flag
		cpu.set_program_counter(0x0600);
		cpu.set_x_register(0x05);
		bus->write(0x0025, 0x00); // Store zero value
		bus->write(0x0600, 0xB5); // LDA zero page,X
		bus->write(0x0601, 0x20); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);

		// Test negative flag
		cpu.set_program_counter(0x0602);
		bus->write(0x0026, 0x80); // Store negative value
		bus->write(0x0602, 0xB5); // LDA zero page,X
		bus->write(0x0603, 0x21); // Base address (21 + 05 = 26)

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x80);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

TEST_CASE("CPU Absolute,Y Addressing - LDA/STA", "[cpu][instructions][addressing][absolute_y]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDA Absolute,Y - No page boundary crossing (4 cycles)") {
		// Set up: LDA $1234,Y with Y=0x10, effective address = $1244
		cpu.set_program_counter(0x0100);
		cpu.set_y_register(0x10);

		// Store test value at effective address
		bus->write(0x1244, 0xAB);

		// LDA $1234,Y instruction
		bus->write(0x0100, 0xB9); // LDA absolute,Y opcode
		bus->write(0x0101, 0x34); // Low byte of base address
		bus->write(0x0102, 0x12); // High byte of base address

		// Execute - should take exactly 4 cycles (no page boundary crossing)
		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0xAB);
		REQUIRE(cpu.get_y_register() == 0x10); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0103);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0xAB has bit 7 set
	}

	SECTION("LDA Absolute,Y - Page boundary crossing (5 cycles)") {
		// Set up: LDA $12FF,Y with Y=0x01, effective address = $1300 (crosses page boundary)
		cpu.set_program_counter(0x0200);
		cpu.set_y_register(0x01);

		// Store test value at effective address
		bus->write(0x1300, 0x55);

		// LDA $12FF,Y instruction
		bus->write(0x0200, 0xB9); // LDA absolute,Y opcode
		bus->write(0x0201, 0xFF); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address

		// Execute - should take exactly 5 cycles (page boundary crossed)
		cpu.tick(cpu_cycles(5));

		REQUIRE(cpu.get_accumulator() == 0x55);
		REQUIRE(cpu.get_y_register() == 0x01); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0203);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false); // 0x55 has bit 7 clear
	}

	SECTION("STA Absolute,Y - Always 5 cycles regardless of page boundary") {
		// Set up: STA $1800,Y with Y=0x20, effective address = $1820
		cpu.set_program_counter(0x0300);
		cpu.set_y_register(0x20);
		cpu.set_accumulator(0xCD);

		// STA $1800,Y instruction
		bus->write(0x0300, 0x99); // STA absolute,Y opcode
		bus->write(0x0301, 0x00); // Low byte of base address
		bus->write(0x0302, 0x18); // High byte of base address

		// Execute - should take exactly 5 cycles (STA always takes 5)
		cpu.tick(cpu_cycles(5));

		REQUIRE(bus->read(0x1820) == 0xCD);		// Value stored at effective address
		REQUIRE(cpu.get_accumulator() == 0xCD); // Accumulator unchanged
		REQUIRE(cpu.get_y_register() == 0x20);	// Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0303);
	}

	SECTION("STA Absolute,Y - Page boundary crossing still 5 cycles") {
		// Set up: STA $18FF,Y with Y=0x02, effective address = $1901 (crosses page boundary)
		cpu.set_program_counter(0x0400);
		cpu.set_y_register(0x02);
		cpu.set_accumulator(0x99);

		// STA $18FF,Y instruction
		bus->write(0x0400, 0x99); // STA absolute,Y opcode
		bus->write(0x0401, 0xFF); // Low byte of base address
		bus->write(0x0402, 0x18); // High byte of base address

		// Execute - should take exactly 5 cycles (STA always takes 5)
		cpu.tick(cpu_cycles(5));

		REQUIRE(bus->read(0x1901) == 0x99);		// Value stored at effective address
		REQUIRE(cpu.get_accumulator() == 0x99); // Accumulator unchanged
		REQUIRE(cpu.get_y_register() == 0x02);	// Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0403);
	}

	SECTION("Absolute,Y page boundary detection edge cases") {
		cpu.set_program_counter(0x0500);

		// Test exact page boundary: $12FF + $01 = $1300
		cpu.set_y_register(0x01);
		bus->write(0x1300, 0x42);
		bus->write(0x0500, 0xB9); // LDA absolute,Y
		bus->write(0x0501, 0xFF); // Low byte
		bus->write(0x0502, 0x12); // High byte

		cpu.tick(cpu_cycles(5)); // Should take 5 cycles (page boundary crossed)

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_program_counter() == 0x0503);

		// Test no page boundary: $1200 + $FE = $12FE (same page)
		cpu.set_y_register(0xFE);
		bus->write(0x12FE, 0x88);
		bus->write(0x0503, 0xB9); // LDA absolute,Y
		bus->write(0x0504, 0x00); // Low byte
		bus->write(0x0505, 0x12); // High byte

		cpu.tick(cpu_cycles(4)); // Should take 4 cycles (no page boundary crossing)

		REQUIRE(cpu.get_accumulator() == 0x88);
		REQUIRE(cpu.get_program_counter() == 0x0506);
	}

	SECTION("LDA/STA Absolute,Y - Round trip test") {
		// Test storing and loading back with Y indexing
		cpu.set_program_counter(0x0600);
		cpu.set_y_register(0x05);
		cpu.set_accumulator(0x77);

		// First: STA $1500,Y (store 0x77 to $1505)
		bus->write(0x0600, 0x99); // STA absolute,Y
		bus->write(0x0601, 0x00); // Low byte
		bus->write(0x0602, 0x15); // High byte

		// Second: LDA #$00 (clear accumulator)
		bus->write(0x0603, 0xA9); // LDA immediate
		bus->write(0x0604, 0x00); // Load 0

		// Third: LDA $1500,Y (load back from $1505)
		bus->write(0x0605, 0xB9); // LDA absolute,Y
		bus->write(0x0606, 0x00); // Low byte
		bus->write(0x0607, 0x15); // High byte

		// Execute all instructions: 5 + 2 + 4 = 11 cycles
		cpu.tick(cpu_cycles(11));

		REQUIRE(cpu.get_accumulator() == 0x77); // Original value restored
		REQUIRE(bus->read(0x1505) == 0x77);		// Value preserved in memory
		REQUIRE(cpu.get_program_counter() == 0x0608);
	}

	SECTION("Absolute,Y with Y=0 (no indexing)") {
		// Test that Absolute,Y works correctly with Y=0
		cpu.set_program_counter(0x0700);
		cpu.set_y_register(0x00);

		bus->write(0x1234, 0x33); // Store test value
		bus->write(0x0700, 0xB9); // LDA absolute,Y
		bus->write(0x0701, 0x34); // Low byte
		bus->write(0x0702, 0x12); // High byte

		cpu.tick(cpu_cycles(4)); // No page boundary crossing

		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_program_counter() == 0x0703);
	}

	SECTION("Absolute,Y flag behavior") {
		cpu.set_program_counter(0x0800);
		cpu.set_y_register(0x10);

		// Test zero flag
		bus->write(0x1310, 0x00); // Store zero value
		bus->write(0x0800, 0xB9); // LDA absolute,Y
		bus->write(0x0801, 0x00); // Low byte
		bus->write(0x0802, 0x13); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);

		// Test negative flag
		cpu.set_program_counter(0x0803);
		bus->write(0x1311, 0x80); // Store negative value
		bus->write(0x0803, 0xB9); // LDA absolute,Y
		bus->write(0x0804, 0x01); // Low byte
		bus->write(0x0805, 0x13); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_accumulator() == 0x80);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

TEST_CASE("CPU LDX/LDY Addressing Modes", "[cpu][instructions][ldx][ldy]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("LDX Zero Page") {
		// Test: LDX $42
		cpu.set_program_counter(0x0100);

		// Store test value at zero page address
		bus->write(0x0042, 0x55);

		// LDX $42 instruction
		bus->write(0x0100, 0xA6); // LDX zero page opcode
		bus->write(0x0101, 0x42); // Zero page address

		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_x_register() == 0x55);
		REQUIRE(cpu.get_program_counter() == 0x0102);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDY Zero Page") {
		// Test: LDY $88
		cpu.set_program_counter(0x0200);

		// Store test value at zero page address
		bus->write(0x0088, 0xAA);

		// LDY $88 instruction
		bus->write(0x0200, 0xA4); // LDY zero page opcode
		bus->write(0x0201, 0x88); // Zero page address

		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_y_register() == 0xAA);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0xAA has bit 7 set
	}

	SECTION("LDY Zero Page,X") {
		// Test: LDY $50,X with X=0x10
		cpu.set_program_counter(0x0300);
		cpu.set_x_register(0x10);

		// Store test value at effective address $60
		bus->write(0x0060, 0x77);

		// LDY $50,X instruction
		bus->write(0x0300, 0xB4); // LDY zero page,X opcode
		bus->write(0x0301, 0x50); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_y_register() == 0x77);
		REQUIRE(cpu.get_program_counter() == 0x0302);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDX Zero Page,Y") {
		// Test: LDX $30,Y with Y=0x05
		cpu.set_program_counter(0x0400);
		cpu.set_y_register(0x05);

		// Store test value at effective address $35
		bus->write(0x0035, 0x99);

		// LDX $30,Y instruction
		bus->write(0x0400, 0xB6); // LDX zero page,Y opcode
		bus->write(0x0401, 0x30); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_x_register() == 0x99);
		REQUIRE(cpu.get_program_counter() == 0x0402);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0x99 has bit 7 set
	}

	SECTION("LDX Absolute") {
		// Test: LDX $1234
		cpu.set_program_counter(0x0500);

		// Store test value at absolute address
		bus->write(0x1234, 0x33);

		// LDX $1234 instruction
		bus->write(0x0500, 0xAE); // LDX absolute opcode
		bus->write(0x0501, 0x34); // Low byte
		bus->write(0x0502, 0x12); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_x_register() == 0x33);
		REQUIRE(cpu.get_program_counter() == 0x0503);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDY Absolute") {
		// Test: LDY $1678
		cpu.set_program_counter(0x0600);

		// Store test value at absolute address
		bus->write(0x1678, 0x44);

		// LDY $1678 instruction
		bus->write(0x0600, 0xAC); // LDY absolute opcode
		bus->write(0x0601, 0x78); // Low byte
		bus->write(0x0602, 0x16); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(cpu.get_y_register() == 0x44);
		REQUIRE(cpu.get_program_counter() == 0x0603);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDY Absolute,X - No page crossing") {
		// Test: LDY $1200,X with X=0x10
		cpu.set_program_counter(0x0700);
		cpu.set_x_register(0x10);

		// Store test value at effective address $1210
		bus->write(0x1210, 0x66);

		// LDY $1200,X instruction
		bus->write(0x0700, 0xBC); // LDY absolute,X opcode
		bus->write(0x0701, 0x00); // Low byte
		bus->write(0x0702, 0x12); // High byte

		cpu.tick(cpu_cycles(4)); // No page crossing

		REQUIRE(cpu.get_y_register() == 0x66);
		REQUIRE(cpu.get_program_counter() == 0x0703);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDY Absolute,X - Page crossing") {
		// Test: LDY $12FF,X with X=0x01 (crosses to $1300)
		cpu.set_program_counter(0x0800);
		cpu.set_x_register(0x01);

		// Store test value at effective address $1300
		bus->write(0x1300, 0x88);

		// LDY $12FF,X instruction
		bus->write(0x0800, 0xBC); // LDY absolute,X opcode
		bus->write(0x0801, 0xFF); // Low byte
		bus->write(0x0802, 0x12); // High byte

		cpu.tick(cpu_cycles(5)); // Page crossing adds 1 cycle

		REQUIRE(cpu.get_y_register() == 0x88);
		REQUIRE(cpu.get_program_counter() == 0x0803);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0x88 has bit 7 set
	}

	SECTION("LDX Absolute,Y - No page crossing") {
		// Test: LDX $1400,Y with Y=0x20
		cpu.set_program_counter(0x0900);
		cpu.set_y_register(0x20);

		// Store test value at effective address $1420
		bus->write(0x1420, 0x11);

		// LDX $1400,Y instruction
		bus->write(0x0900, 0xBE); // LDX absolute,Y opcode
		bus->write(0x0901, 0x00); // Low byte
		bus->write(0x0902, 0x14); // High byte

		cpu.tick(cpu_cycles(4)); // No page crossing

		REQUIRE(cpu.get_x_register() == 0x11);
		REQUIRE(cpu.get_program_counter() == 0x0903);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LDX Absolute,Y - Page crossing") {
		// Test: LDX $14FF,Y with Y=0x01 (crosses to $1500)
		cpu.set_program_counter(0x0A00);
		cpu.set_y_register(0x01);

		// Store test value at effective address $1500
		bus->write(0x1500, 0xCC);

		// LDX $14FF,Y instruction
		bus->write(0x0A00, 0xBE); // LDX absolute,Y opcode
		bus->write(0x0A01, 0xFF); // Low byte
		bus->write(0x0A02, 0x14); // High byte

		cpu.tick(cpu_cycles(5)); // Page crossing adds 1 cycle

		REQUIRE(cpu.get_x_register() == 0xCC);
		REQUIRE(cpu.get_program_counter() == 0x0A03);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true); // 0xCC has bit 7 set
	}

	SECTION("Zero and negative flag behavior") {
		// Test zero flag
		cpu.set_program_counter(0x0B00);
		bus->write(0x00FF, 0x00); // Zero value
		bus->write(0x0B00, 0xA6); // LDX zero page
		bus->write(0x0B01, 0xFF);

		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_x_register() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);

		// Test negative flag
		cpu.set_program_counter(0x0C00);
		bus->write(0x00EE, 0x80); // Negative value
		bus->write(0x0C00, 0xA4); // LDY zero page
		bus->write(0x0C01, 0xEE);

		cpu.tick(cpu_cycles(3));

		REQUIRE(cpu.get_y_register() == 0x80);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

// STX Instruction Tests
TEST_CASE("STX Instructions", "[cpu][stx]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("STX Zero Page - Basic functionality") {
		// Test: STX $55
		cpu.set_x_register(0x42);
		cpu.set_program_counter(0x0100);

		// STX $55 instruction
		bus->write(0x0100, 0x86); // STX zero page opcode
		bus->write(0x0101, 0x55); // Zero page address

		cpu.tick(cpu_cycles(3));

		REQUIRE(bus->read(0x0055) == 0x42);	   // Value stored at zero page address
		REQUIRE(cpu.get_x_register() == 0x42); // X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("STX Zero Page,Y - Basic functionality") {
		// Test: STX $80,Y with Y=0x05
		cpu.set_x_register(0x33);
		cpu.set_y_register(0x05);
		cpu.set_program_counter(0x0200);

		// STX $80,Y instruction
		bus->write(0x0200, 0x96); // STX zero page,Y opcode
		bus->write(0x0201, 0x80); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x0085) == 0x33);	   // Value stored at effective address (0x80 + 0x05)
		REQUIRE(cpu.get_x_register() == 0x33); // X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("STX Zero Page,Y - Wrapping") {
		// Test: STX $02,Y with Y=0xFF (wraps to 0x01)
		cpu.set_x_register(0x77);
		cpu.set_y_register(0xFF);
		cpu.set_program_counter(0x0300);

		// STX $02,Y instruction
		bus->write(0x0300, 0x96); // STX zero page,Y opcode
		bus->write(0x0301, 0x02); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x0001) == 0x77);	   // Value stored at wrapped address (0x02 + 0xFF) & 0xFF = 0x01
		REQUIRE(cpu.get_x_register() == 0x77); // X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}

	SECTION("STX Absolute - Basic functionality") {
		// Test: STX $3000
		cpu.set_x_register(0x99);
		cpu.set_program_counter(0x0400);

		// STX $3000 instruction
		bus->write(0x0400, 0x8E); // STX absolute opcode
		bus->write(0x0401, 0x00); // Low byte
		bus->write(0x0402, 0x30); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x3000) == 0x99);	   // Value stored at absolute address
		REQUIRE(cpu.get_x_register() == 0x99); // X register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0403);
	}

	SECTION("STX - No flags affected") {
		// Test: STX $50 with zero value
		cpu.set_x_register(0x00); // Zero value
		cpu.set_zero_flag(false);
		cpu.set_negative_flag(false);
		cpu.set_program_counter(0x0500);

		// STX $50 instruction
		bus->write(0x0500, 0x86); // STX zero page opcode
		bus->write(0x0501, 0x50); // Zero page address

		cpu.tick(cpu_cycles(3));

		// STX should not affect any flags
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(bus->read(0x0050) == 0x00); // Value stored correctly
		REQUIRE(cpu.get_program_counter() == 0x0502);
	}
}

// STY Instruction Tests
TEST_CASE("STY Instructions", "[cpu][sty]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("STY Zero Page - Basic functionality") {
		// Test: STY $55
		cpu.set_y_register(0x42);
		cpu.set_program_counter(0x0100);

		// STY $55 instruction
		bus->write(0x0100, 0x84); // STY zero page opcode
		bus->write(0x0101, 0x55); // Zero page address

		cpu.tick(cpu_cycles(3));

		REQUIRE(bus->read(0x0055) == 0x42);	   // Value stored at zero page address
		REQUIRE(cpu.get_y_register() == 0x42); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("STY Zero Page,X - Basic functionality") {
		// Test: STY $80,X with X=0x05
		cpu.set_y_register(0x33);
		cpu.set_x_register(0x05);
		cpu.set_program_counter(0x0200);

		// STY $80,X instruction
		bus->write(0x0200, 0x94); // STY zero page,X opcode
		bus->write(0x0201, 0x80); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x0085) == 0x33);	   // Value stored at effective address (0x80 + 0x05)
		REQUIRE(cpu.get_y_register() == 0x33); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("STY Zero Page,X - Wrapping") {
		// Test: STY $02,X with X=0xFF (wraps to 0x01)
		cpu.set_y_register(0x77);
		cpu.set_x_register(0xFF);
		cpu.set_program_counter(0x0300);

		// STY $02,X instruction
		bus->write(0x0300, 0x94); // STY zero page,X opcode
		bus->write(0x0301, 0x02); // Base address

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x0001) == 0x77);	   // Value stored at wrapped address (0x02 + 0xFF) & 0xFF = 0x01
		REQUIRE(cpu.get_y_register() == 0x77); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}

	SECTION("STY Absolute - Basic functionality") {
		// Test: STY $3000
		cpu.set_y_register(0x99);
		cpu.set_program_counter(0x0400);

		// STY $3000 instruction
		bus->write(0x0400, 0x8C); // STY absolute opcode
		bus->write(0x0401, 0x00); // Low byte
		bus->write(0x0402, 0x30); // High byte

		cpu.tick(cpu_cycles(4));

		REQUIRE(bus->read(0x3000) == 0x99);	   // Value stored at absolute address
		REQUIRE(cpu.get_y_register() == 0x99); // Y register unchanged
		REQUIRE(cpu.get_program_counter() == 0x0403);
	}

	SECTION("STY - No flags affected") {
		// Test: STY $50 with zero value
		cpu.set_y_register(0x00); // Zero value
		cpu.set_zero_flag(false);
		cpu.set_negative_flag(false);
		cpu.set_program_counter(0x0500);

		// STY $50 instruction
		bus->write(0x0500, 0x84); // STY zero page opcode
		bus->write(0x0501, 0x50); // Zero page address

		cpu.tick(cpu_cycles(3));

		// STY should not affect any flags
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(bus->read(0x0050) == 0x00); // Value stored correctly
		REQUIRE(cpu.get_program_counter() == 0x0502);
	}
}

TEST_CASE("CPU 6502 - LDA Indexed Indirect (zp,X)", "[LDA][indirect]") {
	auto bus = std::make_shared<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("Basic indexed indirect load") {
		// Setup: LDA ($20,X) with X=5, pointer at $25 points to $3000
		cpu.set_program_counter(0x0100);
		cpu.set_x_register(0x05);
		cpu.set_accumulator(0x00);

		// Write instruction: LDA ($20,X) = 0xA1 0x20
		bus->write(0x0100, 0xA1);
		bus->write(0x0101, 0x20);

		// Setup pointer at $20 + $05 = $25 to point to $0500
		bus->write(0x0025, 0x00); // Low byte of target address
		bus->write(0x0026, 0x05); // High byte of target address

		// Write test value at target address
		bus->write(0x0500, 0x42);

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(cpu.get_accumulator() == 0x42);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("Zero page wrap in indexing") {
		// Setup: LDA ($FF,X) with X=2, should wrap to $01
		cpu.set_program_counter(0x0200);
		cpu.set_x_register(0x02);
		cpu.set_accumulator(0x00);

		// Write instruction: LDA ($FF,X) = 0xA1 0xFF
		bus->write(0x0200, 0xA1);
		bus->write(0x0201, 0xFF);

		// Setup pointer at $FF + $02 = $01 (wrapped) to point to $0510
		bus->write(0x0001, 0x10); // Low byte of target address
		bus->write(0x0002, 0x05); // High byte of target address

		// Write test value at target address
		bus->write(0x0510, 0x84);

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(cpu.get_accumulator() == 0x84);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("Zero flag set") {
		// Setup: LDA ($10,X) with X=0, target contains 0
		cpu.set_program_counter(0x0300);
		cpu.set_x_register(0x00);
		cpu.set_accumulator(0xFF);

		// Write instruction: LDA ($10,X) = 0xA1 0x10
		bus->write(0x0300, 0xA1);
		bus->write(0x0301, 0x10);

		// Setup pointer at $10 to point to $0520
		bus->write(0x0010, 0x20); // Low byte of target address
		bus->write(0x0011, 0x05); // High byte of target address

		// Write zero at target address
		bus->write(0x0520, 0x00);

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}
}

TEST_CASE("CPU 6502 - STA Indexed Indirect (zp,X)", "[STA][indirect]") {
	auto bus = std::make_shared<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("Basic indexed indirect store") {
		// Setup: STA ($30,X) with X=3, pointer at $33 points to $2000
		cpu.set_program_counter(0x0100);
		cpu.set_x_register(0x03);
		cpu.set_accumulator(0x7F);

		// Write instruction: STA ($30,X) = 0x81 0x30
		bus->write(0x0100, 0x81);
		bus->write(0x0101, 0x30);

		// Setup pointer at $30 + $03 = $33 to point to $0530
		bus->write(0x0033, 0x30); // Low byte of target address
		bus->write(0x0034, 0x05); // High byte of target address

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(bus->read(0x0530) == 0x7F); // Value stored correctly
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("Zero page wrap in indexing") {
		// Setup: STA ($FE,X) with X=3, should wrap to $01
		cpu.set_program_counter(0x0200);
		cpu.set_x_register(0x03);
		cpu.set_accumulator(0xAB);

		// Write instruction: STA ($FE,X) = 0x81 0xFE
		bus->write(0x0200, 0x81);
		bus->write(0x0201, 0xFE);

		// Setup pointer at $FE + $03 = $01 (wrapped) to point to $0540
		bus->write(0x0001, 0x40); // Low byte of target address
		bus->write(0x0002, 0x05); // High byte of target address

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(bus->read(0x0540) == 0xAB); // Value stored correctly
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("Store zero value") {
		// Setup: STA ($40,X) with X=0, accumulator contains 0
		cpu.set_program_counter(0x0300);
		cpu.set_x_register(0x00);
		cpu.set_accumulator(0x00);

		// Write instruction: STA ($40,X) = 0x81 0x40
		bus->write(0x0300, 0x81);
		bus->write(0x0301, 0x40);

		// Setup pointer at $40 to point to $0550
		bus->write(0x0040, 0x50); // Low byte of target address
		bus->write(0x0041, 0x05); // High byte of target address

		// Initialize target with non-zero value
		bus->write(0x0550, 0xFF);

		// Execute instruction
		cpu.tick(CpuCycle{6});

		REQUIRE(bus->read(0x0550) == 0x00); // Value stored correctly
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}
}

TEST_CASE("CPU 6502 - LDA Indirect Indexed (zp),Y", "[LDA][indirect]") {
	auto bus = std::make_shared<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("Basic indirect indexed load - no page crossing") {
		// Setup: LDA ($50),Y with Y=10, pointer at $50 points to $2000
		cpu.set_program_counter(0x0100);
		cpu.set_y_register(0x0A);
		cpu.set_accumulator(0x00);

		// Write instruction: LDA ($50),Y = 0xB1 0x50
		bus->write(0x0100, 0xB1);
		bus->write(0x0101, 0x50);

		// Setup pointer at $50 to point to $0560
		bus->write(0x0050, 0x60); // Low byte of base address
		bus->write(0x0051, 0x05); // High byte of base address

		// Write test value at target address $0560 + $0A = $056A
		bus->write(0x056A, 0x55);

		// Execute instruction
		cpu.tick(CpuCycle{5}); // No page crossing = 5 cycles

		REQUIRE(cpu.get_accumulator() == 0x55);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("Indirect indexed load - with page crossing") {
		// Setup: LDA ($60),Y with Y=FF, pointer at $60 points to $20FF
		cpu.set_program_counter(0x0200);
		cpu.set_y_register(0xFF);
		cpu.set_accumulator(0x00);

		// Write instruction: LDA ($60),Y = 0xB1 0x60
		bus->write(0x0200, 0xB1);
		bus->write(0x0201, 0x60);

		// Setup pointer at $60 to point to $05FF
		bus->write(0x0060, 0xFF); // Low byte of base address
		bus->write(0x0061, 0x05); // High byte of base address

		// Write test value at target address $05FF + $FF = $06FE (page crossing)
		bus->write(0x06FE, 0x99);

		// Execute instruction
		cpu.tick(CpuCycle{6}); // Page crossing = 6 cycles

		REQUIRE(cpu.get_accumulator() == 0x99);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("Zero page wrap in pointer") {
		// Setup: LDA ($FF),Y with Y=5, pointer wraps to $00
		cpu.set_program_counter(0x0300);
		cpu.set_y_register(0x05);
		cpu.set_accumulator(0x00);

		// Write instruction: LDA ($FF),Y = 0xB1 0xFF
		bus->write(0x0300, 0xB1);
		bus->write(0x0301, 0xFF);

		// Setup pointer at $FF/$00 to point to $0570
		bus->write(0x00FF, 0x70); // Low byte of base address
		bus->write(0x0000, 0x05); // High byte of base address (wrapped)

		// Write test value at target address $0570 + $05 = $0575
		bus->write(0x0575, 0x00);

		// Execute instruction
		cpu.tick(CpuCycle{5}); // No page crossing = 5 cycles

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}
}

TEST_CASE("CPU 6502 - STA Indirect Indexed (zp),Y", "[STA][indirect]") {
	auto bus = std::make_shared<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("Basic indirect indexed store") {
		// Setup: STA ($70),Y with Y=8, pointer at $70 points to $3000
		cpu.set_program_counter(0x0100);
		cpu.set_y_register(0x08);
		cpu.set_accumulator(0xCD);

		// Write instruction: STA ($70),Y = 0x91 0x70
		bus->write(0x0100, 0x91);
		bus->write(0x0101, 0x70);

		// Setup pointer at $70 to point to $0580
		bus->write(0x0070, 0x80); // Low byte of base address
		bus->write(0x0071, 0x05); // High byte of base address

		// Execute instruction
		cpu.tick(CpuCycle{6}); // Store always takes 6 cycles

		REQUIRE(bus->read(0x0588) == 0xCD); // Value stored at $0580 + $08
		REQUIRE(cpu.get_program_counter() == 0x0102);
	}

	SECTION("Indirect indexed store with page crossing") {
		// Setup: STA ($80),Y with Y=FF, pointer at $80 points to $40FF
		cpu.set_program_counter(0x0200);
		cpu.set_y_register(0xFF);
		cpu.set_accumulator(0x12);

		// Write instruction: STA ($80),Y = 0x91 0x80
		bus->write(0x0200, 0x91);
		bus->write(0x0201, 0x80);

		// Setup pointer at $80 to point to $06FF
		bus->write(0x0080, 0xFF); // Low byte of base address
		bus->write(0x0081, 0x06); // High byte of base address

		// Execute instruction
		cpu.tick(CpuCycle{6}); // Store always takes 6 cycles (no extra for page crossing)

		REQUIRE(bus->read(0x07FE) == 0x12); // Value stored at $06FF + $FF = $07FE
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("Zero page wrap in pointer") {
		// Setup: STA ($FF),Y with Y=2, pointer wraps to $00
		cpu.set_program_counter(0x0300);
		cpu.set_y_register(0x02);
		cpu.set_accumulator(0x88);

		// Write instruction: STA ($FF),Y = 0x91 0xFF
		bus->write(0x0300, 0x91);
		bus->write(0x0301, 0xFF);

		// Setup pointer at $FF/$00 to point to $0590
		bus->write(0x00FF, 0x90); // Low byte of base address
		bus->write(0x0000, 0x05); // High byte of base address (wrapped)

		// Execute instruction
		cpu.tick(CpuCycle{6}); // Store always takes 6 cycles

		REQUIRE(bus->read(0x0592) == 0x88); // Value stored at $0590 + $02
		REQUIRE(cpu.get_program_counter() == 0x0302);
	}
}

TEST_CASE("CPU ADC - Add with Carry", "[cpu][instructions][arithmetic]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("ADC Immediate - Basic addition") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x10);
		cpu.set_carry_flag(false);

		// Write instruction: ADC #$20 = 0x69 0x20
		bus->write(0x0200, 0x69);
		bus->write(0x0201, 0x20);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x30);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(!cpu.get_carry_flag());
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(!cpu.get_overflow_flag());
	}

	SECTION("ADC Immediate - Addition with carry") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x50);
		cpu.set_carry_flag(true);

		// Write instruction: ADC #$30 = 0x69 0x30
		bus->write(0x0200, 0x69);
		bus->write(0x0201, 0x30);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x81); // 0x50 + 0x30 + 1 = 0x81
		REQUIRE(!cpu.get_carry_flag());
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(cpu.get_negative_flag()); // Result is negative (bit 7 set)
		REQUIRE(cpu.get_overflow_flag()); // Positive + Positive = Negative (overflow)
	}

	SECTION("ADC Immediate - Carry flag set") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0xFF);
		cpu.set_carry_flag(false);

		// Write instruction: ADC #$01 = 0x69 0x01
		bus->write(0x0200, 0x69);
		bus->write(0x0201, 0x01);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x00); // 0xFF + 0x01 = 0x100, wraps to 0x00
		REQUIRE(cpu.get_carry_flag());			// Carry set due to overflow
		REQUIRE(cpu.get_zero_flag());			// Result is zero
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(!cpu.get_overflow_flag()); // Negative + Positive = Positive (no overflow)
	}

	SECTION("ADC Zero Page") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x40);
		cpu.set_carry_flag(false);

		// Write instruction: ADC $80 = 0x65 0x80
		bus->write(0x0200, 0x65);
		bus->write(0x0201, 0x80);
		bus->write(0x0080, 0x25); // Value at zero page $80

		cpu.tick(CpuCycle{3});

		REQUIRE(cpu.get_accumulator() == 0x65); // 0x40 + 0x25 = 0x65
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ADC Absolute,X with page crossing") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x10);
		cpu.set_x_register(0x01); // Small offset to cross page boundary
		cpu.set_carry_flag(false);

		// Write instruction: ADC $06FF,X = 0x7D 0xFF 0x06
		// $06FF + $01 = $0700 (crosses from page $06 to page $07)
		bus->write(0x0200, 0x7D);
		bus->write(0x0201, 0xFF);
		bus->write(0x0202, 0x06);
		// Add a valid instruction after our ADC to prevent unknown opcode error
		bus->write(0x0203, 0xA9); // LDA #$00
		bus->write(0x0204, 0x00);
		bus->write(0x0700, 0x30); // Value at $06FF + $01 = $0700 (page boundary crossed)

		cpu.tick(CpuCycle{5}); // 5 cycles due to page boundary crossing

		REQUIRE(cpu.get_accumulator() == 0x40); // 0x10 + 0x30 = 0x40
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ADC Absolute,X without page crossing") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x20);
		cpu.set_x_register(0x10); // Small offset, no page crossing
		cpu.set_carry_flag(false);

		// Write instruction: ADC $0600,X = 0x7D 0x00 0x06
		// $0600 + $10 = $0610 (stays within page $06)
		bus->write(0x0200, 0x7D);
		bus->write(0x0201, 0x00);
		bus->write(0x0202, 0x06);
		// Add a valid instruction after our ADC to prevent unknown opcode error
		bus->write(0x0203, 0xA9); // LDA #$00
		bus->write(0x0204, 0x00);
		bus->write(0x0610, 0x25); // Value at $0600 + $10 = $0610 (no page crossing)

		cpu.tick(CpuCycle{4}); // 4 cycles - no page boundary crossing

		REQUIRE(cpu.get_accumulator() == 0x45); // 0x20 + 0x25 = 0x45
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}
}

TEST_CASE("CPU SBC - Subtract with Carry", "[cpu][instructions][arithmetic]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("SBC Immediate - Basic subtraction") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x50);
		cpu.set_carry_flag(true); // Carry set means no borrow

		// Write instruction: SBC #$30 = 0xE9 0x30
		bus->write(0x0200, 0xE9);
		bus->write(0x0201, 0x30);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x20); // 0x50 - 0x30 = 0x20
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_carry_flag()); // No borrow needed
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(!cpu.get_overflow_flag());
	}

	SECTION("SBC Immediate - Subtraction with borrow") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x50);
		cpu.set_carry_flag(false); // Carry clear means borrow

		// Write instruction: SBC #$30 = 0xE9 0x30
		bus->write(0x0200, 0xE9);
		bus->write(0x0201, 0x30);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x1F); // 0x50 - 0x30 - 1 = 0x1F
		REQUIRE(cpu.get_carry_flag());			// No borrow needed for result
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(!cpu.get_overflow_flag());
	}

	SECTION("SBC Immediate - Result goes negative") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x30);
		cpu.set_carry_flag(true);

		// Write instruction: SBC #$50 = 0xE9 0x50
		bus->write(0x0200, 0xE9);
		bus->write(0x0201, 0x50);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0xE0); // 0x30 - 0x50 = 0xE0 (two's complement)
		REQUIRE(!cpu.get_carry_flag());			// Borrow needed
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(cpu.get_negative_flag()); // Result is negative
	}

	SECTION("SBC Immediate - Result is zero") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x50);
		cpu.set_carry_flag(true);

		// Write instruction: SBC #$50 = 0xE9 0x50
		bus->write(0x0200, 0xE9);
		bus->write(0x0201, 0x50);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x00); // 0x50 - 0x50 = 0x00
		REQUIRE(cpu.get_carry_flag());			// No borrow needed
		REQUIRE(cpu.get_zero_flag());			// Result is zero
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(!cpu.get_overflow_flag());
	}

	SECTION("SBC Zero Page") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x80);
		cpu.set_carry_flag(true);

		// Write instruction: SBC $90 = 0xE5 0x90
		bus->write(0x0200, 0xE5);
		bus->write(0x0201, 0x90);
		bus->write(0x0090, 0x20); // Value at zero page $90

		cpu.tick(CpuCycle{3});

		REQUIRE(cpu.get_accumulator() == 0x60); // 0x80 - 0x20 = 0x60
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("SBC Overflow flag test") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x80); // -128 in signed
		cpu.set_carry_flag(true);

		// Write instruction: SBC #$01 = 0xE9 0x01
		bus->write(0x0200, 0xE9);
		bus->write(0x0201, 0x01);

		cpu.tick(CpuCycle{2});

		REQUIRE(cpu.get_accumulator() == 0x7F); // -128 - 1 = 127 (overflow)
		REQUIRE(cpu.get_carry_flag());
		REQUIRE(!cpu.get_zero_flag());
		REQUIRE(!cpu.get_negative_flag());
		REQUIRE(cpu.get_overflow_flag()); // Negative - Positive = Positive (overflow)
	}
}

TEST_CASE("CPU ADC/SBC - All Addressing Modes", "[cpu][instructions][arithmetic][addressing]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("ADC Zero Page,X") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x10);
		cpu.set_x_register(0x05);
		cpu.set_carry_flag(false);

		// Write instruction: ADC $80,X = 0x75 0x80
		bus->write(0x0200, 0x75);
		bus->write(0x0201, 0x80);
		bus->write(0x0085, 0x25); // Value at $80 + $05 = $85

		cpu.tick(CpuCycle{4});

		REQUIRE(cpu.get_accumulator() == 0x35); // 0x10 + 0x25 = 0x35
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ADC Absolute") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x20);
		cpu.set_carry_flag(false);

		// Write instruction: ADC $0600 = 0x6D 0x00 0x06
		bus->write(0x0200, 0x6D);
		bus->write(0x0201, 0x00);
		bus->write(0x0202, 0x06);
		bus->write(0x0600, 0x30); // Value at $0600

		cpu.tick(CpuCycle{4});

		REQUIRE(cpu.get_accumulator() == 0x50); // 0x20 + 0x30 = 0x50
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ADC Indexed Indirect") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x40);
		cpu.set_x_register(0x04);
		cpu.set_carry_flag(false);

		// Write instruction: ADC ($20,X) = 0x61 0x20
		bus->write(0x0200, 0x61);
		bus->write(0x0201, 0x20);

		// Setup pointer at $20 + $04 = $24
		bus->write(0x0024, 0x00); // Low byte of target address
		bus->write(0x0025, 0x07); // High byte of target address
		bus->write(0x0700, 0x15); // Value at target address $0700

		cpu.tick(CpuCycle{6});

		REQUIRE(cpu.get_accumulator() == 0x55); // 0x40 + 0x15 = 0x55
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("SBC Indirect Indexed") {
		cpu.set_program_counter(0x0200);
		cpu.set_accumulator(0x80);
		cpu.set_y_register(0x10);
		cpu.set_carry_flag(true);

		// Write instruction: SBC ($30),Y = 0xF1 0x30
		bus->write(0x0200, 0xF1);
		bus->write(0x0201, 0x30);

		// Setup pointer at $30
		bus->write(0x0030, 0x00); // Low byte of base address
		bus->write(0x0031, 0x05); // High byte of base address
		bus->write(0x0510, 0x20); // Value at $0500 + $10 = $0510

		cpu.tick(CpuCycle{5}); // 5 cycles, no page crossing

		REQUIRE(cpu.get_accumulator() == 0x60); // 0x80 - 0x20 = 0x60
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}
}

TEST_CASE("CPU Compare Instructions - CMP", "[cpu][instructions][compare][CMP]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("CMP Immediate - Equal values") {
		// Set up: CMP #$42 with A = $42
		cpu.set_accumulator(0x42);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC9); // CMP immediate opcode
		bus->write(0x0201, 0x42); // Compare value

		cpu.execute_instruction();

		// Equal: C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true); // A >= memory
		REQUIRE(cpu.get_zero_flag() == true);  // A == memory
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_accumulator() == 0x42); // A unchanged
	}

	SECTION("CMP Immediate - Accumulator greater") {
		// Set up: CMP #$30 with A = $40
		cpu.set_accumulator(0x40);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC9); // CMP immediate opcode
		bus->write(0x0201, 0x30); // Compare value

		cpu.execute_instruction();

		// Greater: C=1, Z=0, N=0 (positive result)
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CMP Immediate - Accumulator less") {
		// Set up: CMP #$50 with A = $30
		cpu.set_accumulator(0x30);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC9); // CMP immediate opcode
		bus->write(0x0201, 0x50); // Compare value

		cpu.execute_instruction();

		// Less: C=0, Z=0, N=1 (negative result)
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("CMP Immediate - Edge case $00 vs $FF") {
		// Set up: CMP #$FF with A = $00
		cpu.set_accumulator(0x00);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC9); // CMP immediate opcode
		bus->write(0x0201, 0xFF); // Compare value

		cpu.execute_instruction();

		// 0x00 - 0xFF = 0x01 (with borrow), so C=0, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CMP Zero Page") {
		// Set up: CMP $80 with value $25 in zero page
		cpu.set_accumulator(0x30);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC5); // CMP zero page opcode
		bus->write(0x0201, 0x80); // Zero page address
		bus->write(0x0080, 0x25); // Value to compare

		cpu.execute_instruction();

		// 0x30 > 0x25, so C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("CMP Zero Page,X") {
		// Set up: CMP $80,X with X=$05
		cpu.set_accumulator(0x20);
		cpu.set_x_register(0x05);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xD5); // CMP zero page,X opcode
		bus->write(0x0201, 0x80); // Base zero page address
		bus->write(0x0085, 0x20); // Value at $80+$05 = $85

		cpu.execute_instruction();

		// Equal values, so C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CMP Absolute") {
		// Set up: CMP $1234
		cpu.set_accumulator(0x40);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xCD); // CMP absolute opcode
		bus->write(0x0201, 0x34); // Low byte of address
		bus->write(0x0202, 0x12); // High byte of address
		bus->write(0x1234, 0x50); // Value to compare

		cpu.execute_instruction();

		// 0x40 < 0x50, so C=0, Z=0, N=1
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("CMP Absolute,X") {
		// Set up: CMP $1200,X with X=$34
		cpu.set_accumulator(0x60);
		cpu.set_x_register(0x34);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xDD); // CMP absolute,X opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address
		bus->write(0x1234, 0x40); // Value at $1200+$34 = $1234

		cpu.execute_instruction();

		// 0x60 > 0x40, so C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CMP Absolute,Y") {
		// Set up: CMP $1200,Y with Y=$44
		cpu.set_accumulator(0x35);
		cpu.set_y_register(0x44);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xD9); // CMP absolute,Y opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address
		bus->write(0x1244, 0x35); // Value at $1200+$44 = $1244

		cpu.execute_instruction();

		// Equal values, so C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CMP (Indirect,X)") {
		// Set up: CMP ($80,X) with X=$04
		cpu.set_accumulator(0x25);
		cpu.set_x_register(0x04);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC1); // CMP (indirect,X) opcode
		bus->write(0x0201, 0x80); // Base pointer address

		// Indirect address at $80+$04 = $84 points to $1500
		bus->write(0x0084, 0x00); // Low byte of target address
		bus->write(0x0085, 0x15); // High byte of target address
		bus->write(0x1500, 0x30); // Value to compare

		cpu.execute_instruction();

		// 0x25 < 0x30, so C=0, Z=0, N=1
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("CMP (Indirect),Y") {
		// Set up: CMP ($90),Y with Y=$10
		cpu.set_accumulator(0x45);
		cpu.set_y_register(0x10);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xD1); // CMP (indirect),Y opcode
		bus->write(0x0201, 0x90); // Pointer address

		// Indirect address at $90 points to $1600, add Y=$10 = $1610
		bus->write(0x0090, 0x00); // Low byte of base address
		bus->write(0x0091, 0x16); // High byte of base address
		bus->write(0x1610, 0x35); // Value to compare at $1600+$10

		cpu.execute_instruction();

		// 0x45 > 0x35, so C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}
}

TEST_CASE("CPU Compare Instructions - CPX", "[cpu][instructions][compare][CPX]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("CPX Immediate - Equal values") {
		// Set up: CPX #$55 with X = $55
		cpu.set_x_register(0x55);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xE0); // CPX immediate opcode
		bus->write(0x0201, 0x55); // Compare value

		cpu.execute_instruction();

		// Equal: C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_x_register() == 0x55); // X unchanged
	}

	SECTION("CPX Immediate - X register greater") {
		// Set up: CPX #$40 with X = $60
		cpu.set_x_register(0x60);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xE0); // CPX immediate opcode
		bus->write(0x0201, 0x40); // Compare value

		cpu.execute_instruction();

		// Greater: C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CPX Immediate - X register less") {
		// Set up: CPX #$80 with X = $50
		cpu.set_x_register(0x50);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xE0); // CPX immediate opcode
		bus->write(0x0201, 0x80); // Compare value

		cpu.execute_instruction();

		// Less: C=0, Z=0, N=1
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("CPX Zero Page") {
		// Set up: CPX $A0 with value $33 in zero page
		cpu.set_x_register(0x33);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xE4); // CPX zero page opcode
		bus->write(0x0201, 0xA0); // Zero page address
		bus->write(0x00A0, 0x33); // Value to compare

		cpu.execute_instruction();

		// Equal values, so C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("CPX Absolute") {
		// Set up: CPX $2000
		cpu.set_x_register(0x70);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xEC); // CPX absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address
		bus->write(0x0202, 0x20); // High byte of address
		bus->write(0x2000, 0x60); // Value to compare

		cpu.execute_instruction();

		// 0x70 > 0x60, so C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}
}

TEST_CASE("CPU Compare Instructions - CPY", "[cpu][instructions][compare][CPY]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());

	SECTION("CPY Immediate - Equal values") {
		// Set up: CPY #$AA with Y = $AA
		cpu.set_y_register(0xAA);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC0); // CPY immediate opcode
		bus->write(0x0201, 0xAA); // Compare value

		cpu.execute_instruction();

		// Equal: C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
		REQUIRE(cpu.get_y_register() == 0xAA); // Y unchanged
	}

	SECTION("CPY Immediate - Y register greater") {
		// Set up: CPY #$80 with Y = $90
		cpu.set_y_register(0x90);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC0); // CPY immediate opcode
		bus->write(0x0201, 0x80); // Compare value

		cpu.execute_instruction();

		// Greater: C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("CPY Immediate - Y register less") {
		// Set up: CPY #$C0 with Y = $A0
		cpu.set_y_register(0xA0);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC0); // CPY immediate opcode
		bus->write(0x0201, 0xC0); // Compare value

		cpu.execute_instruction();

		// Less: C=0, Z=0, N=1
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("CPY Zero Page") {
		// Set up: CPY $B0 with value $77 in zero page
		cpu.set_y_register(0x88);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xC4); // CPY zero page opcode
		bus->write(0x0201, 0xB0); // Zero page address
		bus->write(0x00B0, 0x77); // Value to compare

		cpu.execute_instruction();

		// 0x88 > 0x77, so C=1, Z=0, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("CPY Absolute") {
		// Set up: CPY $1500 (using working RAM address)
		cpu.set_y_register(0x40);
		cpu.set_program_counter(0x0200);
		bus->write(0x0200, 0xCC); // CPY absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address
		bus->write(0x0202, 0x15); // High byte of address
		bus->write(0x1500, 0x40); // Value to compare

		cpu.execute_instruction();

		// Equal values, so C=1, Z=1, N=0
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}
}

TEST_CASE("CPU Logical Instructions - AND", "[cpu][and]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("AND Immediate - Basic operation") {
		cpu.set_accumulator(0xFF);
		bus->write(0x0200, 0x29); // AND immediate opcode
		bus->write(0x0201, 0x0F); // AND with 0x0F

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x0F);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND Immediate - Zero result") {
		cpu.set_accumulator(0xF0);
		bus->write(0x0200, 0x29); // AND immediate opcode
		bus->write(0x0201, 0x0F); // AND with 0x0F

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND Immediate - Negative result") {
		cpu.set_accumulator(0xFF);
		bus->write(0x0200, 0x29); // AND immediate opcode
		bus->write(0x0201, 0x80); // AND with 0x80

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x80);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND Zero Page") {
		cpu.set_accumulator(0xFF);
		bus->write(0x0200, 0x25); // AND zero page opcode
		bus->write(0x0201, 0x80); // Zero page address 0x80
		bus->write(0x0080, 0x55); // Value at zero page 0x80

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x55);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND Zero Page,X") {
		cpu.set_accumulator(0xFF);
		cpu.set_x_register(0x05);
		bus->write(0x0200, 0x35); // AND zero page,X opcode
		bus->write(0x0201, 0x80); // Base zero page address 0x80
		bus->write(0x0085, 0x33); // Value at zero page 0x85 (0x80 + 0x05)

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND Absolute") {
		cpu.set_accumulator(0xFF);
		bus->write(0x0200, 0x2D); // AND absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address
		bus->write(0x0202, 0x15); // High byte of address (0x1500)
		bus->write(0x1500, 0xAA); // Value at absolute address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xAA);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("AND Absolute,X") {
		cpu.set_accumulator(0xFF);
		cpu.set_x_register(0x10);
		bus->write(0x0200, 0x3D); // AND absolute,X opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1510, 0x77); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x77);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("AND Absolute,Y") {
		cpu.set_accumulator(0xFF);
		cpu.set_y_register(0x20);
		bus->write(0x0200, 0x39); // AND absolute,Y opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1520, 0x11); // Value at 0x1500 + 0x20

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x11);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("AND (Indirect,X)") {
		cpu.set_accumulator(0xFF);
		cpu.set_x_register(0x04);
		bus->write(0x0200, 0x21); // AND (zp,X) opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x24 (0x20 + 0x04) points to 0x1500
		bus->write(0x0024, 0x00); // Low byte of target address
		bus->write(0x0025, 0x15); // High byte of target address
		bus->write(0x1500, 0x66); // Value at target address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x66);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("AND (Indirect),Y") {
		cpu.set_accumulator(0xFF);
		cpu.set_y_register(0x10);
		bus->write(0x0200, 0x31); // AND (zp),Y opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x20 points to 0x1500
		bus->write(0x0020, 0x00); // Low byte of base address
		bus->write(0x0021, 0x15); // High byte of base address
		bus->write(0x1510, 0x44); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x44);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}
}

TEST_CASE("CPU Logical Instructions - ORA", "[cpu][ora]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("ORA Immediate - Basic operation") {
		cpu.set_accumulator(0x0F);
		bus->write(0x0200, 0x09); // ORA immediate opcode
		bus->write(0x0201, 0xF0); // OR with 0xF0

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xFF);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA Immediate - Zero result") {
		cpu.set_accumulator(0x00);
		bus->write(0x0200, 0x09); // ORA immediate opcode
		bus->write(0x0201, 0x00); // OR with 0x00

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA Immediate - Setting bits") {
		cpu.set_accumulator(0x55); // 01010101
		bus->write(0x0200, 0x09);  // ORA immediate opcode
		bus->write(0x0201, 0xAA);  // OR with 10101010

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xFF); // Should be 11111111
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA Zero Page") {
		cpu.set_accumulator(0x0F);
		bus->write(0x0200, 0x05); // ORA zero page opcode
		bus->write(0x0201, 0x80); // Zero page address 0x80
		bus->write(0x0080, 0x70); // Value at zero page 0x80

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x7F);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA Zero Page,X") {
		cpu.set_accumulator(0x11);
		cpu.set_x_register(0x05);
		bus->write(0x0200, 0x15); // ORA zero page,X opcode
		bus->write(0x0201, 0x80); // Base zero page address 0x80
		bus->write(0x0085, 0x22); // Value at zero page 0x85 (0x80 + 0x05)

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA Absolute") {
		cpu.set_accumulator(0x0F);
		bus->write(0x0200, 0x0D); // ORA absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address
		bus->write(0x0202, 0x15); // High byte of address (0x1500)
		bus->write(0x1500, 0x80); // Value at absolute address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x8F);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ORA Absolute,X") {
		cpu.set_accumulator(0x01);
		cpu.set_x_register(0x10);
		bus->write(0x0200, 0x1D); // ORA absolute,X opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1510, 0x02); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x03);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ORA Absolute,Y") {
		cpu.set_accumulator(0x10);
		cpu.set_y_register(0x20);
		bus->write(0x0200, 0x19); // ORA absolute,Y opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1520, 0x20); // Value at 0x1500 + 0x20

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x30);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ORA (Indirect,X)") {
		cpu.set_accumulator(0x08);
		cpu.set_x_register(0x04);
		bus->write(0x0200, 0x01); // ORA (zp,X) opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x24 (0x20 + 0x04) points to 0x1500
		bus->write(0x0024, 0x00); // Low byte of target address
		bus->write(0x0025, 0x15); // High byte of target address
		bus->write(0x1500, 0x04); // Value at target address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x0C);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ORA (Indirect),Y") {
		cpu.set_accumulator(0x40);
		cpu.set_y_register(0x10);
		bus->write(0x0200, 0x11); // ORA (zp),Y opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x20 points to 0x1500
		bus->write(0x0020, 0x00); // Low byte of base address
		bus->write(0x0021, 0x15); // High byte of base address
		bus->write(0x1510, 0x80); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xC0);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}
}

TEST_CASE("CPU Logical Instructions - EOR", "[cpu][eor]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("EOR Immediate - Basic operation") {
		cpu.set_accumulator(0xFF);
		bus->write(0x0200, 0x49); // EOR immediate opcode
		bus->write(0x0201, 0x0F); // XOR with 0x0F

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xF0);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR Immediate - Zero result") {
		cpu.set_accumulator(0xAA);
		bus->write(0x0200, 0x49); // EOR immediate opcode
		bus->write(0x0201, 0xAA); // XOR with same value

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR Immediate - Bit flipping") {
		cpu.set_accumulator(0x55); // 01010101
		bus->write(0x0200, 0x49);  // EOR immediate opcode
		bus->write(0x0201, 0xFF);  // XOR with 11111111

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xAA); // Should be 10101010
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR Zero Page") {
		cpu.set_accumulator(0x33);
		bus->write(0x0200, 0x45); // EOR zero page opcode
		bus->write(0x0201, 0x80); // Zero page address 0x80
		bus->write(0x0080, 0x55); // Value at zero page 0x80

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x66);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR Zero Page,X") {
		cpu.set_accumulator(0xFF);
		cpu.set_x_register(0x05);
		bus->write(0x0200, 0x55); // EOR zero page,X opcode
		bus->write(0x0201, 0x80); // Base zero page address 0x80
		bus->write(0x0085, 0x0F); // Value at zero page 0x85 (0x80 + 0x05)

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xF0);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR Absolute") {
		cpu.set_accumulator(0x88);
		bus->write(0x0200, 0x4D); // EOR absolute opcode
		bus->write(0x0201, 0x00); // Low byte of address
		bus->write(0x0202, 0x15); // High byte of address (0x1500)
		bus->write(0x1500, 0x77); // Value at absolute address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xFF);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("EOR Absolute,X") {
		cpu.set_accumulator(0xC0);
		cpu.set_x_register(0x10);
		bus->write(0x0200, 0x5D); // EOR absolute,X opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1510, 0x30); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xF0);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("EOR Absolute,Y") {
		cpu.set_accumulator(0x11);
		cpu.set_y_register(0x20);
		bus->write(0x0200, 0x59); // EOR absolute,Y opcode
		bus->write(0x0201, 0x00); // Low byte of base address
		bus->write(0x0202, 0x15); // High byte of base address (0x1500)
		bus->write(0x1520, 0x22); // Value at 0x1500 + 0x20

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x33);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("EOR (Indirect,X)") {
		cpu.set_accumulator(0x99);
		cpu.set_x_register(0x04);
		bus->write(0x0200, 0x41); // EOR (zp,X) opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x24 (0x20 + 0x04) points to 0x1500
		bus->write(0x0024, 0x00); // Low byte of target address
		bus->write(0x0025, 0x15); // High byte of target address
		bus->write(0x1500, 0x66); // Value at target address

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xFF);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("EOR (Indirect),Y") {
		cpu.set_accumulator(0x0F);
		cpu.set_y_register(0x10);
		bus->write(0x0200, 0x51); // EOR (zp),Y opcode
		bus->write(0x0201, 0x20); // Zero page address 0x20
		// Pointer at 0x20 points to 0x1500
		bus->write(0x0020, 0x00); // Low byte of base address
		bus->write(0x0021, 0x15); // High byte of base address
		bus->write(0x1510, 0xF0); // Value at 0x1500 + 0x10

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xFF);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}
}

TEST_CASE("CPU Shift/Rotate Instructions - ASL", "[cpu][asl]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("ASL Accumulator - Normal shift") {
		cpu.set_accumulator(0x55); // 01010101
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x0A); // ASL A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xAA); // 10101010
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("ASL Accumulator - Carry set") {
		cpu.set_accumulator(0x80); // 10000000
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x0A); // ASL A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("ASL Zero Page") {
		bus->write(0x0050, 0x40); // Value to shift
		bus->write(0x0200, 0x06); // ASL zp opcode
		bus->write(0x0201, 0x50); // Zero page address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0050) == 0x80);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ASL Zero Page,X") {
		cpu.set_x_register(0x05);
		bus->write(0x0055, 0x7F); // Value to shift at 0x50 + 0x05
		bus->write(0x0200, 0x16); // ASL zp,X opcode
		bus->write(0x0201, 0x50); // Zero page base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0055) == 0xFE);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("ASL Absolute") {
		bus->write(0x1234, 0x01); // Value to shift
		bus->write(0x0200, 0x0E); // ASL abs opcode
		bus->write(0x0201, 0x34); // Low byte of address
		bus->write(0x0202, 0x12); // High byte of address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1234) == 0x02);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ASL Absolute,X") {
		cpu.set_x_register(0x10);
		bus->write(0x1244, 0xFF); // Value to shift at 0x1234 + 0x10
		bus->write(0x0200, 0x1E); // ASL abs,X opcode
		bus->write(0x0201, 0x34); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1244) == 0xFE);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

TEST_CASE("CPU Shift/Rotate Instructions - LSR", "[cpu][lsr]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("LSR Accumulator - Normal shift") {
		cpu.set_accumulator(0xAA); // 10101010
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x4A); // LSR A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x55); // 01010101
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("LSR Accumulator - Carry set") {
		cpu.set_accumulator(0x01); // 00000001
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x4A); // LSR A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LSR Zero Page") {
		bus->write(0x0050, 0x80); // Value to shift
		bus->write(0x0200, 0x46); // LSR zp opcode
		bus->write(0x0201, 0x50); // Zero page address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0050) == 0x40);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("LSR Zero Page,X") {
		cpu.set_x_register(0x05);
		bus->write(0x0055, 0xFE); // Value to shift at 0x50 + 0x05
		bus->write(0x0200, 0x56); // LSR zp,X opcode
		bus->write(0x0201, 0x50); // Zero page base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0055) == 0x7F);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("LSR Absolute") {
		bus->write(0x1234, 0x02); // Value to shift
		bus->write(0x0200, 0x4E); // LSR abs opcode
		bus->write(0x0201, 0x34); // Low byte of address
		bus->write(0x0202, 0x12); // High byte of address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1234) == 0x01);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("LSR Absolute,X") {
		cpu.set_x_register(0x10);
		bus->write(0x1244, 0xFF); // Value to shift at 0x1234 + 0x10
		bus->write(0x0200, 0x5E); // LSR abs,X opcode
		bus->write(0x0201, 0x34); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1244) == 0x7F);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}
}

TEST_CASE("CPU Shift/Rotate Instructions - ROL", "[cpu][rol]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("ROL Accumulator - Normal rotate") {
		cpu.set_accumulator(0x55); // 01010101
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x2A); // ROL A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0xAA); // 10101010
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("ROL Accumulator - With carry in") {
		cpu.set_accumulator(0x40); // 01000000
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x2A); // ROL A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x81); // 10000001
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("ROL Accumulator - Carry out") {
		cpu.set_accumulator(0x80); // 10000000
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x2A); // ROL A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("ROL Zero Page") {
		bus->write(0x0050, 0x40); // Value to rotate
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x26); // ROL zp opcode
		bus->write(0x0201, 0x50); // Zero page address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0050) == 0x81);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ROL Zero Page,X") {
		cpu.set_x_register(0x05);
		bus->write(0x0055, 0x7F); // Value to rotate at 0x50 + 0x05
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x36); // ROL zp,X opcode
		bus->write(0x0201, 0x50); // Zero page base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0055) == 0xFE);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("ROL Absolute") {
		bus->write(0x1234, 0xFF); // Value to rotate
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x2E); // ROL abs opcode
		bus->write(0x0201, 0x34); // Low byte of address
		bus->write(0x0202, 0x12); // High byte of address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1234) == 0xFE);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ROL Absolute,X") {
		cpu.set_x_register(0x10);
		bus->write(0x1244, 0x01); // Value to rotate at 0x1234 + 0x10
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x3E); // ROL abs,X opcode
		bus->write(0x0201, 0x34); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1244) == 0x03);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}
}

TEST_CASE("CPU Shift/Rotate Instructions - ROR", "[cpu][ror]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);

	CPU6502 cpu(bus.get());
	cpu.set_program_counter(0x0200);

	SECTION("ROR Accumulator - Normal rotate") {
		cpu.set_accumulator(0xAA); // 10101010
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x6A); // ROR A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x55); // 01010101
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0201);
	}

	SECTION("ROR Accumulator - With carry in") {
		cpu.set_accumulator(0x02); // 00000010
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x6A); // ROR A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x81); // 10000001
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("ROR Accumulator - Carry out") {
		cpu.set_accumulator(0x01); // 00000001
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x6A); // ROR A opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_accumulator() == 0x00);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("ROR Zero Page") {
		bus->write(0x0050, 0x02); // Value to rotate
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x66); // ROR zp opcode
		bus->write(0x0201, 0x50); // Zero page address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0050) == 0x81);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
		REQUIRE(cpu.get_program_counter() == 0x0202);
	}

	SECTION("ROR Zero Page,X") {
		cpu.set_x_register(0x05);
		bus->write(0x0055, 0xFE); // Value to rotate at 0x50 + 0x05
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x76); // ROR zp,X opcode
		bus->write(0x0201, 0x50); // Zero page base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x0055) == 0x7F);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}

	SECTION("ROR Absolute") {
		bus->write(0x1234, 0xFF); // Value to rotate
		cpu.set_carry_flag(false);
		bus->write(0x0200, 0x6E); // ROR abs opcode
		bus->write(0x0201, 0x34); // Low byte of address
		bus->write(0x0202, 0x12); // High byte of address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1234) == 0x7F);
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
		REQUIRE(cpu.get_program_counter() == 0x0203);
	}

	SECTION("ROR Absolute,X") {
		cpu.set_x_register(0x10);
		bus->write(0x1244, 0x80); // Value to rotate at 0x1234 + 0x10
		cpu.set_carry_flag(true);
		bus->write(0x0200, 0x7E); // ROR abs,X opcode
		bus->write(0x0201, 0x34); // Low byte of base address
		bus->write(0x0202, 0x12); // High byte of base address

		cpu.execute_instruction();

		REQUIRE(bus->read(0x1244) == 0xC0);
		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_negative_flag() == true);
	}
}

TEST_CASE("CPU Branch Instructions - Basic Functionality", "[cpu][instructions][branch]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("BPL - Branch if Plus/Positive (N = 0)") {
		cpu.set_program_counter(0x0200);
		cpu.set_negative_flag(false); // N = 0, branch should be taken

		// Write instruction: BPL +10 = 0x10 0x0A
		bus->write(0x0200, 0x10); // BPL opcode
		bus->write(0x0201, 0x0A); // Offset +10

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x020C); // 0x0202 + 0x0A = 0x020C
	}

	SECTION("BPL - No branch when N = 1") {
		cpu.set_program_counter(0x0200);
		cpu.set_negative_flag(true); // N = 1, branch should NOT be taken

		// Write instruction: BPL +10 = 0x10 0x0A
		bus->write(0x0200, 0x10); // BPL opcode
		bus->write(0x0201, 0x0A); // Offset +10

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0202); // No branch, PC advances normally
	}

	SECTION("BMI - Branch if Minus/Negative (N = 1)") {
		cpu.set_program_counter(0x0200);
		cpu.set_negative_flag(true); // N = 1, branch should be taken

		// Write instruction: BMI -5 = 0x30 0xFB
		bus->write(0x0200, 0x30); // BMI opcode
		bus->write(0x0201, 0xFB); // Offset -5 (as signed byte)

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x01FD); // 0x0202 + (-5) = 0x01FD
	}

	SECTION("BVC - Branch if Overflow Clear (V = 0)") {
		cpu.set_program_counter(0x0200);
		cpu.set_overflow_flag(false); // V = 0, branch should be taken

		// Write instruction: BVC +20 = 0x50 0x14
		bus->write(0x0200, 0x50); // BVC opcode
		bus->write(0x0201, 0x14); // Offset +20

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0216); // 0x0202 + 0x14 = 0x0216
	}

	SECTION("BVS - Branch if Overflow Set (V = 1)") {
		cpu.set_program_counter(0x0200);
		cpu.set_overflow_flag(true); // V = 1, branch should be taken

		// Write instruction: BVS +8 = 0x70 0x08
		bus->write(0x0200, 0x70); // BVS opcode
		bus->write(0x0201, 0x08); // Offset +8

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x020A); // 0x0202 + 0x08 = 0x020A
	}

	SECTION("BCC - Branch if Carry Clear (C = 0)") {
		cpu.set_program_counter(0x0200);
		cpu.set_carry_flag(false); // C = 0, branch should be taken

		// Write instruction: BCC +15 = 0x90 0x0F
		bus->write(0x0200, 0x90); // BCC opcode
		bus->write(0x0201, 0x0F); // Offset +15

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0211); // 0x0202 + 0x0F = 0x0211
	}

	SECTION("BCS - Branch if Carry Set (C = 1)") {
		cpu.set_program_counter(0x0200);
		cpu.set_carry_flag(true); // C = 1, branch should be taken

		// Write instruction: BCS -10 = 0xB0 0xF6
		bus->write(0x0200, 0xB0); // BCS opcode
		bus->write(0x0201, 0xF6); // Offset -10 (as signed byte)

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x01F8); // 0x0202 + (-10) = 0x01F8
	}

	SECTION("BNE - Branch if Not Equal/Zero Clear (Z = 0)") {
		cpu.set_program_counter(0x0200);
		cpu.set_zero_flag(false); // Z = 0, branch should be taken

		// Write instruction: BNE +25 = 0xD0 0x19
		bus->write(0x0200, 0xD0); // BNE opcode
		bus->write(0x0201, 0x19); // Offset +25

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x021B); // 0x0202 + 0x19 = 0x021B
	}

	SECTION("BEQ - Branch if Equal/Zero Set (Z = 1)") {
		cpu.set_program_counter(0x0200);
		cpu.set_zero_flag(true); // Z = 1, branch should be taken

		// Write instruction: BEQ +30 = 0xF0 0x1E
		bus->write(0x0200, 0xF0); // BEQ opcode
		bus->write(0x0201, 0x1E); // Offset +30

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0220); // 0x0202 + 0x1E = 0x0220
	}
}

TEST_CASE("CPU Branch Instructions - Page Boundary Crossing", "[cpu][instructions][branch][timing]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("BPL - Same page branch (3 cycles)") {
		cpu.set_program_counter(0x0280); // Start in middle of page
		cpu.set_negative_flag(false);	 // Branch will be taken

		// Write instruction: BPL +10 = 0x10 0x0A
		bus->write(0x0280, 0x10); // BPL opcode
		bus->write(0x0281, 0x0A); // Offset +10

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x028C); // 0x0282 + 0x0A = 0x028C (same page)
													  // Branch taken, same page = 3 cycles total
	}

	SECTION("BPL - Cross page boundary forward (4 cycles)") {
		cpu.set_program_counter(0x02F0); // Near end of page
		cpu.set_negative_flag(false);	 // Branch will be taken

		// Write instruction: BPL +20 = 0x10 0x14
		bus->write(0x02F0, 0x10); // BPL opcode
		bus->write(0x02F1, 0x14); // Offset +20

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0306); // 0x02F2 + 0x14 = 0x0306 (crosses page)
													  // Branch taken, page boundary crossed = 4 cycles total
	}

	SECTION("BMI - Cross page boundary backward (4 cycles)") {
		cpu.set_program_counter(0x0310); // Start of page
		cpu.set_negative_flag(true);	 // Branch will be taken

		// Write instruction: BMI -20 = 0x30 0xEC
		bus->write(0x0310, 0x30); // BMI opcode
		bus->write(0x0311, 0xEC); // Offset -20 (as signed byte)

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x02FE); // 0x0312 + (-20) = 0x02FE (crosses page)
													  // Branch taken, page boundary crossed = 4 cycles total
	}

	SECTION("BEQ - No branch (2 cycles)") {
		cpu.set_program_counter(0x0200);
		cpu.set_zero_flag(false); // Z = 0, branch should NOT be taken

		// Write instruction: BEQ +50 = 0xF0 0x32
		bus->write(0x0200, 0xF0); // BEQ opcode
		bus->write(0x0201, 0x32); // Offset +50

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0202); // No branch, PC advances normally
													  // Branch not taken = 2 cycles total
	}
}

TEST_CASE("CPU Branch Instructions - Edge Cases", "[cpu][instructions][branch][edge]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("Branch with zero offset") {
		cpu.set_program_counter(0x0200);
		cpu.set_zero_flag(true); // Branch will be taken

		// Write instruction: BEQ +0 = 0xF0 0x00
		bus->write(0x0200, 0xF0); // BEQ opcode
		bus->write(0x0201, 0x00); // Offset 0

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0202); // 0x0202 + 0 = 0x0202
	}

	SECTION("Branch with maximum forward offset (+127)") {
		cpu.set_program_counter(0x0200);
		cpu.set_carry_flag(false); // Branch will be taken

		// Write instruction: BCC +127 = 0x90 0x7F
		bus->write(0x0200, 0x90); // BCC opcode
		bus->write(0x0201, 0x7F); // Offset +127

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0281); // 0x0202 + 127 = 0x0281
	}

	SECTION("Branch with maximum backward offset (-128)") {
		cpu.set_program_counter(0x0300);
		cpu.set_carry_flag(true); // Branch will be taken

		// Write instruction: BCS -128 = 0xB0 0x80
		bus->write(0x0300, 0xB0); // BCS opcode
		bus->write(0x0301, 0x80); // Offset -128 (as signed byte)

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0282); // 0x0302 + (-128) = 0x0282
	}

	SECTION("Branch across multiple page boundaries") {
		cpu.set_program_counter(0x01F0); // Near page boundary
		cpu.set_overflow_flag(false);	 // Branch will be taken

		// Write instruction: BVC +32 = 0x50 0x20
		bus->write(0x01F0, 0x50); // BVC opcode
		bus->write(0x01F1, 0x20); // Offset +32

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0212); // 0x01F2 + 32 = 0x0212 (crosses page)
	}
}

TEST_CASE("CPU Branch Instructions - All Opcodes", "[cpu][instructions][branch][opcodes]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("All branch opcodes with correct conditions") {
		struct BranchTest {
			Byte opcode;
			const char *name;
			void (CPU6502::*set_flag)(bool);
			bool flag_value;
			SignedByte offset;
		};

		std::vector<BranchTest> tests = {{0x10, "BPL", &CPU6502::set_negative_flag, false, 10},
										 {0x30, "BMI", &CPU6502::set_negative_flag, true, -5},
										 {0x50, "BVC", &CPU6502::set_overflow_flag, false, 15},
										 {0x70, "BVS", &CPU6502::set_overflow_flag, true, -10},
										 {0x90, "BCC", &CPU6502::set_carry_flag, false, 8},
										 {0xB0, "BCS", &CPU6502::set_carry_flag, true, 12},
										 {0xD0, "BNE", &CPU6502::set_zero_flag, false, -15},
										 {0xF0, "BEQ", &CPU6502::set_zero_flag, true, 20}};

		for (const auto &test : tests) {
			// Reset CPU state
			cpu.set_program_counter(0x0200);
			cpu.set_carry_flag(false);
			cpu.set_zero_flag(false);
			cpu.set_interrupt_flag(false);
			cpu.set_decimal_flag(false);
			cpu.set_break_flag(false);
			cpu.set_overflow_flag(false);
			cpu.set_negative_flag(false);

			// Set the specific flag for this test
			(cpu.*test.set_flag)(test.flag_value);

			// Write instruction
			bus->write(0x0200, test.opcode);
			bus->write(0x0201, static_cast<Byte>(test.offset));

			// Execute and verify
			cpu.execute_instruction();

			Address expected_pc = static_cast<Address>(0x0202 + test.offset);
			REQUIRE(cpu.get_program_counter() == expected_pc);
		}
	}
}

TEST_CASE("CPU Jump Instructions - JMP", "[cpu][instructions][jump]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("JMP Absolute") {
		cpu.set_program_counter(0x0200);

		// Write instruction: JMP $1234 = 0x4C 0x34 0x12
		bus->write(0x0200, 0x4C); // JMP absolute opcode
		bus->write(0x0201, 0x34); // Low byte of target address
		bus->write(0x0202, 0x12); // High byte of target address

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x1234);
	}

	SECTION("JMP Indirect - Normal case") {
		cpu.set_program_counter(0x0200);

		// Write instruction: JMP ($1000) = 0x6C 0x00 0x10
		bus->write(0x0200, 0x6C); // JMP indirect opcode
		bus->write(0x0201, 0x00); // Low byte of indirect address
		bus->write(0x0202, 0x10); // High byte of indirect address

		// Store target address at $1000-$1001
		bus->write(0x1000, 0x56); // Low byte of target
		bus->write(0x1001, 0x78); // High byte of target

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x7856);
	}

	SECTION("JMP Indirect - Page boundary bug") {
		cpu.set_program_counter(0x0200);

		// Write instruction: JMP ($10FF) = 0x6C 0xFF 0x10
		bus->write(0x0200, 0x6C); // JMP indirect opcode
		bus->write(0x0201, 0xFF); // Low byte of indirect address (page boundary)
		bus->write(0x0202, 0x10); // High byte of indirect address

		// Store target address with page boundary bug
		bus->write(0x10FF, 0x34); // Low byte of target
		bus->write(0x1100, 0xAB); // This should be high byte but won't be read due to bug
		bus->write(0x1000, 0x56); // This will be read instead (wraps to start of page)

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x5634); // 0x56 from $1000, 0x34 from $10FF
	}
}

TEST_CASE("CPU Subroutine Instructions - JSR/RTS", "[cpu][instructions][subroutine]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("JSR - Jump to Subroutine") {
		cpu.set_program_counter(0x0200);
		cpu.set_stack_pointer(0xFF); // Start with full stack

		// Write instruction: JSR $1500 = 0x20 0x00 0x15
		bus->write(0x0200, 0x20); // JSR opcode
		bus->write(0x0201, 0x00); // Low byte of subroutine address
		bus->write(0x0202, 0x15); // High byte of subroutine address

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x1500);
		REQUIRE(cpu.get_stack_pointer() == 0xFD); // Stack pointer decremented by 2

		// Check that return address (0x0202) was pushed to stack
		REQUIRE(bus->read(0x01FF) == 0x02); // High byte of return address
		REQUIRE(bus->read(0x01FE) == 0x02); // Low byte of return address
	}

	SECTION("RTS - Return from Subroutine") {
		cpu.set_program_counter(0x1500);
		cpu.set_stack_pointer(0xFD); // Stack as if JSR was called

		// Set up stack with return address (should return to 0x0203)
		bus->write(0x01FE, 0x02); // Low byte of return address
		bus->write(0x01FF, 0x02); // High byte of return address

		// Write instruction: RTS = 0x60
		bus->write(0x1500, 0x60); // RTS opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x0203); // Return address + 1
		REQUIRE(cpu.get_stack_pointer() == 0xFF);	  // Stack pointer restored
	}

	SECTION("JSR/RTS - Complete subroutine call sequence") {
		cpu.set_program_counter(0x0200);
		cpu.set_stack_pointer(0xFF);

		// Main program: JSR $1500
		bus->write(0x0200, 0x20); // JSR opcode
		bus->write(0x0201, 0x00); // Low byte
		bus->write(0x0202, 0x15); // High byte

		// Subroutine: RTS
		bus->write(0x1500, 0x60); // RTS opcode

		// Execute JSR
		cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x1500);
		REQUIRE(cpu.get_stack_pointer() == 0xFD);

		// Execute RTS
		cpu.execute_instruction();
		REQUIRE(cpu.get_program_counter() == 0x0203); // Next instruction after JSR
		REQUIRE(cpu.get_stack_pointer() == 0xFF);
	}
}

TEST_CASE("CPU Interrupt Instructions - RTI", "[cpu][instructions][interrupt]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("RTI - Return from Interrupt") {
		cpu.set_program_counter(0x8000); // Interrupt handler
		cpu.set_stack_pointer(0xFC);	 // Stack as if interrupt occurred

		// Set up stack with saved state (status register and return address)
		bus->write(0x01FD, 0b11010101); // Saved status register
		bus->write(0x01FE, 0x34);		// Low byte of return address
		bus->write(0x01FF, 0x12);		// High byte of return address

		// Write instruction: RTI = 0x40
		bus->write(0x8000, 0x40); // RTI opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_program_counter() == 0x1234); // Return address restored
		REQUIRE(cpu.get_stack_pointer() == 0xFF);	  // Stack pointer restored

		// Check that status register was restored (with break flag cleared, unused set)
		REQUIRE(cpu.get_carry_flag() == true);
		REQUIRE(cpu.get_zero_flag() == false);
		REQUIRE(cpu.get_interrupt_flag() == true);
		REQUIRE(cpu.get_decimal_flag() == false);
		REQUIRE(cpu.get_break_flag() == false); // Should be cleared by RTI
		REQUIRE(cpu.get_overflow_flag() == true);
		REQUIRE(cpu.get_negative_flag() == true);
	}

	SECTION("RTI - Status register flag handling") {
		cpu.set_program_counter(0x8000);
		cpu.set_stack_pointer(0xFC);

		// Test with different status register values
		bus->write(0x01FD, 0b00101010); // Different flag pattern
		bus->write(0x01FE, 0x00);		// Return address low
		bus->write(0x01FF, 0x30);		// Return address high

		bus->write(0x8000, 0x40); // RTI opcode

		cpu.execute_instruction();

		REQUIRE(cpu.get_carry_flag() == false);
		REQUIRE(cpu.get_zero_flag() == true);
		REQUIRE(cpu.get_interrupt_flag() == false);
		REQUIRE(cpu.get_decimal_flag() == true);
		REQUIRE(cpu.get_break_flag() == false); // Always cleared by RTI
		REQUIRE(cpu.get_overflow_flag() == false);
		REQUIRE(cpu.get_negative_flag() == false);
	}
}

TEST_CASE("CPU Jump/Subroutine Instructions - All Opcodes", "[cpu][instructions][jump][opcodes]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("All jump/subroutine opcodes verification") {
		struct JumpTest {
			Byte opcode;
			const char *name;
			std::function<void(CPU6502 &, SystemBus *)> setup;
			std::function<void(const CPU6502 &)> verify;
		};

		std::vector<JumpTest> tests = {{0x4C, "JMP Absolute",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											bus->write(0x0200, 0x4C);
											bus->write(0x0201, 0x00);
											bus->write(0x0202, 0x30);
										},
										[](const CPU6502 &cpu) { REQUIRE(cpu.get_program_counter() == 0x3000); }},

									   {0x6C, "JMP Indirect",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											bus->write(0x0200, 0x6C);
											bus->write(0x0201, 0x00);
											bus->write(0x0202, 0x10); // Changed from 0x20 to 0x10
											bus->write(0x1000, 0x00); // Changed from 0x2000 to 0x1000
											bus->write(0x1001, 0x40); // Changed from 0x2001 to 0x1001
										},
										[](const CPU6502 &cpu) { REQUIRE(cpu.get_program_counter() == 0x4000); }},

									   {0x20, "JSR",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_stack_pointer(0xFF);
											bus->write(0x0200, 0x20);
											bus->write(0x0201, 0x00);
											bus->write(0x0202, 0x50);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_program_counter() == 0x5000);
											REQUIRE(cpu.get_stack_pointer() == 0xFD);
										}},

									   {0x60, "RTS",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x5000);
											cpu.set_stack_pointer(0xFD);
											bus->write(0x01FE, 0x02);
											bus->write(0x01FF, 0x02);
											bus->write(0x5000, 0x60);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_program_counter() == 0x0203);
											REQUIRE(cpu.get_stack_pointer() == 0xFF);
										}},

									   {0x40, "RTI",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x8000);
											cpu.set_stack_pointer(0xFC);
											bus->write(0x01FD, 0b10000001);
											bus->write(0x01FE, 0x00);
											bus->write(0x01FF, 0x60);
											bus->write(0x8000, 0x40);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_program_counter() == 0x6000);
											REQUIRE(cpu.get_stack_pointer() == 0xFF);
											REQUIRE(cpu.get_carry_flag() == true);
											REQUIRE(cpu.get_negative_flag() == true);
										}}};

		for (const auto &test : tests) {
			// Setup test
			test.setup(cpu, bus.get());

			// Execute instruction
			cpu.execute_instruction();

			// Verify results
			test.verify(cpu);
		}
	}
}

TEST_CASE("CPU Stack Operations - All Opcodes", "[cpu][instructions][stack][opcodes]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("All stack opcodes verification") {
		struct StackTest {
			Byte opcode;
			std::string name;
			std::function<void(CPU6502 &, SystemBus *)> setup;
			std::function<void(const CPU6502 &)> verify;
		};

		std::vector<StackTest> tests = {{0x48, "PHA",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_accumulator(0x42);
											 cpu.set_stack_pointer(0xFF);
											 bus->write(0x0200, 0x48);
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_accumulator() == 0x42);   // Accumulator unchanged
											 REQUIRE(cpu.get_stack_pointer() == 0xFE); // Stack pointer decremented
											 // Note: Can't easily verify stack contents without additional CPU
											 // interface
										 }},

										{0x68, "PLA",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_accumulator(0x00);
											 cpu.set_stack_pointer(0xFE); // Stack pointer as if something was pushed
											 bus->write(0x0200, 0x68);
											 bus->write(0x01FF, 0x42); // Put value on stack
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_accumulator() == 0x42);   // Accumulator loaded from stack
											 REQUIRE(cpu.get_stack_pointer() == 0xFF); // Stack pointer incremented
											 REQUIRE(cpu.get_zero_flag() == false);	   // N=0, Z=0 for 0x42
											 REQUIRE(cpu.get_negative_flag() == false);
										 }},

										{0x68, "PLA Zero Flag",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_accumulator(0xFF);
											 cpu.set_stack_pointer(0xFE);
											 bus->write(0x0200, 0x68);
											 bus->write(0x01FF, 0x00); // Put zero on stack
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_accumulator() == 0x00);
											 REQUIRE(cpu.get_zero_flag() == true); // Z=1 for zero
											 REQUIRE(cpu.get_negative_flag() == false);
										 }},

										{0x68, "PLA Negative Flag",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_accumulator(0x00);
											 cpu.set_stack_pointer(0xFE);
											 bus->write(0x0200, 0x68);
											 bus->write(0x01FF, 0x80); // Put negative value on stack
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_accumulator() == 0x80);
											 REQUIRE(cpu.get_zero_flag() == false);
											 REQUIRE(cpu.get_negative_flag() == true); // N=1 for 0x80
										 }},

										{0x08, "PHP",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_stack_pointer(0xFF);
											 // Set some flags for testing
											 cpu.set_carry_flag(true);
											 cpu.set_zero_flag(true);
											 cpu.set_interrupt_flag(true);
											 bus->write(0x0200, 0x08);
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_stack_pointer() == 0xFE); // Stack pointer decremented
											 // Flags should remain unchanged
											 REQUIRE(cpu.get_carry_flag() == true);
											 REQUIRE(cpu.get_zero_flag() == true);
											 REQUIRE(cpu.get_interrupt_flag() == true);
										 }},

										{0x28, "PLP",
										 [](CPU6502 &cpu, SystemBus *bus) {
											 cpu.set_program_counter(0x0200);
											 cpu.set_stack_pointer(0xFE);
											 // Clear all flags initially
											 cpu.set_carry_flag(false);
											 cpu.set_zero_flag(false);
											 cpu.set_interrupt_flag(false);
											 cpu.set_decimal_flag(false);
											 cpu.set_overflow_flag(false);
											 cpu.set_negative_flag(false);
											 bus->write(0x0200, 0x28);
											 // Put status with some flags set on stack (C=1, Z=1, I=1)
											 bus->write(0x01FF, 0x27); // 00100111 (unused bit always set)
										 },
										 [](const CPU6502 &cpu) {
											 REQUIRE(cpu.get_stack_pointer() == 0xFF); // Stack pointer incremented
											 // Flags should be restored from stack
											 REQUIRE(cpu.get_carry_flag() == true);
											 REQUIRE(cpu.get_zero_flag() == true);
											 REQUIRE(cpu.get_interrupt_flag() == true);
											 REQUIRE(cpu.get_decimal_flag() == false);
											 REQUIRE(cpu.get_overflow_flag() == false);
											 REQUIRE(cpu.get_negative_flag() == false);
										 }}};

		for (const auto &test : tests) {
			DYNAMIC_SECTION("Testing " << test.name << " (0x" << std::hex << (int)test.opcode << ")") {
				// Reset CPU state
				cpu.reset();

				// Set up test
				test.setup(cpu, bus.get());

				// Execute instruction
				cpu.execute_instruction();

				// Verify results
				test.verify(cpu);
			}
		}
	}
}

TEST_CASE("CPU Status Flag Instructions - All Opcodes", "[cpu][instructions][flags][opcodes]") {
	auto bus = std::make_unique<SystemBus>();
	auto ram = std::make_shared<Ram>();
	bus->connect_ram(ram);
	CPU6502 cpu(bus.get());

	SECTION("All status flag opcodes verification") {
		struct FlagTest {
			Byte opcode;
			std::string name;
			std::function<void(CPU6502 &, SystemBus *)> setup;
			std::function<void(const CPU6502 &)> verify;
		};

		std::vector<FlagTest> tests = {{0x18, "CLC",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_carry_flag(true); // Set carry flag initially
											bus->write(0x0200, 0x18);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_carry_flag() == false); // Should be cleared
										}},

									   {0x38, "SEC",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_carry_flag(false); // Clear carry flag initially
											bus->write(0x0200, 0x38);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_carry_flag() == true); // Should be set
										}},

									   {0x58, "CLI",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_interrupt_flag(true); // Set interrupt flag initially
											bus->write(0x0200, 0x58);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_interrupt_flag() == false); // Should be cleared
										}},

									   {0x78, "SEI",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_interrupt_flag(false); // Clear interrupt flag initially
											bus->write(0x0200, 0x78);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_interrupt_flag() == true); // Should be set
										}},

									   {0xB8, "CLV",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_overflow_flag(true); // Set overflow flag initially
											bus->write(0x0200, 0xB8);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_overflow_flag() == false); // Should be cleared
										}},

									   {0xD8, "CLD",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_decimal_flag(true); // Set decimal flag initially
											bus->write(0x0200, 0xD8);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_decimal_flag() == false); // Should be cleared
										}},

									   {0xF8, "SED",
										[](CPU6502 &cpu, SystemBus *bus) {
											cpu.set_program_counter(0x0200);
											cpu.set_decimal_flag(false); // Clear decimal flag initially
											bus->write(0x0200, 0xF8);
										},
										[](const CPU6502 &cpu) {
											REQUIRE(cpu.get_decimal_flag() == true); // Should be set
										}}};

		for (const auto &test : tests) {
			DYNAMIC_SECTION("Testing " << test.name << " (0x" << std::hex << (int)test.opcode << ")") {
				// Reset CPU state
				cpu.reset();

				// Set up test
				test.setup(cpu, bus.get());

				// Store initial state of other flags to ensure they're not affected
				bool initial_zero = cpu.get_zero_flag();
				bool initial_negative = cpu.get_negative_flag();

				// Execute instruction
				cpu.execute_instruction();

				// Verify target flag changed
				test.verify(cpu);

				// Verify other flags are unchanged
				REQUIRE(cpu.get_zero_flag() == initial_zero);
				REQUIRE(cpu.get_negative_flag() == initial_negative);
			}
		}
	}

	SECTION("Flag independence verification") {
		// Test that flag instructions don't affect other flags
		cpu.reset();
		cpu.set_program_counter(0x0200);

		// Set all flags to a known state
		cpu.set_carry_flag(true);
		cpu.set_zero_flag(true);
		cpu.set_interrupt_flag(true);
		cpu.set_decimal_flag(true);
		cpu.set_overflow_flag(true);
		cpu.set_negative_flag(true);

		// Test CLC doesn't affect other flags
		bus->write(0x0200, 0x18); // CLC
		cpu.execute_instruction();

		REQUIRE(cpu.get_carry_flag() == false);	   // Changed
		REQUIRE(cpu.get_zero_flag() == true);	   // Unchanged
		REQUIRE(cpu.get_interrupt_flag() == true); // Unchanged
		REQUIRE(cpu.get_decimal_flag() == true);   // Unchanged
		REQUIRE(cpu.get_overflow_flag() == true);  // Unchanged
		REQUIRE(cpu.get_negative_flag() == true);  // Unchanged
	}
}
