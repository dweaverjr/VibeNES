// VibeNES - NES Emulator
// Undocumented Opcodes Tests
// Tests for the stable undocumented 6502 opcodes implementation

#include "../../include/core/bus.hpp"
#include "../../include/cpu/cpu_6502.hpp"
#include "../../include/memory/ram.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <memory>

using namespace nes;

class UndocumentedOpcodesTestFixture {
  public:
	std::unique_ptr<SystemBus> bus;
	std::shared_ptr<Ram> ram;
	std::unique_ptr<CPU6502> cpu;

	UndocumentedOpcodesTestFixture() {
		bus = std::make_unique<SystemBus>();
		ram = std::make_shared<Ram>();
		bus->connect_ram(ram);
		cpu = std::make_unique<CPU6502>(bus.get());

		// Set up CPU in a known state
		cpu->set_program_counter(0x0200);
		cpu->set_accumulator(0x00);
		cpu->set_x_register(0x00);
		cpu->set_y_register(0x00);
		// Status register is initialized properly by constructor
	}

	void setup_memory(Address addr, std::initializer_list<Byte> bytes) {
		Address current = addr;
		for (Byte byte : bytes) {
			bus->write(current++, byte);
		}
	}
};

TEST_CASE("LAX - Load A and X", "[cpu][undocumented][lax]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("LAX Zero Page - Basic functionality") {
		// LAX $10 (Load value at $10 into both A and X)
		fixture.bus->write(0x10, 0x42);
		fixture.setup_memory(0x0200, {0xA7, 0x10}); // LAX zp

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x42);
		REQUIRE(fixture.cpu->get_x_register() == 0x42);
		REQUIRE(fixture.cpu->get_program_counter() == 0x0202);
		REQUIRE(fixture.cpu->get_zero_flag() == false);
		REQUIRE(fixture.cpu->get_negative_flag() == false);
	}

	SECTION("LAX Zero Page,Y") {
		// LAX $10,Y with Y=5, so loads from $15
		fixture.cpu->set_y_register(0x05);
		fixture.bus->write(0x15, 0x80);				// Negative value
		fixture.setup_memory(0x0200, {0xB7, 0x10}); // LAX zp,Y

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x80);
		REQUIRE(fixture.cpu->get_x_register() == 0x80);
		REQUIRE(fixture.cpu->get_negative_flag() == true);
		REQUIRE(fixture.cpu->get_zero_flag() == false);
	}

	SECTION("LAX Absolute") {
		fixture.bus->write(0x1234, 0x00);				  // Zero value
		fixture.setup_memory(0x0200, {0xAF, 0x34, 0x12}); // LAX abs

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x00);
		REQUIRE(fixture.cpu->get_x_register() == 0x00);
		REQUIRE(fixture.cpu->get_zero_flag() == true);
		REQUIRE(fixture.cpu->get_negative_flag() == false);
	}

	SECTION("LAX Absolute,Y") {
		fixture.cpu->set_y_register(0x02);
		fixture.bus->write(0x1236, 0x7F);				  // Positive value
		fixture.setup_memory(0x0200, {0xBF, 0x34, 0x12}); // LAX abs,Y

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x7F);
		REQUIRE(fixture.cpu->get_x_register() == 0x7F);
		REQUIRE(fixture.cpu->get_zero_flag() == false);
		REQUIRE(fixture.cpu->get_negative_flag() == false);
	}

	SECTION("LAX (Indirect,X)") {
		fixture.cpu->set_x_register(0x04);
		// Set up indirect address at $14 -> $1500
		fixture.bus->write(0x14, 0x00);
		fixture.bus->write(0x15, 0x15);
		fixture.bus->write(0x1500, 0x33);
		fixture.setup_memory(0x0200, {0xA3, 0x10}); // LAX (zp,X)

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x33);
		REQUIRE(fixture.cpu->get_x_register() == 0x33);
	}

	SECTION("LAX (Indirect),Y") {
		fixture.cpu->set_y_register(0x03);
		// Set up indirect address at $10 -> $1200, then add Y to get $1203
		fixture.bus->write(0x10, 0x00);
		fixture.bus->write(0x11, 0x12);
		fixture.bus->write(0x1203, 0x99);
		fixture.setup_memory(0x0200, {0xB3, 0x10}); // LAX (zp),Y

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.cpu->get_accumulator() == 0x99);
		REQUIRE(fixture.cpu->get_x_register() == 0x99);
	}
}

