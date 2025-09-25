// VibeNES - NES Emulator
// System Bus Tests
// Tests for central memory and I/O interconnect

#include "../../include/core/bus.hpp"
#include "../../include/core/types.hpp"
#include "../../include/memory/ram.hpp"
#include "../catch2/catch_amalgamated.hpp"
#include <string>

using namespace nes;

TEST_CASE("Bus Construction", "[bus][core]") {
	SystemBus bus;

	SECTION("Bus should be properly initialized") {
		REQUIRE(std::string(bus.get_name()) == "System Bus");
	}
}

TEST_CASE("Bus Component Management", "[bus][components]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();

	SECTION("Connect RAM component") {
		bus.connect_ram(ram);

		// Verify RAM is connected by testing read/write
		bus.write(0x0000, 0xAA);
		REQUIRE(bus.read(0x0000) == 0xAA);
	}
}

TEST_CASE("Bus Memory Access", "[bus][memory]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();
	bus.connect_ram(ram);

	SECTION("RAM region access (0x0000-0x1FFF)") {
		// Test basic read/write to RAM region
		bus.write(0x0000, 0x11);
		REQUIRE(bus.read(0x0000) == 0x11);

		bus.write(0x07FF, 0x22);
		REQUIRE(bus.read(0x07FF) == 0x22);

		bus.write(0x1000, 0x33);
		REQUIRE(bus.read(0x1000) == 0x33);

		bus.write(0x1FFF, 0x44);
		REQUIRE(bus.read(0x1FFF) == 0x44);
	}

	SECTION("RAM mirroring through bus") {
		// Write to base address
		bus.write(0x0100, 0xAB);

		// Verify mirroring works through bus
		REQUIRE(bus.read(0x0100) == 0xAB);
		REQUIRE(bus.read(0x0900) == 0xAB); // +0x800
		REQUIRE(bus.read(0x1100) == 0xAB); // +0x1000
		REQUIRE(bus.read(0x1900) == 0xAB); // +0x1800

		// Write to mirror address
		bus.write(0x1200, 0xCD);

		// Verify original and other mirrors are updated
		REQUIRE(bus.read(0x0A00) == 0xCD); // Base address
		REQUIRE(bus.read(0x1200) == 0xCD); // Original mirror
		REQUIRE(bus.read(0x1A00) == 0xCD); // Another mirror
	}

	SECTION("Multiple sequential writes and reads") {
		// Fill a range of addresses
		for (Address addr = 0x0000; addr < 0x0100; ++addr) {
			Byte value = static_cast<Byte>(addr & 0xFF);
			bus.write(addr, value);
		}

		// Verify all values
		for (Address addr = 0x0000; addr < 0x0100; ++addr) {
			Byte expected = static_cast<Byte>(addr & 0xFF);
			REQUIRE(bus.read(addr) == expected);
		}
	}
}

TEST_CASE("Bus Open Bus Behavior", "[bus][open-bus]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();
	bus.connect_ram(ram);

	SECTION("Open bus returns last bus value - PPU region") {
		// Write a known value to RAM first to set the bus
		bus.write(0x0100, 0xAB);
		[[maybe_unused]] auto bus_value = bus.read(0x0100); // This sets last_bus_value_ to 0xAB
		(void)bus_value;

		// Now read from unmapped PPU region - should return last bus value
		Byte open_bus_value = bus.read(0x2000);
		REQUIRE(open_bus_value == 0xAB); // Should be last value on bus
	}

	SECTION("Open bus returns last bus value - APU region") {
		// Set a different known value
		bus.write(0x0200, 0xCD);
		[[maybe_unused]] auto bus_value = bus.read(0x0200); // This sets last_bus_value_ to 0xCD
		(void)bus_value;

		// Read from unmapped APU region
		Byte open_bus_value = bus.read(0x4000);
		REQUIRE(open_bus_value == 0xCD); // Should be last value on bus
	}

	SECTION("Open bus returns last bus value - Cartridge region") {
		// Set another known value
		bus.write(0x0300, 0xEF);
		[[maybe_unused]] auto bus_value = bus.read(0x0300); // This sets last_bus_value_ to 0xEF
		(void)bus_value;

		// Read from unmapped cartridge region
		Byte open_bus_value = bus.read(0x4020);
		REQUIRE(open_bus_value == 0xEF); // Should be last value on bus
	}

	SECTION("Writes update bus value for subsequent open bus reads") {
		// Write to unmapped region - this updates last_bus_value_
		bus.write(0x2000, 0x42);

		// Now read from different unmapped region - should return written value
		REQUIRE(bus.read(0x4000) == 0x42);
		REQUIRE(bus.read(0x8000) == 0x42);
	}

	SECTION("Fresh bus has predictable initial state") {
		SystemBus fresh_bus;
		// Before any operations, should return initial value (0xFF)
		REQUIRE(fresh_bus.read(0x2000) == 0xFF);
	}
}

