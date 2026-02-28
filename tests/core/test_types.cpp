// VibeNES - NES Emulator
// Core Types Tests
// Tests for type system, timing types, and utility functions

#include "../../include/core/types.hpp"
#include <catch2/catch_all.hpp>
#include <limits>

using namespace nes;

TEST_CASE("Strong Type System", "[types][core]") {
	SECTION("Address type operations") {
		Address addr1 = 0x1234;
		Address addr2 = 0x5678;

		REQUIRE(addr1 == 0x1234);
		REQUIRE(addr2 == 0x5678);

		// Test comparison operators
		REQUIRE(addr1 != addr2);
		REQUIRE(addr1 == 0x1234);
		REQUIRE(addr1 < addr2);
		REQUIRE(addr2 > addr1);
	}

	SECTION("Byte type operations") {
		Byte byte1 = 0xAA;
		Byte byte2 = 0x55;

		REQUIRE(byte1 == 0xAA);
		REQUIRE(byte2 == 0x55);

		// Test comparison operators
		REQUIRE(byte1 != byte2);
		REQUIRE(byte1 == 0xAA);
		REQUIRE(byte1 > byte2);
		REQUIRE(byte2 < byte1);
	}

	SECTION("Word type operations") {
		Word word1 = 0x1234;
		Word word2 = 0x5678;

		REQUIRE(word1 == 0x1234);
		REQUIRE(word2 == 0x5678);

		// Test comparison operators
		REQUIRE(word1 != word2);
		REQUIRE(word1 == 0x1234);
		REQUIRE(word1 < word2);
		REQUIRE(word2 > word1);
	}
}

TEST_CASE("Timing Types", "[types][timing]") {
	SECTION("CpuCycle operations") {
		CpuCycle cycle1{10};
		CpuCycle cycle2{20};

		REQUIRE(cycle1.count() == 10);
		REQUIRE(cycle2.count() == 20);

		// Arithmetic operations
		auto sum = cycle1 + cycle2;
		REQUIRE(sum.count() == 30);

		auto diff = cycle2 - cycle1;
		REQUIRE(diff.count() == 10);

		// Comparison operations
		REQUIRE(cycle1 < cycle2);
		REQUIRE(cycle2 > cycle1);
		REQUIRE(cycle1 != cycle2);
		REQUIRE(cycle1 == CpuCycle{10});
	}

	SECTION("PpuDot operations") {
		PpuDot dot1{100};
		PpuDot dot2{200};

		REQUIRE(dot1.count() == 100);
		REQUIRE(dot2.count() == 200);

		// Arithmetic operations
		auto sum = dot1 + dot2;
		REQUIRE(sum.count() == 300);

		auto diff = dot2 - dot1;
		REQUIRE(diff.count() == 100);

		// Comparison operations
		REQUIRE(dot1 < dot2);
		REQUIRE(dot2 > dot1);
		REQUIRE(dot1 != dot2);
		REQUIRE(dot1 == PpuDot{100});
	}

	SECTION("Timing constants") {
		REQUIRE(CPU_CLOCK_NTSC > 0);
		REQUIRE(PPU_CLOCK_NTSC > 0);
		REQUIRE(MASTER_CLOCK_NTSC > 0);
	}
}

TEST_CASE("Memory Constants", "[types][memory]") {
	SECTION("RAM constants") {
		REQUIRE(RAM_SIZE == 0x0800); // 2KB
		REQUIRE(RAM_START == 0x0000);
		REQUIRE(RAM_END == 0x07FF); // Physical RAM end
	}

	SECTION("PPU constants") {
		REQUIRE(PPU_REGISTERS_START == 0x2000);
		REQUIRE(PPU_REGISTERS_END == 0x2007);
	}

	SECTION("APU constants") {
		REQUIRE(APU_IO_START == 0x4000);
		REQUIRE(APU_IO_END == 0x4017);
	}

	SECTION("Cartridge constants") {
		REQUIRE(CARTRIDGE_START == 0x4020);
		REQUIRE(CARTRIDGE_END == 0xFFFF);
	}
}

TEST_CASE("Utility Functions", "[types][utils]") {
	SECTION("Mirror RAM address function") {
		// Test RAM mirroring
		REQUIRE(mirror_ram_address(0x0000) == 0x0000);
		REQUIRE(mirror_ram_address(0x0800) == 0x0000);
		REQUIRE(mirror_ram_address(0x1000) == 0x0000);
		REQUIRE(mirror_ram_address(0x1800) == 0x0000);

		REQUIRE(mirror_ram_address(0x01FF) == 0x01FF);
		REQUIRE(mirror_ram_address(0x09FF) == 0x01FF);
		REQUIRE(mirror_ram_address(0x11FF) == 0x01FF);
		REQUIRE(mirror_ram_address(0x19FF) == 0x01FF);

		REQUIRE(mirror_ram_address(0x07FF) == 0x07FF);
		REQUIRE(mirror_ram_address(0x0FFF) == 0x07FF);
		REQUIRE(mirror_ram_address(0x17FF) == 0x07FF);
		REQUIRE(mirror_ram_address(0x1FFF) == 0x07FF);
	}

	SECTION("Address range checking") {
		// Test address ranges
		REQUIRE(0x0000 >= RAM_START);
		REQUIRE(0x07FF <= RAM_END); // Physical RAM end

		REQUIRE(0x2000 >= PPU_REGISTERS_START);
		REQUIRE(0x2007 <= PPU_REGISTERS_END);

		REQUIRE(0x4000 >= APU_IO_START);
		REQUIRE(0x4017 <= APU_IO_END);

		REQUIRE(0x4020 >= CARTRIDGE_START);
		REQUIRE(0xFFFF <= CARTRIDGE_END);
	}
}

TEST_CASE("Basic Type Functionality", "[types][basic]") {
	SECTION("Type size verification") {
		REQUIRE(sizeof(Address) == 2); // 16-bit
		REQUIRE(sizeof(Byte) == 1);	   // 8-bit
		REQUIRE(sizeof(Word) == 2);	   // 16-bit
	}

	SECTION("Type limits") {
		REQUIRE(std::numeric_limits<Address>::max() == 0xFFFF);
		REQUIRE(std::numeric_limits<Byte>::max() == 0xFF);
		REQUIRE(std::numeric_limits<Word>::max() == 0xFFFF);
	}
}