TEST_CASE("SAX - Store A AND X", "[cpu][undocumented][sax]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("SAX Zero Page - Basic functionality") {
		fixture.cpu->set_accumulator(0xF0);
		fixture.cpu->set_x_register(0x0F);
		fixture.setup_memory(0x0200, {0x87, 0x10}); // SAX zp

		(void)fixture.cpu->execute_instruction();

		// A AND X = 0xF0 AND 0x0F = 0x00
		REQUIRE(fixture.bus->read(0x10) == 0x00);
		REQUIRE(fixture.cpu->get_program_counter() == 0x0202);
	}

	SECTION("SAX Zero Page,Y") {
		fixture.cpu->set_accumulator(0xFF);
		fixture.cpu->set_x_register(0x33);
		fixture.cpu->set_y_register(0x05);
		fixture.setup_memory(0x0200, {0x97, 0x10}); // SAX zp,Y

		(void)fixture.cpu->execute_instruction();

		// A AND X = 0xFF AND 0x33 = 0x33, stored at $10+Y = $15
		REQUIRE(fixture.bus->read(0x15) == 0x33);
	}

	SECTION("SAX Absolute") {
		fixture.cpu->set_accumulator(0x81);
		fixture.cpu->set_x_register(0x42);
		fixture.setup_memory(0x0200, {0x8F, 0x34, 0x12}); // SAX abs

		(void)fixture.cpu->execute_instruction();

		// A AND X = 0x81 AND 0x42 = 0x00
		REQUIRE(fixture.bus->read(0x1234) == 0x00);
	}

	SECTION("SAX (Indirect,X)") {
		fixture.cpu->set_accumulator(0xAA);
		fixture.cpu->set_x_register(0x55);
		fixture.cpu->set_x_register(0x04); // For addressing
		// Set up indirect address at $14 -> $1500
		fixture.bus->write(0x14, 0x00);
		fixture.bus->write(0x15, 0x15);
		fixture.setup_memory(0x0200, {0x83, 0x10}); // SAX (zp,X)

		fixture.cpu->set_accumulator(0xAA);
		fixture.cpu->set_x_register(0x55);
		(void)fixture.cpu->execute_instruction();

		// A AND X = 0xAA AND 0x55 = 0x00
		REQUIRE(fixture.bus->read(0x1500) == 0x00);
	}
}

TEST_CASE("DCP - Decrement and Compare", "[cpu][undocumented][dcp]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("DCP Zero Page - Basic functionality") {
		fixture.cpu->set_accumulator(0x05);
		fixture.bus->write(0x10, 0x08);
		fixture.setup_memory(0x0200, {0xC7, 0x10}); // DCP zp

		(void)fixture.cpu->execute_instruction();

		// Memory decremented: 0x08 -> 0x07
		// Compare A (0x05) with decremented value (0x07)
		// 0x05 - 0x07 = 0xFE (underflow)
		REQUIRE(fixture.bus->read(0x10) == 0x07);
		REQUIRE(fixture.cpu->get_carry_flag() == false); // A < memory
		REQUIRE(fixture.cpu->get_zero_flag() == false);
		REQUIRE(fixture.cpu->get_negative_flag() == true); // Result is negative
	}

	SECTION("DCP Zero Page,X - Equal values") {
		fixture.cpu->set_accumulator(0x10);
		fixture.cpu->set_x_register(0x05);
		fixture.bus->write(0x15, 0x11);				// Will be decremented to 0x10
		fixture.setup_memory(0x0200, {0xD7, 0x10}); // DCP zp,X

		(void)fixture.cpu->execute_instruction();

		REQUIRE(fixture.bus->read(0x15) == 0x10);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // A >= memory
		REQUIRE(fixture.cpu->get_zero_flag() == true);	// A == memory
		REQUIRE(fixture.cpu->get_negative_flag() == false);
	}

	SECTION("DCP Absolute") {
		fixture.cpu->set_accumulator(0x20);
		fixture.bus->write(0x1234, 0x15);
		fixture.setup_memory(0x0200, {0xCF, 0x34, 0x12}); // DCP abs

		(void)fixture.cpu->execute_instruction();

		// Memory: 0x15 -> 0x14, compare with A (0x20)
		// 0x20 > 0x14, so carry set, zero clear, negative clear
		REQUIRE(fixture.bus->read(0x1234) == 0x14);
		REQUIRE(fixture.cpu->get_carry_flag() == true);
		REQUIRE(fixture.cpu->get_zero_flag() == false);
		REQUIRE(fixture.cpu->get_negative_flag() == false);
	}
}