TEST_CASE("Bus Component Interface", "[bus][component]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();
	bus.connect_ram(ram);

	SECTION("Component interface methods") {
		REQUIRE_NOTHROW(bus.tick(CpuCycle{1}));
		REQUIRE_NOTHROW(bus.reset());
		REQUIRE_NOTHROW(bus.power_on());
	}

	SECTION("Reset propagates to connected components") {
		// Write some data
		bus.write(0x0500, 0xAA);
		REQUIRE(bus.read(0x0500) == 0xAA);

		// Reset should preserve RAM contents (real NES hardware behavior)
		bus.reset();
		REQUIRE(bus.read(0x0500) == 0xAA); // RAM contents preserved on reset
	}

	SECTION("Power on propagates to connected components") {
		// Clear the initial random garbage first by writing known values
		bus.write(0x0600, 0xBB);
		REQUIRE(bus.read(0x0600) == 0xBB);

		// Power on should fill RAM with random garbage (real NES hardware behavior)
		bus.power_on();
		// Can't test for specific value since it's random, but should not be the written value
		// Just test that power_on doesn't crash and ram still works
		bus.write(0x0600, 0xCC);
		REQUIRE(bus.read(0x0600) == 0xCC);
	}

	SECTION("Tick propagates to connected components") {
		// This should not crash - tick gets forwarded to RAM
		REQUIRE_NOTHROW(bus.tick(CpuCycle{10}));
		REQUIRE_NOTHROW(bus.tick(CpuCycle{100}));
	}
}

TEST_CASE("Bus Address Decoding", "[bus][addressing]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();
	bus.connect_ram(ram);

	SECTION("Address space boundaries") {
		// Last RAM address
		bus.write(0x1FFF, 0xAA);
		REQUIRE(bus.read(0x1FFF) == 0xAA);

		// After RAM read, open bus should return last read value
		REQUIRE(bus.read(0x2000) == 0xAA); // PPU region returns last bus value
		REQUIRE(bus.read(0x3FFF) == 0xAA); // Still last bus value
		REQUIRE(bus.read(0x4000) == 0xAA); // APU region returns last bus value
	}

	SECTION("Comprehensive address range test") {
		// Test key boundary addresses
		std::vector<std::pair<Address, bool>> test_addresses = {
			{0x0000, true},	 // RAM start
			{0x07FF, true},	 // RAM physical end
			{0x0800, true},	 // RAM mirror start
			{0x1FFF, true},	 // RAM end
			{0x2000, false}, // PPU start
			{0x3FFF, false}, // PPU end
			{0x4000, false}, // APU start
			{0x4017, false}, // APU end
			{0x4020, false}, // Cartridge start
			{0x8000, false}, // PRG ROM start
			{0xFFFF, false}	 // Address space end
		};

		// First establish a known bus value
		bus.write(0x0100, 0x55);
		[[maybe_unused]] auto bus_value = bus.read(0x0100); // Sets last_bus_value_ to 0x55
		(void)bus_value;

		for (const auto &[addr, is_ram] : test_addresses) {
			if (is_ram) {
				bus.write(addr, 0x77); // Use different value to test RAM
				REQUIRE(bus.read(addr) == 0x77);
			} else {
				// Should return last bus value (0x77 from last RAM read)
				REQUIRE(bus.read(addr) == 0x77);
			}
		}
	}
}

TEST_CASE("Bus Performance", "[bus][performance]") {
	SystemBus bus;
	auto ram = std::make_shared<Ram>();
	bus.connect_ram(ram);

	SECTION("Large number of operations") {
		const int num_operations = 10000;

		// Write pattern
		for (int i = 0; i < num_operations; ++i) {
			Address addr = static_cast<Address>(i % 0x2000); // Stay in RAM range
			Byte value = static_cast<Byte>(i & 0xFF);
			bus.write(addr, value);
		}

		// Verify pattern
		for (int i = 0; i < num_operations; ++i) {
			Address addr = static_cast<Address>(i % 0x2000);
			Byte expected = static_cast<Byte>(i & 0xFF);
			REQUIRE(bus.read(addr) == expected);
		}
	}
}
