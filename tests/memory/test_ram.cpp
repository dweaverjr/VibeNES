// VibeNES - NES Emulator
// RAM Component Tests
// Tests for 2KB work RAM with address mirroring

#include "../../include/core/types.hpp"
#include "../../include/memory/ram.hpp"
#include <catch2/catch_all.hpp>

using namespace nes;

TEST_CASE("RAM Construction", "[ram][memory]") {
	Ram ram;

	SECTION("RAM should be properly initialized") {
		REQUIRE(std::string(ram.get_name()) == "Work RAM");

		// All memory should be zero after construction
		for (Address addr = 0x0000; addr <= 0x1FFF; addr += 0x100) {
			REQUIRE(ram.read(addr) == 0x00);
		}
	}
}

TEST_CASE("RAM Basic Read/Write", "[ram][memory]") {
	Ram ram;

	SECTION("Basic write and read operations") {
		ram.write(0x0000, 0xAA);
		REQUIRE(ram.read(0x0000) == 0xAA);

		ram.write(0x07FF, 0x55);
		REQUIRE(ram.read(0x07FF) == 0x55);

		ram.write(0x0400, 0xFF);
		REQUIRE(ram.read(0x0400) == 0xFF);
	}

	SECTION("Multiple writes to same address") {
		ram.write(0x0200, 0x11);
		REQUIRE(ram.read(0x0200) == 0x11);

		ram.write(0x0200, 0x22);
		REQUIRE(ram.read(0x0200) == 0x22);

		ram.write(0x0200, 0x33);
		REQUIRE(ram.read(0x0200) == 0x33);
	}
}

TEST_CASE("RAM Address Mirroring", "[ram][memory][mirroring]") {
	Ram ram;

	SECTION("Address $0000 mirrors to $0800, $1000, $1800") {
		ram.write(0x0000, 0xAB);

		REQUIRE(ram.read(0x0000) == 0xAB);
		REQUIRE(ram.read(0x0800) == 0xAB);
		REQUIRE(ram.read(0x1000) == 0xAB);
		REQUIRE(ram.read(0x1800) == 0xAB);
	}

	SECTION("Address $07FF mirrors to $0FFF, $17FF, $1FFF") {
		ram.write(0x07FF, 0xCD);

		REQUIRE(ram.read(0x07FF) == 0xCD);
		REQUIRE(ram.read(0x0FFF) == 0xCD);
		REQUIRE(ram.read(0x17FF) == 0xCD);
		REQUIRE(ram.read(0x1FFF) == 0xCD);
	}

	SECTION("Write to mirror address affects base address") {
		ram.write(0x1234, 0xEF);

		// Calculate mirrored addresses
		[[maybe_unused]] Address mirror1 = 0x1234 - 0x0800; // Should be 0x0A34
		[[maybe_unused]] Address mirror2 = 0x1234 + 0x0800; // Should be 0x1A34

		REQUIRE(ram.read(0x0A34) == 0xEF);
		REQUIRE(ram.read(0x1234) == 0xEF);
		REQUIRE(ram.read(0x1A34) == 0xEF);
	}

	SECTION("Comprehensive mirroring test") {
		// Test every 256th address to verify mirroring
		for (Address base_addr = 0x0000; base_addr < 0x0800; base_addr += 0x100) {
			Byte test_value = static_cast<Byte>(base_addr & 0xFF);

			ram.write(base_addr, test_value);

			// Check all mirror locations
			REQUIRE(ram.read(base_addr) == test_value);
			REQUIRE(ram.read(base_addr + 0x0800) == test_value);
			REQUIRE(ram.read(base_addr + 0x1000) == test_value);
			REQUIRE(ram.read(base_addr + 0x1800) == test_value);
		}
	}
}

TEST_CASE("RAM Component Interface", "[ram][component]") {
	Ram ram;

	SECTION("Component interface methods") {
		REQUIRE_NOTHROW(ram.tick(CpuCycle{1}));
		REQUIRE_NOTHROW(ram.reset());
		REQUIRE_NOTHROW(ram.power_on());
	}

	SECTION("Reset preserves memory contents") {
		// Fill some memory
		ram.write(0x0100, 0xAA);
		ram.write(0x0500, 0xBB);
		ram.write(0x0700, 0xCC);

		// Verify data is there
		REQUIRE(ram.read(0x0100) == 0xAA);
		REQUIRE(ram.read(0x0500) == 0xBB);
		REQUIRE(ram.read(0x0700) == 0xCC);

		// Reset should preserve memory (hardware-accurate behavior)
		ram.reset();

		REQUIRE(ram.read(0x0100) == 0xAA);
		REQUIRE(ram.read(0x0500) == 0xBB);
		REQUIRE(ram.read(0x0700) == 0xCC);
	}

	SECTION("Power on fills memory with random garbage") {
		// Start with a clean state
		ram.power_on();

		// Read initial values (should be random garbage)
		[[maybe_unused]] Byte initial_1 = ram.read(0x0200);
		[[maybe_unused]] Byte initial_2 = ram.read(0x0600);

		// Write known values
		ram.write(0x0200, 0xDD);
		ram.write(0x0600, 0xEE);

		// Verify data is there
		REQUIRE(ram.read(0x0200) == 0xDD);
		REQUIRE(ram.read(0x0600) == 0xEE);

		// Power on should fill with new random garbage
		ram.power_on();

		// Memory should be different from the written values
		// (Very unlikely to randomly get the same values we wrote)
		Byte after_1 = ram.read(0x0200);
		Byte after_2 = ram.read(0x0600);

		// Test that power_on changed the memory (hardware-accurate behavior)
		// Note: In extremely rare cases this might fail due to random chance,
		// but the deterministic RNG should make this test reliable
		bool memory_changed = (after_1 != 0xDD) || (after_2 != 0xEE);
		REQUIRE(memory_changed);
	}
}

TEST_CASE("RAM Edge Cases", "[ram][memory][edge-cases]") {
	Ram ram;

	SECTION("Maximum address range") {
		// Test highest valid address
		ram.write(0x1FFF, 0x99);
		REQUIRE(ram.read(0x1FFF) == 0x99);

		// Should mirror to base address 0x07FF
		REQUIRE(ram.read(0x07FF) == 0x99);
	}

	SECTION("All byte values") {
		Address test_addr = 0x0300;

		// Test all possible byte values
		for (int value = 0; value <= 255; ++value) {
			Byte byte_value = static_cast<Byte>(value);
			ram.write(test_addr, byte_value);
			REQUIRE(ram.read(test_addr) == byte_value);
		}
	}
}