TEST_CASE("ISC - Increment and Subtract with Carry", "[cpu][undocumented][isc]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("ISC Zero Page - Basic functionality") {
		fixture.cpu->set_accumulator(0x50);
		fixture.cpu->set_carry_flag(true); // Set carry for subtraction
		fixture.bus->write(0x10, 0x0F);
		fixture.setup_memory(0x0200, {0xE7, 0x10}); // ISC zp

		(void)fixture.cpu->execute_instruction();

		// Memory incremented: 0x0F -> 0x10
		// SBC: A = 0x50 - 0x10 - (1 - carry) = 0x50 - 0x10 - 0 = 0x40
		REQUIRE(fixture.bus->read(0x10) == 0x10);
		REQUIRE(fixture.cpu->get_accumulator() == 0x40);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // No borrow
	}

	SECTION("ISC with borrow") {
		fixture.cpu->set_accumulator(0x05);
		fixture.cpu->set_carry_flag(false); // Clear carry (will cause extra borrow)
		fixture.bus->write(0x10, 0x09);
		fixture.setup_memory(0x0200, {0xE7, 0x10}); // ISC zp

		(void)fixture.cpu->execute_instruction();

		// Memory incremented: 0x09 -> 0x0A
		// SBC: A = 0x05 - 0x0A - (1 - carry) = 0x05 - 0x0A - 1 = 0xFA
		REQUIRE(fixture.bus->read(0x10) == 0x0A);
		REQUIRE(fixture.cpu->get_accumulator() == 0xFA);
		REQUIRE(fixture.cpu->get_carry_flag() == false); // Borrow occurred
		REQUIRE(fixture.cpu->get_negative_flag() == true);
	}
}

TEST_CASE("SLO - Shift Left and OR", "[cpu][undocumented][slo]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("SLO Zero Page") {
		fixture.cpu->set_accumulator(0x0F);
		fixture.bus->write(0x10, 0x81);				// Will become 0x02 after shift
		fixture.setup_memory(0x0200, {0x07, 0x10}); // SLO zp

		(void)fixture.cpu->execute_instruction();

		// Memory shifted: 0x81 -> 0x02, carry set from bit 7
		// A = A OR shifted = 0x0F OR 0x02 = 0x0F
		REQUIRE(fixture.bus->read(0x10) == 0x02);
		REQUIRE(fixture.cpu->get_accumulator() == 0x0F);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // Bit 7 was set
	}

	SECTION("SLO Zero Page,X") {
		fixture.cpu->set_accumulator(0x30);
		fixture.cpu->set_x_register(0x05);
		fixture.bus->write(0x15, 0x44);				// Will become 0x88 after shift
		fixture.setup_memory(0x0200, {0x17, 0x10}); // SLO zp,X

		(void)fixture.cpu->execute_instruction();

		// Memory shifted: 0x44 -> 0x88
		// A = A OR shifted = 0x30 OR 0x88 = 0xB8
		REQUIRE(fixture.bus->read(0x15) == 0x88);
		REQUIRE(fixture.cpu->get_accumulator() == 0xB8);
		REQUIRE(fixture.cpu->get_carry_flag() == false);   // Bit 7 was clear
		REQUIRE(fixture.cpu->get_negative_flag() == true); // Result is negative
	}
}

TEST_CASE("RLA - Rotate Left and AND", "[cpu][undocumented][rla]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("RLA Zero Page") {
		fixture.cpu->set_accumulator(0xFF);
		fixture.cpu->set_carry_flag(true);			// Will be rotated into bit 0
		fixture.bus->write(0x10, 0x80);				// Will become 0x01 after rotate
		fixture.setup_memory(0x0200, {0x27, 0x10}); // RLA zp

		(void)fixture.cpu->execute_instruction();

		// Memory rotated: 0x80 -> 0x01 (carry=1 rotated in, bit 7=1 to carry)
		// A = A AND rotated = 0xFF AND 0x01 = 0x01
		REQUIRE(fixture.bus->read(0x10) == 0x01);
		REQUIRE(fixture.cpu->get_accumulator() == 0x01);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // Bit 7 was set
	}

	SECTION("RLA with carry clear") {
		fixture.cpu->set_accumulator(0x55);
		fixture.cpu->set_carry_flag(false);			// Will be rotated into bit 0
		fixture.bus->write(0x10, 0x2A);				// Will become 0x54 after rotate
		fixture.setup_memory(0x0200, {0x27, 0x10}); // RLA zp

		(void)fixture.cpu->execute_instruction();

		// Memory rotated: 0x2A -> 0x54 (carry=0 rotated in, bit 7=0 to carry)
		// A = A AND rotated = 0x55 AND 0x54 = 0x54
		REQUIRE(fixture.bus->read(0x10) == 0x54);
		REQUIRE(fixture.cpu->get_accumulator() == 0x54);
		REQUIRE(fixture.cpu->get_carry_flag() == false); // Bit 7 was clear
	}
}

TEST_CASE("SRE - Shift Right and EOR", "[cpu][undocumented][sre]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("SRE Zero Page") {
		fixture.cpu->set_accumulator(0xFF);
		fixture.bus->write(0x10, 0x81);				// Will become 0x40 after shift
		fixture.setup_memory(0x0200, {0x47, 0x10}); // SRE zp

		(void)fixture.cpu->execute_instruction();

		// Memory shifted: 0x81 -> 0x40, carry set from bit 0
		// A = A EOR shifted = 0xFF EOR 0x40 = 0xBF
		REQUIRE(fixture.bus->read(0x10) == 0x40);
		REQUIRE(fixture.cpu->get_accumulator() == 0xBF);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // Bit 0 was set
		REQUIRE(fixture.cpu->get_negative_flag() == true);
	}

	SECTION("SRE resulting in zero") {
		fixture.cpu->set_accumulator(0x20);
		fixture.bus->write(0x10, 0x40);				// Will become 0x20 after shift
		fixture.setup_memory(0x0200, {0x47, 0x10}); // SRE zp

		(void)fixture.cpu->execute_instruction();

		// Memory shifted: 0x40 -> 0x20
		// A = A EOR shifted = 0x20 EOR 0x20 = 0x00
		REQUIRE(fixture.bus->read(0x10) == 0x20);
		REQUIRE(fixture.cpu->get_accumulator() == 0x00);
		REQUIRE(fixture.cpu->get_zero_flag() == true);
		REQUIRE(fixture.cpu->get_carry_flag() == false); // Bit 0 was clear
	}
}

TEST_CASE("RRA - Rotate Right and Add with Carry", "[cpu][undocumented][rra]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("RRA Zero Page") {
		fixture.cpu->set_accumulator(0x10);
		fixture.cpu->set_carry_flag(true);			// Will be rotated into bit 7, and used in addition
		fixture.bus->write(0x10, 0x02);				// Will become 0x81 after rotate
		fixture.setup_memory(0x0200, {0x67, 0x10}); // RRA zp

		(void)fixture.cpu->execute_instruction();

		// Memory rotated: 0x02 -> 0x81 (carry=1 rotated into bit 7, bit 0=0 to carry)
		// ADC: A = 0x10 + 0x81 + carry = 0x10 + 0x81 + 0 = 0x91
		REQUIRE(fixture.bus->read(0x10) == 0x81);
		REQUIRE(fixture.cpu->get_accumulator() == 0x91);
		REQUIRE(fixture.cpu->get_carry_flag() == false); // No carry from addition
		REQUIRE(fixture.cpu->get_negative_flag() == true);
	}

	SECTION("RRA with carry from rotation and addition") {
		fixture.cpu->set_accumulator(0xFF);
		fixture.cpu->set_carry_flag(false);			// Will be rotated into bit 7
		fixture.bus->write(0x10, 0x03);				// Will become 0x01 after rotate
		fixture.setup_memory(0x0200, {0x67, 0x10}); // RRA zp

		(void)fixture.cpu->execute_instruction();

		// Memory rotated: 0x03 -> 0x01 (carry=0 rotated into bit 7, bit 0=1 to carry)
		// ADC: A = 0xFF + 0x01 + carry = 0xFF + 0x01 + 1 = 0x101 -> 0x01 (carry set)
		REQUIRE(fixture.bus->read(0x10) == 0x01);
		REQUIRE(fixture.cpu->get_accumulator() == 0x01);
		REQUIRE(fixture.cpu->get_carry_flag() == true); // Carry from addition
		REQUIRE(fixture.cpu->get_zero_flag() == false);
	}
}

TEST_CASE("NOP Variants", "[cpu][undocumented][nop]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("NOP Immediate - 2 cycles") {
		// Save initial state
		Byte initial_a = fixture.cpu->get_accumulator();
		Byte initial_x = fixture.cpu->get_x_register();
		Byte initial_status = fixture.cpu->get_status_register();

		fixture.setup_memory(0x0200, {0x80, 0x42}); // NOP #$42 (0x80 is actual NOP immediate)

		(void)fixture.cpu->execute_instruction();

		// Should do nothing except advance PC
		REQUIRE(fixture.cpu->get_accumulator() == initial_a);
		REQUIRE(fixture.cpu->get_x_register() == initial_x);
		REQUIRE(fixture.cpu->get_status_register() == initial_status);
		REQUIRE(fixture.cpu->get_program_counter() == 0x0202);
	}
}

TEST_CASE("Crash Opcodes", "[cpu][undocumented][crash]") {
	UndocumentedOpcodesTestFixture fixture;

	SECTION("Highly unstable opcode should be handled") {
		// Test one of the crash opcodes (ANE/XAA)
		fixture.setup_memory(0x0200, {0x8B, 0x42}); // 0x8B is ANE/XAA crash opcode

		// The CPU should handle this gracefully - in our implementation it prints and continues
		Address pc_before = fixture.cpu->get_program_counter();
		(void)fixture.cpu->execute_instruction();

		// Should advance PC and not actually crash the test
		REQUIRE(fixture.cpu->get_program_counter() > pc_before);
	}
}
